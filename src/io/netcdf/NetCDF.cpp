/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * IO-routines for writing simulation data as NetCDF.
 **/
#include "NetCDF.h"

#include <cstdlib>
#include <iostream>
#include <vector>

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
  checkErr(nc_put_att_text(m_ncId, m_varTime, "units", 29,
                           "seconds since earthquake event"));
  checkErr(nc_put_att_text(m_ncId, m_varX, "units", 6, "meters"));
  checkErr(nc_put_att_text(m_ncId, m_varX, "axis", 1, "X"));
  checkErr(nc_put_att_text(m_ncId, m_varY, "units", 6, "meters"));
  checkErr(nc_put_att_text(m_ncId, m_varY, "axis", 1, "Y"));

  // data variables: [time, x, y]
  int l_dimsTxy[3] = {m_dimTime, m_dimX, m_dimY};
  int l_dimsXy[2] = {m_dimX, m_dimY};

  checkErr(nc_def_var(m_ncId, "h", NC_FLOAT, 3, l_dimsTxy, &m_varH));
  checkErr(nc_put_att_text(m_ncId, m_varH, "units", 6, "meters"));
  checkErr(nc_put_att_text(m_ncId, m_varH, "long_name", 12, "water height"));

  checkErr(nc_def_var(m_ncId, "hu", NC_FLOAT, 3, l_dimsTxy, &m_varHu));
  checkErr(nc_put_att_text(m_ncId, m_varHu, "units", 14, "meters^2/second"));
  checkErr(
      nc_put_att_text(m_ncId, m_varHu, "long_name", 19, "x-momentum (h * u)"));

  checkErr(nc_def_var(m_ncId, "hv", NC_FLOAT, 3, l_dimsTxy, &m_varHv));
  checkErr(nc_put_att_text(m_ncId, m_varHv, "units", 14, "meters^2/second"));
  checkErr(
      nc_put_att_text(m_ncId, m_varHv, "long_name", 19, "y-momentum (h * v)"));

  checkErr(nc_def_var(m_ncId, "b", NC_FLOAT, 2, l_dimsXy, &m_varB));
  checkErr(nc_put_att_text(m_ncId, m_varB, "units", 6, "meters"));
  checkErr(nc_put_att_text(m_ncId, m_varB, "long_name", 10, "bathymetry"));

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

} // namespace io
} // namespace tsunami_lab