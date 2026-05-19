/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Cross-check TsunamiEvent2d against ArtificialTsunami2d using the linked
 * artificial-tsunami input data (1km x 1km displacement, 10km x 10km flat
 * bathymetry at -100 m). Both setups must produce the same h / hu / hv / b
 * for points sampled at the input grid's cell centers.
 *
 * The test is auto-skipped when the input data files are not present
 * (Catch2 SUCCEED), so the suite stays green for users who haven't
 * downloaded the resources.
 **/
#include "../artificialtsunami2d/ArtificialTsunami2d.h"
#include "TsunamiEvent2d.h"
#include <catch2/catch.hpp>
#include <fstream>

namespace {
const char* kBathPath =
    "ressources/artificial_tsunami_2d/artificialtsunami_bathymetry_1000.nc";
const char* kDisplPath =
    "ressources/artificial_tsunami_2d/artificialtsunami_displ_1000.nc";

bool fileExists(const char* i_path) {
  std::ifstream l_f(i_path);
  return l_f.good();
}
} // namespace

TEST_CASE("TsunamiEvent2d matches ArtificialTsunami2d on linked input data",
          "[TsunamiEvent2d][compare]") {
  if (!fileExists(kBathPath) || !fileExists(kDisplPath)) {
    SUCCEED("artificial-tsunami input data not present, skipping");
    return;
  }

  tsunami_lab::setups::TsunamiEvent2d l_fileSetup(kBathPath, kDisplPath);
  tsunami_lab::setups::ArtificialTsunami2d l_refSetup;

  // momenta are constant zero in both setups
  REQUIRE(l_fileSetup.getMomentumX(0.f, 0.f) == Approx(0.f));
  REQUIRE(l_fileSetup.getMomentumY(0.f, 0.f) == Approx(0.f));

  // Domain reported by the file-based setup must match the bathymetry file:
  // 1000 cells x 10 m spacing centered on the origin, full extent [-5000, 5000].
  REQUIRE(l_fileSetup.getDomainOriginX() == Approx(-5000.f));
  REQUIRE(l_fileSetup.getDomainOriginY() == Approx(-5000.f));
  REQUIRE(l_fileSetup.getDomainSizeX() == Approx(10000.f));
  REQUIRE(l_fileSetup.getDomainSizeY() == Approx(10000.f));

  // ---- inside the displacement square: sample at the cell centers
  // of the 100x100 displacement grid (x, y in {-495, -485, ..., 495}).
  // Nearest-neighbour lookup is exact at those points, so file and
  // analytic setup must agree to float precision.
  for (int l_iy = 0; l_iy < 100; l_iy += 7) { // stride to keep the test fast
    for (int l_ix = 0; l_ix < 100; l_ix += 7) {
      float l_x = -495.f + 10.f * static_cast<float>(l_ix);
      float l_y = -495.f + 10.f * static_cast<float>(l_iy);

      INFO("sample (" << l_x << ", " << l_y << ")");
      REQUIRE(l_fileSetup.getHeight(l_x, l_y) ==
              Approx(l_refSetup.getHeight(l_x, l_y)).margin(1e-4f));
      REQUIRE(l_fileSetup.getBathymetry(l_x, l_y) ==
              Approx(l_refSetup.getBathymetry(l_x, l_y)).margin(1e-4f));
    }
  }

  // ---- outside the displacement square but inside the bathymetry domain:
  // displacement = 0 in both setups, bathymetry = -100, height = 100.
  for (float l_x : {-4000.f, -1500.f, 600.f, 2500.f, 4500.f}) {
    for (float l_y : {-4000.f, -1500.f, 600.f, 2500.f, 4500.f}) {
      INFO("outside-square sample (" << l_x << ", " << l_y << ")");
      REQUIRE(l_fileSetup.getHeight(l_x, l_y) ==
              Approx(l_refSetup.getHeight(l_x, l_y)).margin(1e-4f));
      REQUIRE(l_fileSetup.getBathymetry(l_x, l_y) ==
              Approx(l_refSetup.getBathymetry(l_x, l_y)).margin(1e-4f));
    }
  }
}
