/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * One-dimensional shock-shock problem (with optional bathymetry variant).
 **/
#include "ShockShock1d.h"
#include <cmath>

tsunami_lab::setups::ShockShock1d::ShockShock1d(t_real i_height,
                                                t_real i_momentum,
                                                t_real i_locationDis,
                                                t_real i_bathymetryAmplitude,
                                                t_real i_bathymetryCenterX,
                                                t_real i_bathymetryWidth) {
  m_height = i_height;
  m_momentum = i_momentum;
  m_locationDis = i_locationDis;
  m_bathymetryAmplitude = i_bathymetryAmplitude;
  m_bathymetryCenterX = i_bathymetryCenterX;
  m_bathymetryWidth = i_bathymetryWidth;
}

tsunami_lab::t_real tsunami_lab::setups::ShockShock1d::getHeight(t_real,
                                                                 t_real) const {
  return m_height;
}

tsunami_lab::t_real
tsunami_lab::setups::ShockShock1d::getMomentumX(t_real i_x, t_real) const {
  if (i_x <= m_locationDis) {
    return m_momentum;
  } else {
    return -m_momentum;
  }
}

tsunami_lab::t_real
tsunami_lab::setups::ShockShock1d::getMomentumY(t_real, t_real) const {
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::ShockShock1d::getBathymetry(t_real i_x, t_real) const {

  if (m_bathymetryAmplitude == 0 || m_bathymetryWidth == 0) {
    // flat bathymetry
    return 0;
  }
  // Gaussian profile centered at m_bathymetryCenterX
  // Amplitude controls height, width controls spread
  t_real l_dx = i_x - m_bathymetryCenterX;
  t_real l_exponent =
      -(l_dx * l_dx) / (2.0f * m_bathymetryWidth * m_bathymetryWidth);

  return m_bathymetryAmplitude * std::exp(l_exponent);
}
