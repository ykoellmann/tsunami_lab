/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Unit tests for the NetCDF IO class.
 **/
#include "NetCDF.h"
#include <catch2/catch.hpp>
#include <cstdio>
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

TEST_CASE("NetCDF hasCheckpoint detects usable checkpoints", "[NetCDF]") {
  const char* l_path = "/tmp/test_netcdf_has_checkpoint.nc";
  const tsunami_lab::t_idx l_nx = 3;
  const tsunami_lab::t_idx l_ny = 2;
  const tsunami_lab::t_idx l_stride = l_nx;
  std::vector<float> l_field(l_nx * l_ny, 0.0f);

  // missing file is not a checkpoint
  std::remove(l_path);
  REQUIRE(tsunami_lab::io::NetCDF::hasCheckpoint(l_path) == false);

  // a freshly created file with zero time steps is not yet a checkpoint
  {
    tsunami_lab::io::NetCDF l_writer(l_nx, l_ny, 1.0f, 1.0f, 0.0f, 0.0f,
                                     l_path);
  }
  REQUIRE(tsunami_lab::io::NetCDF::hasCheckpoint(l_path) == false);

  // after a single written time step it becomes a valid checkpoint
  {
    tsunami_lab::io::NetCDF l_writer(l_nx, l_ny, 1.0f, 1.0f, 0.0f, 0.0f,
                                     l_path);
    l_writer.write(0.0f, l_field.data(), l_field.data(), l_field.data(),
                   l_field.data(), l_stride);
  }
  REQUIRE(tsunami_lab::io::NetCDF::hasCheckpoint(l_path) == true);
}

TEST_CASE("NetCDF metadata + readCheckpoint round-trip", "[NetCDF]") {
  const char* l_path = "/tmp/test_netcdf_checkpoint.nc";
  const tsunami_lab::t_idx l_nx = 3;
  const tsunami_lab::t_idx l_ny = 2;
  const tsunami_lab::t_real l_dxy = 100.0f;
  const tsunami_lab::t_real l_originX = -50.0f;
  const tsunami_lab::t_real l_originY = -20.0f;
  const tsunami_lab::t_idx l_stride = l_nx;

  // interior fields; the last time step carries the values we verify
  std::vector<float> l_h0(l_nx * l_ny, 0.0f);
  std::vector<float> l_h1(l_nx * l_ny), l_hu1(l_nx * l_ny), l_hv1(l_nx * l_ny),
      l_b(l_nx * l_ny);
  for (tsunami_lab::t_idx l_iy = 0; l_iy < l_ny; l_iy++) {
    for (tsunami_lab::t_idx l_ix = 0; l_ix < l_nx; l_ix++) {
      tsunami_lab::t_idx l_i = l_ix + l_iy * l_stride;
      l_h1[l_i] = 100.0f + l_ix + 10.0f * l_iy;
      l_hu1[l_i] = 1.0f + l_ix;
      l_hv1[l_i] = 2.0f + l_iy;
      l_b[l_i] = -500.0f + l_ix + l_iy;
    }
  }

  {
    tsunami_lab::io::NetCDF l_writer(l_nx, l_ny, l_dxy, l_dxy, l_originX,
                                     l_originY, l_path);
    l_writer.writeMetadata(500.0f, 2.5f, "outflow", "reflecting", "fwave",
                           "tsunamievent2d");
    // first step: zeros (b is written here, once)
    l_writer.write(0.0f, l_h0.data(), l_h0.data(), l_h0.data(), l_b.data(),
                   l_stride);
    // last step: the state a restart must recover
    l_writer.write(2.5f, l_h1.data(), l_hu1.data(), l_hv1.data(), l_b.data(),
                   l_stride);
  }

  tsunami_lab::io::NetCDF::CheckpointInfo l_info;
  tsunami_lab::t_real *l_rh = nullptr, *l_rhu = nullptr, *l_rhv = nullptr,
                      *l_rb = nullptr;
  tsunami_lab::io::NetCDF::readCheckpoint(l_path, l_info, l_rh, l_rhu, l_rhv,
                                          l_rb);

  // metadata round-trips
  REQUIRE(l_info.nx == l_nx);
  REQUIRE(l_info.ny == l_ny);
  REQUIRE(l_info.nTimeSteps == 2);
  REQUIRE(l_info.dxy == Approx(l_dxy));
  REQUIRE(l_info.originX == Approx(l_originX));
  REQUIRE(l_info.originY == Approx(l_originY));
  REQUIRE(l_info.endTime == Approx(500.0f));
  REQUIRE(l_info.dt == Approx(2.5f));
  REQUIRE(l_info.lastSimTime == Approx(2.5f));
  REQUIRE(l_info.bcLeft == "outflow");
  REQUIRE(l_info.bcRight == "reflecting");
  REQUIRE(l_info.solverMode == "fwave");
  REQUIRE(l_info.setupMode == "tsunamievent2d");

  // last time slice round-trips; output is row-major (iy * nx + ix)
  for (tsunami_lab::t_idx l_iy = 0; l_iy < l_ny; l_iy++) {
    for (tsunami_lab::t_idx l_ix = 0; l_ix < l_nx; l_ix++) {
      tsunami_lab::t_idx l_src = l_ix + l_iy * l_stride;
      tsunami_lab::t_idx l_dst = l_iy * l_nx + l_ix;
      REQUIRE(l_rh[l_dst] == Approx(l_h1[l_src]));
      REQUIRE(l_rhu[l_dst] == Approx(l_hu1[l_src]));
      REQUIRE(l_rhv[l_dst] == Approx(l_hv1[l_src]));
      REQUIRE(l_rb[l_dst] == Approx(l_b[l_src]));
    }
  }

  delete[] l_rh;
  delete[] l_rhu;
  delete[] l_rhv;
  delete[] l_rb;
}

