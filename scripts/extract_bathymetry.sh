#!/usr/bin/env bash
# ==============================================================================
# extract_bathymetry.sh
#
# Automated workflow for extracting 1D bathymetry profiles from GEBCO data
# using Generic Mapping Tools (GMT).
#
# Steps:
#   1. Download GEBCO grid (if not already present)
#   2. Cut region from global grid
#   3. Extract 1D profile via grdtrack
#   4. Convert to clean CSV
#   5. (optional) Generate map visualization (.ps / .pdf)
#
# Re-runs always recompute steps 2–5. The GEBCO download/unzip is
# skipped only when the .nc file already exists on disk.
#
# Usage examples:
#   # Minimal: just extract the 1D profile CSV
#   ./extract_bathymetry.sh
#
#   # With map generation
#   ./extract_bathymetry.sh --map
#
#   # With map + PDF conversion
#   ./extract_bathymetry.sh --map --pdf
#
#   # Custom region and profile
#   ./extract_bathymetry.sh \
#     --region 138/147/35/39 \
#     --profile-start 141.024949/37.316569 \
#     --profile-end 146.0/37.316569 \
#     --sampling 250 \
#     --map --pdf
#
# Authors: Brückner, Köllmann, Vogt
# Project: Tsunami Lab — Friedrich Schiller University Jena
# ==============================================================================
set -euo pipefail

# ------------------------------------------------------------------------------
# Resolve paths
# ------------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." 2>/dev/null && pwd || echo "${SCRIPT_DIR}")"

# ------------------------------------------------------------------------------
# Defaults
# ------------------------------------------------------------------------------
GEBCO_URL="https://dap.ceda.ac.uk/bodc/gebco/global/gebco_2025/ice_surface_elevation/netcdf/gebco_2025.zip?download=1"
GEBCO_ZIP_NAME="gebco_2025.zip"
GEBCO_NC_GLOB="*.nc"                 # auto-detect .nc filename after unzip

DATA_DIR="${PROJECT_DIR}/data"
TMP_DIR="${DATA_DIR}/tmp"
RESSOURCES_DIR="${PROJECT_DIR}/ressources"
OUTPUT_DIR="${PROJECT_DIR}/output"

REGION_CUT="138/147/35/39"

PROFILE_START="141.024949/37.316569"  # lon/lat of p1
PROFILE_END="146.0/37.316569"         # lon/lat of p2
SAMPLING_M=250                        # metres between samples

GENERATE_MAP=false
GENERATE_PDF=false

# Points-of-interest file for map overlay.
# Format: whitespace-separated, one point per line:
#   lon  lat  label
# Example file (store in ressources/):
#   139.6917  35.6895  Tokyo
#   142.3700  38.3000  Sendai
# Leave empty to skip.
POI_CSV=""

# Map projection & styling
MAP_PROJ="JM20c"
MAP_ANNOTATION="-B1"
MAP_CPT="etopo1"
MAP_DPI=500

# ------------------------------------------------------------------------------
# Parse CLI arguments
# ------------------------------------------------------------------------------
print_usage() {
  cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Options:
  --region R/L/B/T        Region to cut (default: ${REGION_CUT})
  --profile-start LON/LAT Start point of 1D profile (default: ${PROFILE_START})
  --profile-end LON/LAT   End point of 1D profile (default: ${PROFILE_END})
  --sampling METRES       Sampling distance in metres (default: ${SAMPLING_M})
  --gebco-url URL         URL to download GEBCO zip (default: current GEBCO URL)
  --gebco-zip NAME        Filename of the GEBCO zip (default: ${GEBCO_ZIP_NAME})
  --data-dir PATH         Base data directory (default: ${DATA_DIR})
  --output-dir PATH       Output directory (default: ${OUTPUT_DIR})
  --poi-csv PATH          CSV with points of interest (lon,lat,label)
  --map                   Generate map visualization (.ps)
  --pdf                   Also convert .ps to .pdf (implies --map)
  -h, --help              Show this help
EOF
  exit 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --region)         REGION_CUT="$2";      shift 2 ;;
    --profile-start)  PROFILE_START="$2";   shift 2 ;;
    --profile-end)    PROFILE_END="$2";     shift 2 ;;
    --sampling)       SAMPLING_M="$2";      shift 2 ;;
    --gebco-url)      GEBCO_URL="$2";       shift 2 ;;
    --gebco-zip)      GEBCO_ZIP_NAME="$2";  shift 2 ;;
    --data-dir)       DATA_DIR="$2"; TMP_DIR="${DATA_DIR}/tmp"; shift 2 ;;
    --output-dir)     OUTPUT_DIR="$2";      shift 2 ;;
    --poi-csv)        POI_CSV="$2";         shift 2 ;;
    --map)            GENERATE_MAP=true;    shift ;;
    --pdf)            GENERATE_PDF=true; GENERATE_MAP=true; shift ;;
    -h|--help)        print_usage ;;
    *)
      echo "Error: Unknown option '$1'"
      print_usage
      ;;
  esac
