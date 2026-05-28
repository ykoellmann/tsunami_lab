/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * IO-routines for writing simulation data as NetCDF.
 **/
#include "NetCDF.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
inline int
putAttStr(int i_ncId, int i_varId, const char* i_name, const char* i_value) {
  return nc_put_att_text(i_ncId, i_varId, i_name, std::strlen(i_value),
                         i_value);
}

// Read a text global attribute into a std::string. Returns true on success.
bool getAttStr(int i_ncId, const char* i_name, std::string& o_value) {
  size_t l_len = 0;
  if (nc_inq_attlen(i_ncId, NC_GLOBAL, i_name, &l_len) != NC_NOERR)
    return false;
  std::vector<char> l_buf(l_len + 1, '\0');
  if (nc_get_att_text(i_ncId, NC_GLOBAL, i_name, l_buf.data()) != NC_NOERR)
    return false;
  o_value.assign(l_buf.data(), l_len);
  return true;
}
} // namespace

namespace tsunami_lab {
namespace io {

void NetCDF::checkErr(int i_err) {
  if (i_err != NC_NOERR) {
    std::cerr << "NetCDF error: " << nc_strerror(i_err) << std::endl;
    std::exit(EXIT_FAILURE);
  }
}

NetCDF::NetCDF(t_idx i_nx,
               t_idx i_ny,
               t_real i_dx,
               t_real i_dy,
               t_real i_originX,
               t_real i_originY,
               const char* i_path)
    : m_nx(i_nx), m_ny(i_ny), m_dx(i_dx), m_dy(i_dy), m_originX(i_originX),
      m_originY(i_originY) {

  checkErr(nc_create(i_path, NC_CLOBBER | NC_NETCDF4, &m_ncId));

  // define dimensions
  checkErr(nc_def_dim(m_ncId, "time", NC_UNLIMITED, &m_dimTime));
  checkErr(nc_def_dim(m_ncId, "x", m_nx, &m_dimX));
  checkErr(nc_def_dim(m_ncId, "y", m_ny, &m_dimY));

  // coordinate variables
  checkErr(nc_def_var(m_ncId, "time", NC_FLOAT, 1, &m_dimTime, &m_varTime));
  checkErr(nc_def_var(m_ncId, "x", NC_FLOAT, 1, &m_dimX, &m_varX));
  checkErr(nc_def_var(m_ncId, "y", NC_FLOAT, 1, &m_dimY, &m_varY));

  // COARDS attributes for coordinate variables
  checkErr(
      putAttStr(m_ncId, m_varTime, "units", "seconds since earthquake event"));
  checkErr(putAttStr(m_ncId, m_varX, "units", "meters"));
  checkErr(putAttStr(m_ncId, m_varX, "axis", "X"));
  checkErr(putAttStr(m_ncId, m_varY, "units", "meters"));
  checkErr(putAttStr(m_ncId, m_varY, "axis", "Y"));

  // grid layout as global attributes — required for checkpoint restart
  float l_dx = static_cast<float>(m_dx);
  float l_dy = static_cast<float>(m_dy);
  float l_originX = static_cast<float>(m_originX);
  float l_originY = static_cast<float>(m_originY);
  checkErr(nc_put_att_float(m_ncId, NC_GLOBAL, "dx", NC_FLOAT, 1, &l_dx));
  checkErr(nc_put_att_float(m_ncId, NC_GLOBAL, "dy", NC_FLOAT, 1, &l_dy));
  checkErr(
      nc_put_att_float(m_ncId, NC_GLOBAL, "origin_x", NC_FLOAT, 1, &l_originX));
  checkErr(
      nc_put_att_float(m_ncId, NC_GLOBAL, "origin_y", NC_FLOAT, 1, &l_originY));

  // data variables: [time, x, y]
  int l_dimsTxy[3] = {m_dimTime, m_dimX, m_dimY};
  int l_dimsXy[2] = {m_dimX, m_dimY};

  checkErr(nc_def_var(m_ncId, "h", NC_FLOAT, 3, l_dimsTxy, &m_varH));
  checkErr(putAttStr(m_ncId, m_varH, "units", "meters"));
  checkErr(putAttStr(m_ncId, m_varH, "long_name", "water height"));

  checkErr(nc_def_var(m_ncId, "hu", NC_FLOAT, 3, l_dimsTxy, &m_varHu));
  checkErr(putAttStr(m_ncId, m_varHu, "units", "meters^2/second"));
  checkErr(putAttStr(m_ncId, m_varHu, "long_name", "x-momentum (h * u)"));

  checkErr(nc_def_var(m_ncId, "hv", NC_FLOAT, 3, l_dimsTxy, &m_varHv));
  checkErr(putAttStr(m_ncId, m_varHv, "units", "meters^2/second"));
  checkErr(putAttStr(m_ncId, m_varHv, "long_name", "y-momentum (h * v)"));

  checkErr(nc_def_var(m_ncId, "b", NC_FLOAT, 2, l_dimsXy, &m_varB));
  checkErr(putAttStr(m_ncId, m_varB, "units", "meters"));
  checkErr(putAttStr(m_ncId, m_varB, "long_name", "bathymetry"));

  checkErr(nc_enddef(m_ncId));

  // write x coordinate values: cell centers
  std::vector<float> l_x(m_nx), l_y(m_ny);
  for (t_idx l_i = 0; l_i < m_nx; l_i++)
    l_x[l_i] = m_originX + (l_i + 0.5f) * m_dx;
  for (t_idx l_j = 0; l_j < m_ny; l_j++)
    l_y[l_j] = m_originY + (l_j + 0.5f) * m_dy;

  checkErr(nc_put_var_float(m_ncId, m_varX, l_x.data()));
  checkErr(nc_put_var_float(m_ncId, m_varY, l_y.data()));
}

NetCDF::NetCDF(const char* i_path) {
  // open existing file for append
  checkErr(nc_open(i_path, NC_WRITE, &m_ncId));

  // look up dimensions
  checkErr(nc_inq_dimid(m_ncId, "time", &m_dimTime));
  checkErr(nc_inq_dimid(m_ncId, "x", &m_dimX));
  checkErr(nc_inq_dimid(m_ncId, "y", &m_dimY));

  size_t l_nTime = 0, l_nx = 0, l_ny = 0;
  checkErr(nc_inq_dimlen(m_ncId, m_dimTime, &l_nTime));
  checkErr(nc_inq_dimlen(m_ncId, m_dimX, &l_nx));
  checkErr(nc_inq_dimlen(m_ncId, m_dimY, &l_ny));
  m_nx = l_nx;
  m_ny = l_ny;
  m_timeStep = l_nTime;

  // look up variables
  checkErr(nc_inq_varid(m_ncId, "time", &m_varTime));
  checkErr(nc_inq_varid(m_ncId, "x", &m_varX));
  checkErr(nc_inq_varid(m_ncId, "y", &m_varY));
  checkErr(nc_inq_varid(m_ncId, "h", &m_varH));
  checkErr(nc_inq_varid(m_ncId, "hu", &m_varHu));
  checkErr(nc_inq_varid(m_ncId, "hv", &m_varHv));
  checkErr(nc_inq_varid(m_ncId, "b", &m_varB));

  // restore grid layout from global attributes
  float l_dx = 0, l_dy = 0, l_originX = 0, l_originY = 0;
  checkErr(nc_get_att_float(m_ncId, NC_GLOBAL, "dx", &l_dx));
  checkErr(nc_get_att_float(m_ncId, NC_GLOBAL, "dy", &l_dy));
  checkErr(nc_get_att_float(m_ncId, NC_GLOBAL, "origin_x", &l_originX));
  checkErr(nc_get_att_float(m_ncId, NC_GLOBAL, "origin_y", &l_originY));
  m_dx = l_dx;
  m_dy = l_dy;
  m_originX = l_originX;
  m_originY = l_originY;

  // bathymetry is a non-time variable; if any time step exists, it was
  // written by the original run — don't overwrite it on append.
  m_bWritten = (l_nTime > 0);
}

NetCDF::~NetCDF() {
  if (m_ncId >= 0)
    nc_close(m_ncId);
}

void NetCDF::write(t_real i_simTime,
                   t_real const* i_h,
                   t_real const* i_hu,
                   t_real const* i_hv,
                   t_real const* i_b,
                   t_idx i_stride) {
  // copy interior cells into contiguous buffer [x][y]
  std::vector<float> l_buf(m_nx * m_ny);

  auto fillBuf = [&](t_real const* i_src) {
    for (t_idx l_ix = 0; l_ix < m_nx; l_ix++)
      for (t_idx l_iy = 0; l_iy < m_ny; l_iy++)
        l_buf[l_ix * m_ny + l_iy] = i_src[l_ix + l_iy * i_stride];
  };

  // time coordinate
  size_t l_tIdx = m_timeStep;
  float l_time = i_simTime;
  checkErr(nc_put_var1_float(m_ncId, m_varTime, &l_tIdx, &l_time));

  // 3-D slice: [timeStep, :, :]
  size_t l_start3[3] = {m_timeStep, 0, 0};
  size_t l_count3[3] = {1, m_nx, m_ny};

  fillBuf(i_h);
  checkErr(nc_put_vara_float(m_ncId, m_varH, l_start3, l_count3, l_buf.data()));

  fillBuf(i_hu);
  checkErr(
      nc_put_vara_float(m_ncId, m_varHu, l_start3, l_count3, l_buf.data()));

  fillBuf(i_hv);
  checkErr(
      nc_put_vara_float(m_ncId, m_varHv, l_start3, l_count3, l_buf.data()));

  // bathymetry written once
  if (!m_bWritten) {
    fillBuf(i_b);
    checkErr(nc_put_var_float(m_ncId, m_varB, l_buf.data()));
    m_bWritten = true;
  }

  m_timeStep++;

  // flush in-memory buffers so an unexpected crash doesn't lose this step
  checkErr(nc_sync(m_ncId));
}

void NetCDF::writeMetadata(t_real i_endTime,
                           t_real i_dt,
                           const std::string& i_bcLeft,
                           const std::string& i_bcRight,
                           const std::string& i_solverMode,
                           const std::string& i_setupMode) {
  float l_endTime = static_cast<float>(i_endTime);
  float l_dt = static_cast<float>(i_dt);
  checkErr(
      nc_put_att_float(m_ncId, NC_GLOBAL, "end_time", NC_FLOAT, 1, &l_endTime));
  checkErr(nc_put_att_float(m_ncId, NC_GLOBAL, "dt", NC_FLOAT, 1, &l_dt));
  checkErr(putAttStr(m_ncId, NC_GLOBAL, "bc_left", i_bcLeft.c_str()));
  checkErr(putAttStr(m_ncId, NC_GLOBAL, "bc_right", i_bcRight.c_str()));
  checkErr(putAttStr(m_ncId, NC_GLOBAL, "solver_mode", i_solverMode.c_str()));
  checkErr(putAttStr(m_ncId, NC_GLOBAL, "setup_mode", i_setupMode.c_str()));
}

bool NetCDF::hasCheckpoint(const char* i_path) {
  int l_ncId = -1;
  if (nc_open(i_path, NC_NOWRITE, &l_ncId) != NC_NOERR)
    return false;

  int l_dimTime = -1;
  if (nc_inq_dimid(l_ncId, "time", &l_dimTime) != NC_NOERR) {
    nc_close(l_ncId);
    return false;
  }
  size_t l_len = 0;
  if (nc_inq_dimlen(l_ncId, l_dimTime, &l_len) != NC_NOERR) {
    nc_close(l_ncId);
    return false;
  }
  nc_close(l_ncId);
  return l_len >= 1;
}

void NetCDF::readCheckpoint(const char* i_path,
                            CheckpointInfo& o_info,
                            t_real*& o_h,
                            t_real*& o_hu,
                            t_real*& o_hv,
                            t_real*& o_b) {
  int l_ncId = -1;
  checkErr(nc_open(i_path, NC_NOWRITE, &l_ncId));

  // dimension ids and lengths
  int l_dimTime = -1, l_dimX = -1, l_dimY = -1;
  checkErr(nc_inq_dimid(l_ncId, "time", &l_dimTime));
  checkErr(nc_inq_dimid(l_ncId, "x", &l_dimX));
  checkErr(nc_inq_dimid(l_ncId, "y", &l_dimY));

  size_t l_nTime = 0, l_nx = 0, l_ny = 0;
  checkErr(nc_inq_dimlen(l_ncId, l_dimTime, &l_nTime));
  checkErr(nc_inq_dimlen(l_ncId, l_dimX, &l_nx));
  checkErr(nc_inq_dimlen(l_ncId, l_dimY, &l_ny));

  if (l_nTime == 0) {
    std::cerr << "NetCDF readCheckpoint: file has no time steps" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  o_info.nx = l_nx;
  o_info.ny = l_ny;
  o_info.nTimeSteps = l_nTime;

  // global attributes
  float l_dx = 0, l_dy = 0, l_originX = 0, l_originY = 0;
  checkErr(nc_get_att_float(l_ncId, NC_GLOBAL, "dx", &l_dx));
  checkErr(nc_get_att_float(l_ncId, NC_GLOBAL, "dy", &l_dy));
  checkErr(nc_get_att_float(l_ncId, NC_GLOBAL, "origin_x", &l_originX));
  checkErr(nc_get_att_float(l_ncId, NC_GLOBAL, "origin_y", &l_originY));
  o_info.dxy = l_dx;
  o_info.originX = l_originX;
  o_info.originY = l_originY;
  (void)l_dy; // dx == dy by construction

  float l_endTime = 0, l_dt = 0;
  if (nc_get_att_float(l_ncId, NC_GLOBAL, "end_time", &l_endTime) == NC_NOERR)
    o_info.endTime = l_endTime;
  if (nc_get_att_float(l_ncId, NC_GLOBAL, "dt", &l_dt) == NC_NOERR)
    o_info.dt = l_dt;
  getAttStr(l_ncId, "bc_left", o_info.bcLeft);
  getAttStr(l_ncId, "bc_right", o_info.bcRight);
  getAttStr(l_ncId, "solver_mode", o_info.solverMode);
  getAttStr(l_ncId, "setup_mode", o_info.setupMode);

  // last sim time
  int l_varTime = -1;
  checkErr(nc_inq_varid(l_ncId, "time", &l_varTime));
  size_t l_lastIdx = l_nTime - 1;
  float l_lastTime = 0;
  checkErr(nc_get_var1_float(l_ncId, l_varTime, &l_lastIdx, &l_lastTime));
  o_info.lastSimTime = l_lastTime;

  // read last time slice of h, hu, hv and 2D b — all in stored (x, y) layout
  int l_varH = -1, l_varHu = -1, l_varHv = -1, l_varB = -1;
  checkErr(nc_inq_varid(l_ncId, "h", &l_varH));
  checkErr(nc_inq_varid(l_ncId, "hu", &l_varHu));
  checkErr(nc_inq_varid(l_ncId, "hv", &l_varHv));
  checkErr(nc_inq_varid(l_ncId, "b", &l_varB));

  size_t l_start3[3] = {l_lastIdx, 0, 0};
  size_t l_count3[3] = {1, l_nx, l_ny};

  std::vector<float> l_buf(l_nx * l_ny);

  auto fetch3 = [&](int i_var, t_real*& o_dst) {
    checkErr(nc_get_vara_float(l_ncId, i_var, l_start3, l_count3, l_buf.data()));
    o_dst = new t_real[l_nx * l_ny];
    // stored as (x, y) -> emit row-major (iy*nx + ix) for consumer convenience
    for (size_t l_ix = 0; l_ix < l_nx; l_ix++)
      for (size_t l_iy = 0; l_iy < l_ny; l_iy++)
        o_dst[l_iy * l_nx + l_ix] =
            static_cast<t_real>(l_buf[l_ix * l_ny + l_iy]);
  };

  fetch3(l_varH, o_h);
  fetch3(l_varHu, o_hu);
  fetch3(l_varHv, o_hv);

  // b is 2D (x, y)
  checkErr(nc_get_var_float(l_ncId, l_varB, l_buf.data()));
  o_b = new t_real[l_nx * l_ny];
  for (size_t l_ix = 0; l_ix < l_nx; l_ix++)
    for (size_t l_iy = 0; l_iy < l_ny; l_iy++)
      o_b[l_iy * l_nx + l_ix] =
          static_cast<t_real>(l_buf[l_ix * l_ny + l_iy]);

  checkErr(nc_close(l_ncId));
}

void NetCDF::read(const char* i_path,
                  const char* i_varName,
                  t_idx& o_nx,
                  t_idx& o_ny,
                  t_real*& o_x,
                  t_real*& o_y,
                  t_real*& o_z) {
  int l_ncId = -1;
  checkErr(nc_open(i_path, NC_NOWRITE, &l_ncId));

  // dimension ids and lengths
  int l_dimXId = -1, l_dimYId = -1;
  checkErr(nc_inq_dimid(l_ncId, "x", &l_dimXId));
  checkErr(nc_inq_dimid(l_ncId, "y", &l_dimYId));

  size_t l_lenX = 0, l_lenY = 0;
  checkErr(nc_inq_dimlen(l_ncId, l_dimXId, &l_lenX));
  checkErr(nc_inq_dimlen(l_ncId, l_dimYId, &l_lenY));
  o_nx = static_cast<t_idx>(l_lenX);
  o_ny = static_cast<t_idx>(l_lenY);

  // coordinate variables
  int l_varX = -1, l_varY = -1;
  checkErr(nc_inq_varid(l_ncId, "x", &l_varX));
  checkErr(nc_inq_varid(l_ncId, "y", &l_varY));

  std::vector<float> l_xBuf(l_lenX), l_yBuf(l_lenY);
  checkErr(nc_get_var_float(l_ncId, l_varX, l_xBuf.data()));
  checkErr(nc_get_var_float(l_ncId, l_varY, l_yBuf.data()));

  o_x = new t_real[l_lenX];
  o_y = new t_real[l_lenY];
  for (size_t l_i = 0; l_i < l_lenX; l_i++)
    o_x[l_i] = static_cast<t_real>(l_xBuf[l_i]);
  for (size_t l_j = 0; l_j < l_lenY; l_j++)
    o_y[l_j] = static_cast<t_real>(l_yBuf[l_j]);

  // data variable
  int l_varZ = -1;
  checkErr(nc_inq_varid(l_ncId, i_varName, &l_varZ));

  int l_nDims = 0;
  checkErr(nc_inq_varndims(l_ncId, l_varZ, &l_nDims));
  if (l_nDims != 2) {
    std::cerr << "NetCDF read: variable '" << i_varName
              << "' is not 2D (ndims=" << l_nDims << ")" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  int l_zDimIds[2] = {-1, -1};
  checkErr(nc_inq_vardimid(l_ncId, l_varZ, l_zDimIds));

  // load raw data
  std::vector<float> l_zBuf(l_lenX * l_lenY);
  checkErr(nc_get_var_float(l_ncId, l_varZ, l_zBuf.data()));

  o_z = new t_real[l_lenX * l_lenY];

  // determine layout: (y, x) is the COARDS-typical order; (x, y) is also
  // accepted. In both cases we emit row-major (y, x) in the output array.
  if (l_zDimIds[0] == l_dimYId && l_zDimIds[1] == l_dimXId) {
    // (y, x) -> already row-major (y, x)
    for (size_t l_iy = 0; l_iy < l_lenY; l_iy++)
      for (size_t l_ix = 0; l_ix < l_lenX; l_ix++)
        o_z[l_iy * l_lenX + l_ix] =
            static_cast<t_real>(l_zBuf[l_iy * l_lenX + l_ix]);
  } else if (l_zDimIds[0] == l_dimXId && l_zDimIds[1] == l_dimYId) {
    // (x, y) -> transpose to (y, x)
    for (size_t l_ix = 0; l_ix < l_lenX; l_ix++)
      for (size_t l_iy = 0; l_iy < l_lenY; l_iy++)
        o_z[l_iy * l_lenX + l_ix] =
            static_cast<t_real>(l_zBuf[l_ix * l_lenY + l_iy]);
  } else {
    std::cerr << "NetCDF read: variable '" << i_varName
              << "' has dimensions other than (x, y) or (y, x)" << std::endl;
    std::exit(EXIT_FAILURE);
  }

  checkErr(nc_close(l_ncId));
}

} // namespace io
} // namespace tsunami_lab