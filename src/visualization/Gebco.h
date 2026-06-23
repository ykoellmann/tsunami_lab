#ifndef TSUNAMI_LAB_VISUALIZATION_GEBCO_H
#define TSUNAMI_LAB_VISUALIZATION_GEBCO_H

#include "BBox.h"
#include <string>
#include <vector>

namespace tsunami_lab {
namespace visualization {
namespace gebco {

// A native-resolution (optionally strided) extract of the GEBCO grid.
struct Region {
  int w = 0; // number of samples along longitude (west → east)
  int h = 0; // number of samples along latitude  (south → north)
  // Row-major: elev[row * w + col], row indexes latitude (south → north),
  // col indexes longitude (west → east). Values are elevation in metres
  // (negative = below sea level / water depth).
  std::vector<float> elev;
  // Actual geographic bounds that were covered (may differ slightly from the
  // requested box because they snap to grid lines).
  double lonMin = 0, lonMax = 0, latMin = 0, latMax = 0;
};

// Return a local path to the GEBCO grid, downloading it on first use if the
// file is missing. Blocks (and prints progress to the terminal) while the
// one-time download/unzip runs. Returns an empty string on failure.
std::string ensureAvailable();

// Read the bathymetry for i_bbox at native resolution. If either dimension
// would exceed i_maxDim samples, the read is strided down to stay at/below it
// (keeps the GPU mesh manageable for very large selections). Returns false on
// any NetCDF error.
bool readRegion(const std::string& i_path,
                const BBox& i_bbox,
                Region& o_region,
                int i_maxDim = 4000);

} // namespace gebco
} // namespace visualization
} // namespace tsunami_lab

#endif