TEST_CASE("NetCDF coarse output averages k x k blocks", "[NetCDF]") {
  // 4x4 source grid, k = 2 -> 2x2 output, every block fully populated
  const tsunami_lab::t_idx l_nx = 4;
  const tsunami_lab::t_idx l_ny = 4;
  const tsunami_lab::t_idx l_k = 2;
  const tsunami_lab::t_idx l_stride = l_nx;
  const char* l_path = "/tmp/test_netcdf_coarse.nc";

  // h[ix + iy*stride] = ix + 10*iy
  std::vector<float> l_h(l_nx * l_ny), l_zero(l_nx * l_ny, 0.0f);
  for (tsunami_lab::t_idx l_iy = 0; l_iy < l_ny; l_iy++)
    for (tsunami_lab::t_idx l_ix = 0; l_ix < l_nx; l_ix++)
      l_h[l_ix + l_iy * l_stride] =
          static_cast<float>(l_ix) + 10.0f * static_cast<float>(l_iy);

  {
    tsunami_lab::io::NetCDF l_writer(l_nx, l_ny, 1.0f, 1.0f, 0.0f, 0.0f, l_path,
                                     l_k);
    l_writer.write(0.0f, l_h.data(), l_zero.data(), l_zero.data(),
                   l_zero.data(), l_stride);
  }

  int l_ncId = -1;
  REQUIRE(nc_open(l_path, NC_NOWRITE, &l_ncId) == NC_NOERR);

  int l_dimX, l_dimY;
  REQUIRE(nc_inq_dimid(l_ncId, "x", &l_dimX) == NC_NOERR);
  REQUIRE(nc_inq_dimid(l_ncId, "y", &l_dimY) == NC_NOERR);
  size_t l_outNx, l_outNy;
  nc_inq_dimlen(l_ncId, l_dimX, &l_outNx);
  nc_inq_dimlen(l_ncId, l_dimY, &l_outNy);
  REQUIRE(l_outNx == 2);
  REQUIRE(l_outNy == 2);

  int l_varH = -1;
  REQUIRE(nc_inq_varid(l_ncId, "h", &l_varH) == NC_NOERR);
  size_t l_start[3] = {0, 0, 0};
  size_t l_count[3] = {1, l_outNx, l_outNy};
  std::vector<float> l_read(l_outNx * l_outNy);
  REQUIRE(nc_get_vara_float(l_ncId, l_varH, l_start, l_count, l_read.data()) ==
          NC_NOERR);

  // block (ox, oy) averages source values; stored as read[ox*outNy + oy]
  REQUIRE(l_read[0 * l_outNy + 0] == Approx(5.5f));  // ix{0,1} iy{0,1}
  REQUIRE(l_read[1 * l_outNy + 0] == Approx(7.5f));  // ix{2,3} iy{0,1}
  REQUIRE(l_read[0 * l_outNy + 1] == Approx(25.5f)); // ix{0,1} iy{2,3}
  REQUIRE(l_read[1 * l_outNy + 1] == Approx(27.5f)); // ix{2,3} iy{2,3}

  nc_close(l_ncId);
}

