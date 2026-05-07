/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Two-dimensional artificial tsunami event setup with hard-coded bathymetry
 * and displacement (no file input). Implements Eq. 5.2.1.
 **/
#ifndef TSUNAMI_LAB_SETUPS_ARTIFICIAL_TSUNAMI_2D_H
#define TSUNAMI_LAB_SETUPS_ARTIFICIAL_TSUNAMI_2D_H

#include "../../constants.h"
#include "../Setup.h"

namespace tsunami_lab {
namespace setups {
class ArtificialTsunami2d;
} // namespace setups
} // namespace tsunami_lab

/**
 * 2D artificial tsunami setup.
 *
 * Bathymetry is a flat ocean floor at -100 m everywhere.
 * The vertical displacement is a Gaussian centered at the origin with
 * amplitude 5 m and standard deviation 25 km, mimicking a submarine
 * earthquake. Initial water height and bathymetry follow Eq. 5.2.1.
 **/
class tsunami_lab::setups::ArtificialTsunami2d : public Setup {
private:
  //! small constant to avoid dry cells (m)
  t_real m_delta;

  /**
   * Computes the hard-coded raw bathymetry b_in at (x, y).
   *
   * @return -100 m (flat ocean floor).
   **/
  t_real getBathymetryRaw(t_real i_x, t_real i_y) const;

  /**
   * Computes the vertical displacement at (x, y).
   * Gaussian with amplitude 5 m and sigma = 25 km centred at the origin.
   *
   * @param i_x x-coordinate [m].
   * @param i_y y-coordinate [m].
   * @return displacement [m].
   **/
  t_real getDisplacement(t_real i_x, t_real i_y) const;

public:
  /**
   * Constructor.
   *
   * @param i_delta small constant to avoid dry cells (default: 20 m).
   **/
  ArtificialTsunami2d(t_real i_delta = 20);

  /**
   * Gets the water height at (x, y) using Eq. 5.2.1.
   *
   * @param i_x x-coordinate [m].
   * @param i_y y-coordinate [m].
   * @return water height [m].
   **/
  t_real getHeight(t_real i_x, t_real i_y) const;

  /**
   * Gets the momentum in x-direction (always 0).
   **/
  t_real getMomentumX(t_real, t_real) const;

  /**
   * Gets the momentum in y-direction (always 0).
   **/
  t_real getMomentumY(t_real, t_real) const;

  /**
   * Gets the bathymetry at (x, y) using Eq. 5.2.1.
   *
   * @param i_x x-coordinate [m].
   * @param i_y y-coordinate [m].
   * @return bathymetry [m].
   **/
  t_real getBathymetry(t_real i_x, t_real i_y) const;
};

#endif // TSUNAMI_LAB_SETUPS_ARTIFICIAL_TSUNAMI_2D_H
