/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Unit tests for the CheckPoint setup.
 **/
#include "CheckPoint.h"
#include "../../io/netcdf/NetCDF.h"
#include <catch2/catch.hpp>
#include <vector>

TEST_CASE("CheckPoint restores fields from a netCDF checkpoint",
          "[CheckPoint]") {
  const char* l_path = "/tmp/test_checkpoint_setup.nc";
  const tsunami_lab::t_idx l_nx = 4;
  const tsunami_lab::t_idx l_ny = 3;
  const tsunami_lab::t_real l_dxy = 10.0f;
  const tsunami_lab::t_real l_originX = -5.0f;
  const tsunami_lab::t_real l_originY = 7.0f;
  const tsunami_lab::t_idx l_stride = l_nx;

  // build a known interior state
  std::vector<float> l_h(l_nx * l_ny), l_hu(l_nx * l_ny), l_hv(l_nx * l_ny),
      l_b(l_nx * l_ny);
  auto val = [](tsunami_lab::t_idx i_ix, tsunami_lab::t_idx i_iy) {
    return static_cast<float>(i_ix) + 100.0f * static_cast<float>(i_iy);
  };
  for (tsunami_lab::t_idx l_iy = 0; l_iy < l_ny; l_iy++) {
    for (tsunami_lab::t_idx l_ix = 0; l_ix < l_nx; l_ix++) {
      tsunami_lab::t_idx l_i = l_ix + l_iy * l_stride;
      l_h[l_i] = 1.0f + val(l_ix, l_iy);
      l_hu[l_i] = 2.0f + val(l_ix, l_iy);
      l_hv[l_i] = 3.0f + val(l_ix, l_iy);
      l_b[l_i] = -1000.0f + val(l_ix, l_iy);
    }
  }

  {
    tsunami_lab::io::NetCDF l_writer(l_nx, l_ny, l_dxy, l_dxy, l_originX,
                                     l_originY, l_path);
    l_writer.writeMetadata(123.0f, 0.5f, "outflow", "outflow", "fwave",
                           "tsunamievent2d");
    l_writer.write(0.5f, l_h.data(), l_hu.data(), l_hv.data(), l_b.data(),
                   l_stride);
  }

  tsunami_lab::setups::CheckPoint l_cp(l_path);

  // metadata survives the round-trip
  const auto& l_info = l_cp.getInfo();
  REQUIRE(l_info.nx == l_nx);
  REQUIRE(l_info.ny == l_ny);
  REQUIRE(l_info.dxy == Approx(l_dxy));
  REQUIRE(l_info.originX == Approx(l_originX));
  REQUIRE(l_info.originY == Approx(l_originY));
  REQUIRE(l_info.lastSimTime == Approx(0.5f));

  // querying each cell center returns that cell's stored value
  for (tsunami_lab::t_idx l_iy = 0; l_iy < l_ny; l_iy++) {
    for (tsunami_lab::t_idx l_ix = 0; l_ix < l_nx; l_ix++) {
      tsunami_lab::t_real l_x = l_originX + (l_ix + 0.5f) * l_dxy;
      tsunami_lab::t_real l_y = l_originY + (l_iy + 0.5f) * l_dxy;
      REQUIRE(l_cp.getHeight(l_x, l_y) == Approx(1.0f + val(l_ix, l_iy)));
      REQUIRE(l_cp.getMomentumX(l_x, l_y) == Approx(2.0f + val(l_ix, l_iy)));
      REQUIRE(l_cp.getMomentumY(l_x, l_y) == Approx(3.0f + val(l_ix, l_iy)));
      REQUIRE(l_cp.getBathymetry(l_x, l_y) ==
              Approx(-1000.0f + val(l_ix, l_iy)));
    }
  }
}

TEST_CASE("CheckPoint clamps out-of-domain queries to the border",
          "[CheckPoint]") {
  const char* l_path = "/tmp/test_checkpoint_clamp.nc";
  const tsunami_lab::t_idx l_nx = 2;
  const tsunami_lab::t_idx l_ny = 2;
  const tsunami_lab::t_real l_dxy = 1.0f;
  const tsunami_lab::t_idx l_stride = l_nx;

  std::vector<float> l_h(l_nx * l_ny), l_zero(l_nx * l_ny, 0.0f);
  // distinct values so we can identify which cell got picked
  l_h[0 + 0 * l_stride] = 10.0f; // (0,0)
  l_h[1 + 0 * l_stride] = 20.0f; // (1,0)
  l_h[0 + 1 * l_stride] = 30.0f; // (0,1)
  l_h[1 + 1 * l_stride] = 40.0f; // (1,1)

  {
    tsunami_lab::io::NetCDF l_writer(l_nx, l_ny, l_dxy, l_dxy, 0.0f, 0.0f,
                                     l_path);
    l_writer.write(0.0f, l_h.data(), l_zero.data(), l_zero.data(),
                   l_zero.data(), l_stride);
  }

  tsunami_lab::setups::CheckPoint l_cp(l_path);

  // far below origin -> clamped to cell (0,0)
  REQUIRE(l_cp.getHeight(-100.0f, -100.0f) == Approx(10.0f));
  // far beyond domain -> clamped to cell (nx-1, ny-1)
  REQUIRE(l_cp.getHeight(100.0f, 100.0f) == Approx(40.0f));
}
