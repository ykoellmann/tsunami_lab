#!/usr/bin/env bash
# ==============================================================================
# plot_bathy.sh — 3D Bathymetry plot using GMT
# Usage: ./plot_bathy.sh <file.nc> [-o <name>] [-h]
#
# Options:
#   -o <name>      Output filename without extension  (default: <nc-name>_3d)
#   -h             Show this help
# ==============================================================================

set -euo pipefail

# Settings
FORMAT="png"
AZIMUT=215
ELEV=30
MAP_WIDTH_CM=14
JZHEIGHT="5c"
CPT_NAME="geo"
OUTPUT=""

# Help
usage() {
  sed -n '2,9p' "$0" | sed 's/^# \?//'
  exit 0
}

# Parse arguments
[[ $# -eq 0 ]] && { echo "Error: No NC file specified."; usage; }

NCFILE="$1"
shift

while [[ $# -gt 0 ]]; do
  case "$1" in
    -o) OUTPUT="$2"; shift 2 ;;
    -h) usage ;;
    *)  echo "Unknown option: $1"; usage ;;
  esac
done

# Validate input
[[ ! -f "$NCFILE" ]] && { echo "Error: File '$NCFILE' not found."; exit 1; }
command -v gmt &>/dev/null || { echo "Error: GMT not found in PATH."; exit 1; }

# Output name
BASENAME=$(basename "$NCFILE" .nc)
OUTPUT="${OUTPUT:-${BASENAME}_3d}"

WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

CPT="$WORKDIR/bathy.cpt"
SHD="$WORKDIR/shading.nc"

echo "> File:        $NCFILE"
echo "> Output:      ${OUTPUT}.${FORMAT}"
echo "> Perspective: Az=${AZIMUT}° El=${ELEV}°"

# Read grid info
GINFO=$(gmt grdinfo "$NCFILE" -C)
XMIN=$(echo "$GINFO" | awk '{print $2}')
XMAX=$(echo "$GINFO" | awk '{print $3}')
YMIN=$(echo "$GINFO" | awk '{print $4}')
YMAX=$(echo "$GINFO" | awk '{print $5}')

# Detect cartesian grid: coordinates in meters if |x| > 360
IS_CARTESIAN=false
if (( $(echo "$XMAX > 360" | bc -l) )) || (( $(echo "$XMIN < -360" | bc -l) )); then
  IS_CARTESIAN=true
fi

echo "> Projection:  $([ "$IS_CARTESIAN" = true ] && echo 'Cartesian (meters)' || echo 'Geographic (degrees)')"

# Z range
ZMIN=$(echo "$GINFO" | awk '{printf "%.4f", $6}')
ZMAX=$(echo "$GINFO" | awk '{printf "%.4f", $7}')

echo "> Z range:     ${ZMIN} to ${ZMAX} m"

# Contour interval
ZRANGE=$(awk -v zmin="$ZMIN" -v zmax="$ZMAX" 'BEGIN { printf "%.4f", zmax - zmin }')
ZSTEP=$(awk -v r="$ZRANGE" 'BEGIN {
  if      (r > 8000) print 500
  else if (r > 4000) print 250
  else if (r > 1000) print 100
  else if (r > 100)  print 10
  else if (r > 10)   print 1
  else               printf "%.4f", r / 10
}')

# Projection
if [ "$IS_CARTESIAN" = true ]; then
  SCALE=$(awk -v w="$MAP_WIDTH_CM" -v xmin="$XMIN" -v xmax="$XMAX" \
    'BEGIN { printf "%.10f", w / (xmax - xmin) }')
  PROJ="-Jx${SCALE}c"
  echo "> Scale:       ${SCALE} cm/m"
else
  PROJ="-JM${MAP_WIDTH_CM}c"
fi

# Color palette
gmt makecpt \
  -C"$CPT_NAME" \
  -T"${ZMIN}/${ZMAX}/${ZSTEP}" \
  -Z \
  > "$CPT"

# Hillshading
gmt grdgradient "$NCFILE" \
  -A270/45 \
  -G"$SHD" \
  -Nt0.7

# Plot
CONTOUR_INT=$(awk -v s="$ZSTEP" 'BEGIN { printf "%.4f", s * 4 }')
CONTOUR_ANN=$(awk -v s="$ZSTEP" 'BEGIN { printf "%.4f", s * 8 }')

gmt begin "$OUTPUT" "$FORMAT"

  gmt grdview "$NCFILE" \
    -C"$CPT" \
    -I"$SHD" \
    -R"${XMIN}/${XMAX}/${YMIN}/${YMAX}" \
    $PROJ \
    -JZ"$JZHEIGHT" \
    -p"${AZIMUT}/${ELEV}" \
    -Qs300 \
    -Bxyzg0

  gmt grdcontour "$NCFILE" \
    -C"$CONTOUR_INT" \
    -A"${CONTOUR_ANN}+f7p,white" \
    -W0.3p,white@60 \
    -R"${XMIN}/${XMAX}/${YMIN}/${YMAX}" \
    $PROJ \
    -p"${AZIMUT}/${ELEV}"

  gmt colorbar \
    -C"$CPT" \
    -DJBC+w10c/0.4c+o0c/1c+h \
    -Bxa"${CONTOUR_INT}"+l"Depth / Elevation (m)"

gmt end show

echo "> Done: ${OUTPUT}.${FORMAT}"