done

# ------------------------------------------------------------------------------
# Derived paths
# ------------------------------------------------------------------------------
GEBCO_ZIP="${DATA_DIR}/${GEBCO_ZIP_NAME}"
CUT_NC="${TMP_DIR}/GEBCO_cut.nc"
PROFILE_RAW="${TMP_DIR}/profile_raw.csv"
PROFILE_CSV="${RESSOURCES_DIR}/bathymetry_profile.csv"
MAP_PS="${OUTPUT_DIR}/bathymetry_map.ps"
MAP_PDF="${OUTPUT_DIR}/bathymetry_map.pdf"
CPT_FILE="${TMP_DIR}/topo.cpt"

# Build grdtrack -E argument:  lon1/lat1/lon2/lat2+<sampling>e+d
#   +<N>e  = sample every N metres along great circle
#   +d     = append distance column
GRDTRACK_PROFILE="${PROFILE_START}/${PROFILE_END}+${SAMPLING_M}e+d"

# ------------------------------------------------------------------------------
# Helpers
# ------------------------------------------------------------------------------
info()  { echo -e "\033[1;34m[INFO]\033[0m  $*"; }
warn()  { echo -e "\033[1;33m[WARN]\033[0m  $*"; }
error() { echo -e "\033[1;31m[ERROR]\033[0m $*" >&2; exit 1; }

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || error "'$1' is not installed or not in PATH."
}

# ------------------------------------------------------------------------------
# Pre-flight checks
# ------------------------------------------------------------------------------
require_cmd gmt
require_cmd unzip

mkdir -p "${DATA_DIR}" "${TMP_DIR}" "${RESSOURCES_DIR}" "${OUTPUT_DIR}"

# ------------------------------------------------------------------------------
# Step 1: Download GEBCO grid
# ------------------------------------------------------------------------------
find_gebco_nc() {
  # Find the .nc file — either directly in DATA_DIR or one level deeper
  find "${DATA_DIR}" -maxdepth 2 -name "${GEBCO_NC_GLOB}" -print -quit 2>/dev/null
}

GEBCO_NC="$(find_gebco_nc)"

if [[ -n "${GEBCO_NC}" ]]; then
  info "GEBCO NetCDF already present: ${GEBCO_NC}"
else
  if [[ -f "${GEBCO_ZIP}" ]]; then
    info "GEBCO zip already downloaded: ${GEBCO_ZIP}"
  else
    info "Downloading GEBCO grid from ${GEBCO_URL} ..."
    info "(This is a large file — ~8 GB. Be patient.)"
    wget --no-verbose --show-progress -O "${GEBCO_ZIP}" "${GEBCO_URL}" \
      || error "Download failed. You can manually place the zip at: ${GEBCO_ZIP}"
  fi

  info "Unzipping ${GEBCO_ZIP} ..."
  unzip -o -d "${DATA_DIR}" "${GEBCO_ZIP}"

  GEBCO_NC="$(find_gebco_nc)"
  [[ -n "${GEBCO_NC}" ]] || error "Could not find .nc file after unzipping. Check ${DATA_DIR}."
  info "Found NetCDF: ${GEBCO_NC}"
fi

# ------------------------------------------------------------------------------
# Step 2: Cut region
# ------------------------------------------------------------------------------
info "Cutting region ${REGION_CUT} from ${GEBCO_NC} ..."
gmt grdcut "${GEBCO_NC}" -R"${REGION_CUT}" -G"${CUT_NC}"
info "Cut grid info:"
gmt grdinfo "${CUT_NC}"