TEST_CASE("NetCDF coarse output handles non-divisible dimensions", "[NetCDF]") {
  // 3x3 source, k = 2 -> ceil(3/2) = 2 output cells per axis; the trailing
  // blocks only cover a single source cell.
  const tsunami_lab::t_idx l_nx = 3;
  const tsunami_lab::t_idx l_ny = 3;
  const tsunami_lab::t_idx l_k = 2;
  const tsunami_lab::t_idx l_stride = l_nx;
  const char* l_path = "/tmp/test_netcdf_coarse_odd.nc";

  std::vector<float> l_h(l_nx * l_ny), l_zero(l_nx * l_ny, 0.0f);
  for (tsunami_lab::t_idx l_iy = 0; l_iy < l_ny; l_iy++)
    for (tsunami_lab::t_idx l_ix = 0; l_ix < l_nx; l_ix++)
      l_h[l_ix + l_iy * l_stride] =
          static_cast<float>(l_ix) + 10.0f * static_cast<float>(l_iy);

  {
    tsunami_lab::io::NetCDF l_writer(l_nx, l_ny, 1.0f, 1.0f, 0.0f, 0.0f, l_path,
                                     l_k);
    l_writer.write(0.0f, l_h.data(), l_zero.data(), l_zero.data(),
                   l_zero.data(), l_stride);
  }

  int l_ncId = -1;
  REQUIRE(nc_open(l_path, NC_NOWRITE, &l_ncId) == NC_NOERR);

  int l_dimX, l_dimY;
  nc_inq_dimid(l_ncId, "x", &l_dimX);
  nc_inq_dimid(l_ncId, "y", &l_dimY);
  size_t l_outNx, l_outNy;
  nc_inq_dimlen(l_ncId, l_dimX, &l_outNx);
  nc_inq_dimlen(l_ncId, l_dimY, &l_outNy);
  REQUIRE(l_outNx == 2);
  REQUIRE(l_outNy == 2);

  int l_varH = -1;
  nc_inq_varid(l_ncId, "h", &l_varH);
  size_t l_start[3] = {0, 0, 0};
  size_t l_count[3] = {1, l_outNx, l_outNy};
  std::vector<float> l_read(l_outNx * l_outNy);
  nc_get_vara_float(l_ncId, l_varH, l_start, l_count, l_read.data());

  // (0,0): full 2x2 block ix{0,1} iy{0,1}: 0,1,10,11 -> 5.5
  REQUIRE(l_read[0 * l_outNy + 0] == Approx(5.5f));
  // (1,0): partial ix{2} iy{0,1}: 2,12 -> 7.0
  REQUIRE(l_read[1 * l_outNy + 0] == Approx(7.0f));
  // (0,1): partial ix{0,1} iy{2}: 20,21 -> 20.5
  REQUIRE(l_read[0 * l_outNy + 1] == Approx(20.5f));
  // (1,1): single cell ix{2} iy{2}: 22 -> 22.0
  REQUIRE(l_read[1 * l_outNy + 1] == Approx(22.0f));

  nc_close(l_ncId);
}