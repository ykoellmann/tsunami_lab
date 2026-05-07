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
        l_buf[l_ix * m_ny + l_iy] = i_src[l_ix * i_stride + l_iy];
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