# ------------------------------------------------------------------------------
# Step 3 & 4: Extract 1D profile and convert to CSV
# ------------------------------------------------------------------------------
info "Extracting 1D profile: ${PROFILE_START} -> ${PROFILE_END} (${SAMPLING_M}m sampling) ..."
# grdtrack has issues with spaces in -G paths — use relative path via subshell
( cd "${TMP_DIR}" && gmt grdtrack -G"GEBCO_cut.nc" -E"${GRDTRACK_PROFILE}" -Ar ) > "${PROFILE_RAW}"

# Convert whitespace-delimited GMT output to clean CSV with header
info "Converting to CSV: ${PROFILE_CSV}"
echo "longitude,latitude,distance,bathymetry" > "${PROFILE_CSV}"
# Skip comment lines, collapse whitespace to commas
grep -v '^[>#]' "${PROFILE_RAW}" | tr -s '[:blank:]' ',' >> "${PROFILE_CSV}"

NSAMPLES=$(( $(wc -l < "${PROFILE_CSV}") - 1 ))
info "Profile extracted: ${NSAMPLES} samples written to ${PROFILE_CSV}"

# ------------------------------------------------------------------------------
# Step 5 (optional): Generate map
# ------------------------------------------------------------------------------
if [[ "${GENERATE_MAP}" == true ]]; then
  info "Generating map visualization ..."

  # Color palette
  gmt makecpt -C"${MAP_CPT}" -T-8000/8000/100 > "${CPT_FILE}"

  # Base image
  gmt grdimage "${CUT_NC}" -"${MAP_PROJ}" -R"${REGION_CUT}" \
    "${MAP_ANNOTATION}" -E"${MAP_DPI}" -C"${CPT_FILE}" -K > "${MAP_PS}"

  # Coastlines and national borders
  gmt pscoast -"${MAP_PROJ}" -R"${REGION_CUT}" \
    -Dfull -W0.5p -N1/0.5p,gray50 -O -K >> "${MAP_PS}"

  # Overlay profile line
  # Create a two-line file with start and end point for psxy
  PROFILE_LINE_TMP="${TMP_DIR}/profile_line.txt"
  echo "${PROFILE_START}" | tr '/' ' ' >  "${PROFILE_LINE_TMP}"
  echo "${PROFILE_END}"   | tr '/' ' ' >> "${PROFILE_LINE_TMP}"
  gmt psxy "${PROFILE_LINE_TMP}" -"${MAP_PROJ}" -R"${REGION_CUT}" \
    -W1.5p,red -O -K >> "${MAP_PS}"

  # Profile endpoints as triangles
  gmt psxy "${PROFILE_LINE_TMP}" -"${MAP_PROJ}" -R"${REGION_CUT}" \
    -Sa0.4 -Gred -O -K >> "${MAP_PS}"

  # Optional: Points of interest overlay
  if [[ -n "${POI_CSV}" ]] && [[ -f "${POI_CSV}" ]]; then
    info "Overlaying points of interest from ${POI_CSV} ..."
    gmt psxy "${POI_CSV}" -"${MAP_PROJ}" -R"${REGION_CUT}" \
      -Sa0.3 -Gblack -O -K >> "${MAP_PS}"
    gmt pstext "${POI_CSV}" -"${MAP_PROJ}" -R"${REGION_CUT}" \
      -D0.2c/0.2c -F+jBL+f8p -O -K >> "${MAP_PS}"
  fi

  # Color bar
  gmt psscale -C"${CPT_FILE}" -D10c/-1.5c/15c/0.5ch -B2000+l"Elevation [m]" \
    -O >> "${MAP_PS}"

  info "Map written to: ${MAP_PS}"

  # Optional PDF conversion
  if [[ "${GENERATE_PDF}" == true ]]; then
    if command -v ps2pdf >/dev/null 2>&1; then
      info "Converting to PDF ..."
      ps2pdf "${MAP_PS}" "${MAP_PDF}"
      info "PDF written to: ${MAP_PDF}"
    else
      warn "ps2pdf not found — skipping PDF conversion."
      warn "Install ghostscript: sudo apt install ghostscript"
    fi
  fi
fi

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------
echo ""
info "========================================="
info "  Done!"
info "========================================="
info "Profile CSV : ${PROFILE_CSV}"
[[ "${GENERATE_MAP}" == true ]]  && info "Map (.ps)   : ${MAP_PS}"
[[ "${GENERATE_PDF}" == true ]]  && info "Map (.pdf)  : ${MAP_PDF}"
echo ""