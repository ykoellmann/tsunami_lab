/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Two-dimensional tsunami event setup reading bathymetry and displacement
 * from NetCDF files (COARDS convention). Implements Eq. 5.2.1 with
 * nearest-neighbour grid lookup.
 **/
#include "TsunamiEvent2d.h"
#include "../../io/netcdf/NetCDF.h"
#include <algorithm>

tsunami_lab::t_idx tsunami_lab::setups::TsunamiEvent2d::closestIdx(
    t_real const* i_arr, t_idx i_n, t_real i_val) {
  if (i_val <= i_arr[0])
    return 0;
  if (i_val >= i_arr[i_n - 1])
    return i_n - 1;

  // binary search: find largest index where arr[lo] <= val
  t_idx l_lo = 0, l_hi = i_n - 1;
  while (l_hi - l_lo > 1) {
    t_idx l_mid = (l_lo + l_hi) / 2;
    if (i_arr[l_mid] <= i_val)
      l_lo = l_mid;
    else
      l_hi = l_mid;
  }
  return (i_val - i_arr[l_lo]) <= (i_arr[l_hi] - i_val) ? l_lo : l_hi;
}

tsunami_lab::setups::TsunamiEvent2d::TsunamiEvent2d(const char* i_bathPath,
                                                    const char* i_displPath,
                                                    const char* i_bathVar,
                                                    const char* i_displVar,
                                                    t_real i_delta)
    : m_delta(i_delta) {
  io::NetCDF::read(i_bathPath, i_bathVar, m_nxBath, m_nyBath, m_xBath, m_yBath,
                   m_bath);
  io::NetCDF::read(i_displPath, i_displVar, m_nxDispl, m_nyDispl, m_xDispl,
                   m_yDispl, m_displ);
}

tsunami_lab::setups::TsunamiEvent2d::~TsunamiEvent2d() {
  delete[] m_xBath;
  delete[] m_yBath;
  delete[] m_bath;
  delete[] m_xDispl;
  delete[] m_yDispl;
  delete[] m_displ;
}

tsunami_lab::t_real
tsunami_lab::setups::TsunamiEvent2d::getBathymetryRaw(t_real i_x,
                                                      t_real i_y) const {
  t_idx l_ix = closestIdx(m_xBath, m_nxBath, i_x);
  t_idx l_iy = closestIdx(m_yBath, m_nyBath, i_y);
  return m_bath[l_iy * m_nxBath + l_ix];
}

tsunami_lab::t_real
tsunami_lab::setups::TsunamiEvent2d::getDisplacement(t_real i_x,
                                                     t_real i_y) const {
  // zero outside the displacement grid extent
  if (i_x < m_xDispl[0] || i_x > m_xDispl[m_nxDispl - 1] || i_y < m_yDispl[0] ||
      i_y > m_yDispl[m_nyDispl - 1])
    return 0;

  t_idx l_ix = closestIdx(m_xDispl, m_nxDispl, i_x);
  t_idx l_iy = closestIdx(m_yDispl, m_nyDispl, i_y);
  return m_displ[l_iy * m_nxDispl + l_ix];
}

tsunami_lab::t_real
tsunami_lab::setups::TsunamiEvent2d::getHeight(t_real i_x, t_real i_y) const {
  t_real l_bIn = getBathymetryRaw(i_x, i_y);
  if (l_bIn < 0)
    return std::max(-l_bIn, m_delta);
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::TsunamiEvent2d::getMomentumX(t_real, t_real) const {
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::TsunamiEvent2d::getMomentumY(t_real, t_real) const {
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::TsunamiEvent2d::getBathymetry(t_real i_x,
                                                   t_real i_y) const {
  t_real l_bIn = getBathymetryRaw(i_x, i_y);
  t_real l_d = getDisplacement(i_x, i_y);
  if (l_bIn < 0)
    return std::min(l_bIn, -m_delta) + l_d;
  return std::max(l_bIn, m_delta) + l_d;
}
