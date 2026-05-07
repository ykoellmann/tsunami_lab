/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Two-dimensional artificial tsunami event setup with hard-coded bathymetry
 * and displacement (no file input). Implements Eq. 5.2.1.
 **/
#include "ArtificialTsunami2d.h"
#include <algorithm>
#include <cmath>

tsunami_lab::setups::ArtificialTsunami2d::ArtificialTsunami2d(t_real i_delta)
    : m_delta(i_delta) {}

tsunami_lab::t_real
tsunami_lab::setups::ArtificialTsunami2d::getBathymetryRaw(t_real,
                                                            t_real) const {
  return -100.0f;
}

tsunami_lab::t_real
tsunami_lab::setups::ArtificialTsunami2d::getDisplacement(t_real i_x,
                                                           t_real i_y) const {
  // Gaussian: amplitude 5 m, sigma = 25 km
  constexpr t_real l_amp = 5.0f;
  constexpr t_real l_sigma = 25000.0f;
  return l_amp *
         std::exp(-(i_x * i_x + i_y * i_y) / (2.0f * l_sigma * l_sigma));
}

tsunami_lab::t_real
tsunami_lab::setups::ArtificialTsunami2d::getHeight(t_real i_x,
                                                     t_real i_y) const {
  t_real l_bIn = getBathymetryRaw(i_x, i_y);
  if (l_bIn < 0) {
    return std::max(-l_bIn, m_delta);
  }
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::ArtificialTsunami2d::getMomentumX(t_real, t_real) const {
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::ArtificialTsunami2d::getMomentumY(t_real, t_real) const {
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::ArtificialTsunami2d::getBathymetry(t_real i_x,
                                                         t_real i_y) const {
  t_real l_bIn = getBathymetryRaw(i_x, i_y);
  t_real l_d = getDisplacement(i_x, i_y);
  if (l_bIn < 0) {
    return std::min(l_bIn, -m_delta) + l_d;
  }
  return std::max(l_bIn, m_delta) + l_d;
}
