#include "Gebco.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <netcdf.h>

namespace tsunami_lab {
namespace visualization {
namespace gebco {

// GEBCO_2026 ice-surface global grid (15 arc-second), hosted by BODC/CEDA.
// The .zip (~4.3 GB) expands to GEBCO_2026.nc (~7.5 GB).
static const char* k_url =
    "https://dap.ceda.ac.uk/bodc/gebco/global/gebco_2026/"
    "ice_surface_elevation/netcdf/GEBCO_2026.zip?download=1";
static const char* k_nc = "data/GEBCO_2026.nc";
static const char* k_zip = "data/GEBCO_2026.zip";

// True if the file opens and contains an 'elevation' variable.
static bool isValidGrid(const char* i_path) {
  int l_ncid = -1;
  if (nc_open(i_path, NC_NOWRITE, &l_ncid) != NC_NOERR)
    return false;
  int l_vid = -1;
  bool l_ok = (nc_inq_varid(l_ncid, "elevation", &l_vid) == NC_NOERR);
  nc_close(l_ncid);
  return l_ok;
}

std::string ensureAvailable() {
  // Already present and usable? (The version-stamped filename doubles as the
  // freshness check — bump it to GEBCO_2027 etc. to force a re-download.)
  if (isValidGrid(k_nc))
    return k_nc;

  std::fprintf(stderr,
               "\n[GEBCO] Grid '%s' nicht gefunden.\n"
               "[GEBCO] Einmaliger Download (~4.3 GB Zip → ~7.5 GB NetCDF) …\n"
               "[GEBCO] Quelle: %s\n\n",
               k_nc, k_url);

  if (std::system("mkdir -p data") != 0) {
    std::fprintf(stderr, "[GEBCO] Konnte 'data/' nicht anlegen.\n");
    return "";
  }

  // -c resumes a partially downloaded file if the run was interrupted.
  std::string l_dl = std::string("wget -c -O '") + k_zip + "' '" + k_url + "'";
  if (std::system(l_dl.c_str()) != 0) {
    std::fprintf(stderr, "[GEBCO] Download fehlgeschlagen (wget).\n");
    return "";
  }

  std::fprintf(stderr, "\n[GEBCO] Entpacke %s …\n", k_zip);
  std::string l_uz = std::string("unzip -o '") + k_zip + "' -d data";
  if (std::system(l_uz.c_str()) != 0) {
    std::fprintf(stderr, "[GEBCO] Entpacken fehlgeschlagen (unzip).\n");
    return "";
  }

  if (!isValidGrid(k_nc)) {
    std::fprintf(stderr,
                 "[GEBCO] '%s' nach dem Entpacken nicht vorhanden/lesbar.\n"
                 "[GEBCO] Bitte den Inhalt von data/ prüfen.\n",
                 k_nc);
    return "";
  }

  // Reclaim the ~4 GB zip; the .nc is all we need from here on.
  std::remove(k_zip);
  std::fprintf(stderr, "[GEBCO] Bereit: %s\n\n", k_nc);
  return k_nc;
}

bool readRegion(const std::string& i_path,
                const BBox& i_bbox,
                Region& o_region,
                int i_maxDim) {
  int l_ncid = -1;
  if (nc_open(i_path.c_str(), NC_NOWRITE, &l_ncid) != NC_NOERR)
    return false;

  // Dimensions + coordinate / data variables.
  int l_latDim = -1, l_lonDim = -1;
  int l_latVar = -1, l_lonVar = -1, l_elevVar = -1;
  size_t l_nLat = 0, l_nLon = 0;
  if (nc_inq_dimid(l_ncid, "lat", &l_latDim) != NC_NOERR ||
      nc_inq_dimid(l_ncid, "lon", &l_lonDim) != NC_NOERR ||
      nc_inq_dimlen(l_ncid, l_latDim, &l_nLat) != NC_NOERR ||
      nc_inq_dimlen(l_ncid, l_lonDim, &l_nLon) != NC_NOERR ||
      nc_inq_varid(l_ncid, "lat", &l_latVar) != NC_NOERR ||
      nc_inq_varid(l_ncid, "lon", &l_lonVar) != NC_NOERR ||
      nc_inq_varid(l_ncid, "elevation", &l_elevVar) != NC_NOERR || l_nLat < 2 ||
      l_nLon < 2) {
    nc_close(l_ncid);
    return false;
  }

  // Derive the grid spacing from the first and last coordinate samples; GEBCO
  // is a regular grid, so two endpoints fully define it.
  auto readCoord = [&](int i_var, size_t i_idx) -> double {
    double l_v = 0;
    nc_get_var1_double(l_ncid, i_var, &i_idx, &l_v);
    return l_v;
  };
  double l_latS = readCoord(l_latVar, 0);
  double l_latE = readCoord(l_latVar, l_nLat - 1);
  double l_lonS = readCoord(l_lonVar, 0);
  double l_lonE = readCoord(l_lonVar, l_nLon - 1);
  double l_dLat = (l_latE - l_latS) / (double)(l_nLat - 1);
  double l_dLon = (l_lonE - l_lonS) / (double)(l_nLon - 1);

  // Map a geographic coordinate to the nearest grid index, clamped in range.
  auto toIdx = [](double i_v, double i_start, double i_step,
                  size_t i_n) -> long {
    long l_i = (long)std::floor((i_v - i_start) / i_step + 0.5);
    if (l_i < 0)
      l_i = 0;
    if (l_i >= (long)i_n)
      l_i = (long)i_n - 1;
    return l_i;
  };

  long l_la = toIdx(i_bbox.latMin, l_latS, l_dLat, l_nLat);
  long l_lb = toIdx(i_bbox.latMax, l_latS, l_dLat, l_nLat);
  if (l_la > l_lb)
    std::swap(l_la, l_lb);
  long l_oa = toIdx(i_bbox.lonMin, l_lonS, l_dLon, l_nLon);
  long l_ob = toIdx(i_bbox.lonMax, l_lonS, l_dLon, l_nLon);
  if (l_oa > l_ob)
    std::swap(l_oa, l_ob);

  long l_cntLat = l_lb - l_la + 1;
  long l_cntLon = l_ob - l_oa + 1;

  // Stride down so neither dimension exceeds i_maxDim samples.
  long l_stride = 1;
  long l_big = std::max(l_cntLat, l_cntLon);
  if (i_maxDim > 0 && l_big > i_maxDim)
    l_stride = (l_big + i_maxDim - 1) / i_maxDim;

  size_t l_outH = (size_t)((l_cntLat - 1) / l_stride + 1);
  size_t l_outW = (size_t)((l_cntLon - 1) / l_stride + 1);

  o_region.w = (int)l_outW;
  o_region.h = (int)l_outH;
  o_region.elev.assign(l_outW * l_outH, 0.0f);

  size_t l_start[2] = {(size_t)l_la, (size_t)l_oa};
  size_t l_count[2] = {l_outH, l_outW};
  ptrdiff_t l_strd[2] = {l_stride, l_stride};

  // nc_get_vars_float converts from whatever type GEBCO stores (short/float)
  // to float for us, so we don't need to special-case the storage type.
  int l_rc = nc_get_vars_float(l_ncid, l_elevVar, l_start, l_count, l_strd,
                               o_region.elev.data());

  o_region.latMin = l_latS + (double)l_la * l_dLat;
  o_region.latMax =
      l_latS + (double)(l_la + (long)(l_outH - 1) * l_stride) * l_dLat;
  o_region.lonMin = l_lonS + (double)l_oa * l_dLon;
  o_region.lonMax =
      l_lonS + (double)(l_oa + (long)(l_outW - 1) * l_stride) * l_dLon;

  nc_close(l_ncid);

  if (l_stride > 1)
    std::fprintf(stderr,
                 "[GEBCO] Auswahl %ld×%ld Zellen → auf %d×%d unterabgetastet "
                 "(Stride %ld, Limit %d).\n",
                 l_cntLon, l_cntLat, o_region.w, o_region.h, l_stride,
                 i_maxDim);
  else
    std::fprintf(stderr, "[GEBCO] Auswahl nativ geladen: %d×%d Zellen.\n",
                 o_region.w, o_region.h);

  return l_rc == NC_NOERR;
}

} // namespace gebco
} // namespace visualization
} // namespace tsunami_lab
