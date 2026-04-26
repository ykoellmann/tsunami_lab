/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * One-dimensional shock-shock problem (with optional bathymetry variant).
 **/
#ifndef TSUNAMI_LAB_SETUPS_SHOCK_SHOCK_1D_H
#define TSUNAMI_LAB_SETUPS_SHOCK_SHOCK_1D_H

#include "../Setup.h"

namespace tsunami_lab {
namespace setups {
class ShockShock1d;
}
} // namespace tsunami_lab

/**
 * 1d shock-shock setup (flat bathymetry).
 **/
class tsunami_lab::setups::ShockShock1d : public Setup {
private:
  //! water height on both sides
  t_real m_height = 0;

  //! magnitude of momentum; left side flows right (+), right side flows left
  //! (-)
  t_real m_momentum = 0;

  //! location of the initial discontinuity
  t_real m_locationDis = 0;

  //! amplitude of the bathymetric feature (positive = hump, negative = valley)
  t_real m_bathymetryAmplitude = 0;

  //! center location of the bathymetric feature
  t_real m_bathymetryCenterX = 0;

  //! width (standard deviation) of the bathymetric feature
  t_real m_bathymetryWidth = 0;

public:
  /**
   * Constructor.
   *
   * @param i_height water height on both sides of the discontinuity.
   * @param i_momentum magnitude of initial momentum; left uses +, right uses -.
   * @param i_locationDis location (x-coordinate) of the initial discontinuity.
   * @param i_bathymetryAmplitude amplitude of bathymetric feature (default: 0
   * for flat).
   * @param i_bathymetryCenterX x-coordinate of bathymetry center (default: 0
   * for flat).
   * @param i_bathymetryWidth width of bathymetric feature (Gaussian std dev,
   * default: 0 for flat).
   **/
  ShockShock1d(t_real i_height,
               t_real i_momentum,
               t_real i_locationDis,
               t_real i_bathymetryAmplitude,
               t_real i_bathymetryCenterX,
               t_real i_bathymetryWidth);
  /**
   * Gets the water height at a given point.
   *
   * @param i_x x-coordinate of the queried point.
   * @return height at the given point.
   **/
  t_real getHeight(t_real i_x, t_real) const;

  /**
   * Gets the momentum in x-direction.
   *
   * @param i_x x-coordinate of the queried point.
   * @return momentum in x-direction.
   **/
  t_real getMomentumX(t_real i_x, t_real) const;

  /**
   * Gets the momentum in y-direction.
   *
   * @return momentum in y-direction.
   **/
  t_real getMomentumY(t_real, t_real) const;

  /**
   * Gets the bathymetry data at a given point.
   *
   * @param i_x x-coordinate of the queried point.
   *
   * @return bathymetry value
   **/
  t_real getBathymetry(t_real i_x, t_real) const;
};

#endif
