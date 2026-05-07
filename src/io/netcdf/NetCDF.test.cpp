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

namespace {
// Helper: create a small netCDF file with x, y coords and a 2D variable z
// whose dimension order is (y, x) — the COARDS-typical layout.
void writeGridFileYX(const char* i_path,
                     size_t i_nx,
                     size_t i_ny,
                     const float* i_x,
                     const float* i_y,
                     const float* i_z) {
  int l_ncId = -1;
  REQUIRE(nc_create(i_path, NC_CLOBBER | NC_NETCDF4, &l_ncId) == NC_NOERR);

  int l_dimX, l_dimY;
  REQUIRE(nc_def_dim(l_ncId, "x", i_nx, &l_dimX) == NC_NOERR);
  REQUIRE(nc_def_dim(l_ncId, "y", i_ny, &l_dimY) == NC_NOERR);

  int l_varX, l_varY, l_varZ;
  REQUIRE(nc_def_var(l_ncId, "x", NC_FLOAT, 1, &l_dimX, &l_varX) == NC_NOERR);
  REQUIRE(nc_def_var(l_ncId, "y", NC_FLOAT, 1, &l_dimY, &l_varY) == NC_NOERR);
  int l_dimsYX[2] = {l_dimY, l_dimX};
  REQUIRE(nc_def_var(l_ncId, "z", NC_FLOAT, 2, l_dimsYX, &l_varZ) == NC_NOERR);

  REQUIRE(nc_enddef(l_ncId) == NC_NOERR);

  REQUIRE(nc_put_var_float(l_ncId, l_varX, i_x) == NC_NOERR);
  REQUIRE(nc_put_var_float(l_ncId, l_varY, i_y) == NC_NOERR);
  REQUIRE(nc_put_var_float(l_ncId, l_varZ, i_z) == NC_NOERR);

  nc_close(l_ncId);
}

// Same as above but data variable has dimensions in (x, y) order — the
// reader must transpose to row-major (y, x).
void writeGridFileXY(const char* i_path,
                     size_t i_nx,
                     size_t i_ny,
                     const float* i_x,
                     const float* i_y,
                     const float* i_zXy) {
  int l_ncId = -1;
  REQUIRE(nc_create(i_path, NC_CLOBBER | NC_NETCDF4, &l_ncId) == NC_NOERR);

  int l_dimX, l_dimY;
  REQUIRE(nc_def_dim(l_ncId, "x", i_nx, &l_dimX) == NC_NOERR);
  REQUIRE(nc_def_dim(l_ncId, "y", i_ny, &l_dimY) == NC_NOERR);

  int l_varX, l_varY, l_varZ;
  REQUIRE(nc_def_var(l_ncId, "x", NC_FLOAT, 1, &l_dimX, &l_varX) == NC_NOERR);
  REQUIRE(nc_def_var(l_ncId, "y", NC_FLOAT, 1, &l_dimY, &l_varY) == NC_NOERR);
  int l_dimsXY[2] = {l_dimX, l_dimY};
  REQUIRE(nc_def_var(l_ncId, "z", NC_FLOAT, 2, l_dimsXY, &l_varZ) == NC_NOERR);

  REQUIRE(nc_enddef(l_ncId) == NC_NOERR);

  REQUIRE(nc_put_var_float(l_ncId, l_varX, i_x) == NC_NOERR);
  REQUIRE(nc_put_var_float(l_ncId, l_varY, i_y) == NC_NOERR);
  REQUIRE(nc_put_var_float(l_ncId, l_varZ, i_zXy) == NC_NOERR);

  nc_close(l_ncId);
}
} // namespace

TEST_CASE("NetCDF read parses (y, x) layout", "[NetCDF]") {
  const size_t l_nx = 4;
  const size_t l_ny = 3;
  const char* l_path = "/tmp/test_netcdf_read_yx.nc";

  float l_x[l_nx] = {0.0f, 10.0f, 20.0f, 30.0f};
  float l_y[l_ny] = {-5.0f, 0.0f, 5.0f};
  // z[iy * nx + ix] = ix * 100 + iy
  float l_z[l_nx * l_ny];
  for (size_t l_iy = 0; l_iy < l_ny; l_iy++)
    for (size_t l_ix = 0; l_ix < l_nx; l_ix++)
      l_z[l_iy * l_nx + l_ix] =
          static_cast<float>(l_ix) * 100.0f + static_cast<float>(l_iy);

  writeGridFileYX(l_path, l_nx, l_ny, l_x, l_y, l_z);

  tsunami_lab::t_idx l_outNx = 0, l_outNy = 0;
  tsunami_lab::t_real* l_outX = nullptr;
  tsunami_lab::t_real* l_outY = nullptr;
  tsunami_lab::t_real* l_outZ = nullptr;
  tsunami_lab::io::NetCDF::read(l_path, "z", l_outNx, l_outNy, l_outX, l_outY,
                                l_outZ);

  REQUIRE(l_outNx == l_nx);
  REQUIRE(l_outNy == l_ny);

  for (size_t l_i = 0; l_i < l_nx; l_i++)
    REQUIRE(l_outX[l_i] == Approx(l_x[l_i]));
  for (size_t l_j = 0; l_j < l_ny; l_j++)
    REQUIRE(l_outY[l_j] == Approx(l_y[l_j]));

  // values should round-trip exactly
  for (size_t l_iy = 0; l_iy < l_ny; l_iy++)
    for (size_t l_ix = 0; l_ix < l_nx; l_ix++)
      REQUIRE(l_outZ[l_iy * l_nx + l_ix] == Approx(l_z[l_iy * l_nx + l_ix]));

  delete[] l_outX;
  delete[] l_outY;
  delete[] l_outZ;
}

TEST_CASE("NetCDF read transposes (x, y) layout", "[NetCDF]") {
  const size_t l_nx = 3;
  const size_t l_ny = 4;
  const char* l_path = "/tmp/test_netcdf_read_xy.nc";

  float l_x[l_nx] = {1.0f, 2.0f, 3.0f};
  float l_y[l_ny] = {0.0f, 1.0f, 2.0f, 3.0f};
  // file stores data in (x, y) order: zXy[ix * ny + iy] = ix * 10 + iy
  float l_zXy[l_nx * l_ny];
  for (size_t l_ix = 0; l_ix < l_nx; l_ix++)
    for (size_t l_iy = 0; l_iy < l_ny; l_iy++)
      l_zXy[l_ix * l_ny + l_iy] =
          static_cast<float>(l_ix) * 10.0f + static_cast<float>(l_iy);

  writeGridFileXY(l_path, l_nx, l_ny, l_x, l_y, l_zXy);

  tsunami_lab::t_idx l_outNx = 0, l_outNy = 0;
  tsunami_lab::t_real* l_outX = nullptr;
  tsunami_lab::t_real* l_outY = nullptr;
  tsunami_lab::t_real* l_outZ = nullptr;
  tsunami_lab::io::NetCDF::read(l_path, "z", l_outNx, l_outNy, l_outX, l_outY,
                                l_outZ);

  REQUIRE(l_outNx == l_nx);
  REQUIRE(l_outNy == l_ny);

  // output is row-major (y, x): out[iy * nx + ix] == zXy[ix * ny + iy]
  for (size_t l_iy = 0; l_iy < l_ny; l_iy++)
    for (size_t l_ix = 0; l_ix < l_nx; l_ix++)
      REQUIRE(l_outZ[l_iy * l_nx + l_ix] == Approx(l_zXy[l_ix * l_ny + l_iy]));

  delete[] l_outX;
  delete[] l_outY;
  delete[] l_outZ;
}