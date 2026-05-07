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
  // Eq. 5.2.1: d(x,y) = 5 * f(x) * g(y) on [-500, 500]^2, else 0.
  if (i_x < -500 || i_x > 500 || i_y < -500 || i_y > 500)
    return 0;
  constexpr t_real l_pi = static_cast<t_real>(3.14159265358979323846);
  t_real l_f = std::sin((i_x / 500.0f + 1.0f) * l_pi);
  t_real l_g = -(i_y / 500.0f) * (i_y / 500.0f) + 1.0f;
  return 5.0f * l_f * l_g;
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
