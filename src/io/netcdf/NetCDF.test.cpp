/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Unit tests for the NetCDF IO class.
 **/
#include "NetCDF.h"
#include <catch2/catch.hpp>
#include <cstring>
#include <netcdf.h>
#include <vector>

TEST_CASE("NetCDF write produces readable output", "[NetCDF]") {
  const tsunami_lab::t_idx l_nx = 4;
  const tsunami_lab::t_idx l_ny = 3;
  const tsunami_lab::t_real l_dx = 1.0f;
  const tsunami_lab::t_real l_dy = 1.0f;
  const char* l_path = "/tmp/test_netcdf_output.nc";

  // stride for a 2D patch: nCells_y + 2 ghost cells
  const tsunami_lab::t_idx l_stride = l_ny + 2;

  // interior data stored with ghost-cell stride
  const tsunami_lab::t_idx l_size = l_nx * l_stride;
  std::vector<float> l_h(l_size, 0.0f);
  std::vector<float> l_hu(l_size, 0.0f);
  std::vector<float> l_hv(l_size, 0.0f);
  std::vector<float> l_b(l_size, 0.0f);

  // fill interior cells: h[ix][iy] = ix + iy, b = -100
  for (tsunami_lab::t_idx l_ix = 0; l_ix < l_nx; l_ix++) {
    for (tsunami_lab::t_idx l_iy = 0; l_iy < l_ny; l_iy++) {
      l_h[l_ix * l_stride + l_iy] = (float)(l_ix + l_iy);
      l_b[l_ix * l_stride + l_iy] = -100.0f;
    }
  }

  {
    tsunami_lab::io::NetCDF l_writer(l_nx, l_ny, l_dx, l_dy, 0.0f, 0.0f,
                                     l_path);
    l_writer.write(0.0f, l_h.data(), l_hu.data(), l_hv.data(), l_b.data(),
                   l_stride);
    l_writer.write(1.0f, l_h.data(), l_hu.data(), l_hv.data(), l_b.data(),
                   l_stride);
  }

  // verify with raw netCDF API
  int l_ncId = -1;
  REQUIRE(nc_open(l_path, NC_NOWRITE, &l_ncId) == NC_NOERR);

  int l_dimTime, l_dimX, l_dimY;
  REQUIRE(nc_inq_dimid(l_ncId, "time", &l_dimTime) == NC_NOERR);
  REQUIRE(nc_inq_dimid(l_ncId, "x", &l_dimX) == NC_NOERR);
  REQUIRE(nc_inq_dimid(l_ncId, "y", &l_dimY) == NC_NOERR);

  size_t l_lenTime, l_lenX, l_lenY;
  nc_inq_dimlen(l_ncId, l_dimTime, &l_lenTime);
  nc_inq_dimlen(l_ncId, l_dimX, &l_lenX);
  nc_inq_dimlen(l_ncId, l_dimY, &l_lenY);

  REQUIRE(l_lenTime == 2);
  REQUIRE(l_lenX == l_nx);
  REQUIRE(l_lenY == l_ny);

  // read h from first time step and check one cell
  int l_varH = -1;
  REQUIRE(nc_inq_varid(l_ncId, "h", &l_varH) == NC_NOERR);

  size_t l_start[3] = {0, 0, 0};
  size_t l_count[3] = {1, l_nx, l_ny};
  std::vector<float> l_hRead(l_nx * l_ny);
  REQUIRE(nc_get_vara_float(l_ncId, l_varH, l_start, l_count, l_hRead.data()) ==
          NC_NOERR);

  // cell (2, 1): h = 2 + 1 = 3
  REQUIRE(l_hRead[2 * l_ny + 1] == Approx(3.0f));

  nc_close(l_ncId);
}