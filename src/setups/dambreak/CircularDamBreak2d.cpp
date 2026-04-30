/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Two-dimensional circular dam break problem (with optional bathymetric
 * obstacle).
 **/
#include "CircularDamBreak2d.h"
#include <cmath>

tsunami_lab::setups::CircularDamBreak2d::CircularDamBreak2d(
    t_real i_heightInner,
    t_real i_heightOuter,
    t_real i_centerX,
    t_real i_centerY,
    t_real i_radius,
    t_real i_obstacleAmplitude,
    t_real i_obstacleCenterX,
    t_real i_obstacleCenterY,
    t_real i_obstacleWidth) {
  m_heightInner = i_heightInner;
  m_heightOuter = i_heightOuter;
  m_centerX = i_centerX;
  m_centerY = i_centerY;
  m_radius = i_radius;
  m_obstacleAmplitude = i_obstacleAmplitude;
  m_obstacleCenterX = i_obstacleCenterX;
  m_obstacleCenterY = i_obstacleCenterY;
  m_obstacleWidth = i_obstacleWidth;
}

tsunami_lab::t_real
tsunami_lab::setups::CircularDamBreak2d::getHeight(t_real i_x,
                                                   t_real i_y) const {
  t_real l_dx = i_x - m_centerX;
  t_real l_dy = i_y - m_centerY;
  if (l_dx * l_dx + l_dy * l_dy < m_radius * m_radius) {
    return m_heightInner;
  } else {
    return m_heightOuter;
  }
}

tsunami_lab::t_real
tsunami_lab::setups::CircularDamBreak2d::getMomentumX(t_real, t_real) const {
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::CircularDamBreak2d::getMomentumY(t_real, t_real) const {
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::CircularDamBreak2d::getBathymetry(t_real i_x,
                                                       t_real i_y) const {
  if (m_obstacleAmplitude == 0 || m_obstacleWidth == 0) {
    // flat bathymetry
    return 0;
  }
  // isotropic 2D Gaussian obstacle centered at (m_obstacleCenterX,
  // m_obstacleCenterY); amplitude controls height, width controls spread
  t_real l_dx = i_x - m_obstacleCenterX;
  t_real l_dy = i_y - m_obstacleCenterY;
  t_real l_exponent =
      -(l_dx * l_dx + l_dy * l_dy) / (2.0f * m_obstacleWidth * m_obstacleWidth);
  return m_obstacleAmplitude * std::exp(l_exponent);
}
