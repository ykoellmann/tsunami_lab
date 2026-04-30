/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Two-dimensional circular dam break problem (with optional bathymetric
 * obstacle).
 **/
#ifndef TSUNAMI_LAB_SETUPS_CIRCULAR_DAM_BREAK_2D_H
#define TSUNAMI_LAB_SETUPS_CIRCULAR_DAM_BREAK_2D_H

#include "../Setup.h"

namespace tsunami_lab {
namespace setups {
class CircularDamBreak2d;
}
} // namespace tsunami_lab

/**
 * 2d circular dam break setup.
 *
 * Initial water height is i_heightInner inside a circle of radius i_radius
 * around (i_centerX, i_centerY) and i_heightOuter outside. Momentum is zero
 * everywhere. The bathymetry is flat by default; an optional isotropic 2D
 * Gaussian obstacle can be added on top.
 **/
class tsunami_lab::setups::CircularDamBreak2d : public Setup {
private:
  //! water height inside the circle
  t_real m_heightInner = 0;
  //! water height outside the circle
  t_real m_heightOuter = 0;
  //! x-coordinate of the circle's center
  t_real m_centerX = 0;
  //! y-coordinate of the circle's center
  t_real m_centerY = 0;
  //! radius of the circle
  t_real m_radius = 0;

  //! amplitude of the bathymetric obstacle (positive = hump, negative = pit)
  t_real m_obstacleAmplitude = 0;
  //! x-coordinate of the obstacle's center
  t_real m_obstacleCenterX = 0;
  //! y-coordinate of the obstacle's center
  t_real m_obstacleCenterY = 0;
  //! width (Gaussian standard deviation) of the obstacle
  t_real m_obstacleWidth = 0;

public:
  /**
   * Constructor.
   *
   * @param i_heightInner water height inside the circle.
   * @param i_heightOuter water height outside the circle.
   * @param i_centerX x-coordinate of the circle's center.
   * @param i_centerY y-coordinate of the circle's center.
   * @param i_radius radius of the circle.
   * @param i_obstacleAmplitude amplitude of the optional bathymetric obstacle
   *                            (default: 0 for flat bathymetry).
   * @param i_obstacleCenterX x-coordinate of the obstacle's center
   *                          (default: 0).
   * @param i_obstacleCenterY y-coordinate of the obstacle's center
   *                          (default: 0).
   * @param i_obstacleWidth width of the obstacle (Gaussian std dev,
   *                       default: 0 for flat bathymetry).
   **/
  CircularDamBreak2d(t_real i_heightInner,
                     t_real i_heightOuter,
                     t_real i_centerX,
                     t_real i_centerY,
                     t_real i_radius,
                     t_real i_obstacleAmplitude = 0,
                     t_real i_obstacleCenterX = 0,
                     t_real i_obstacleCenterY = 0,
                     t_real i_obstacleWidth = 0);

  /**
   * Gets the water height at a given point.
   *
   * @param i_x x-coordinate of the queried point.
   * @param i_y y-coordinate of the queried point.
   * @return height at the given point.
   **/
  t_real getHeight(t_real i_x, t_real i_y) const;

  /**
   * Gets the momentum in x-direction.
   *
   * @return momentum in x-direction.
   **/
  t_real getMomentumX(t_real, t_real) const;

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
   * @param i_y y-coordinate of the queried point.
   * @return bathymetry value.
   **/
  t_real getBathymetry(t_real i_x, t_real i_y) const;
};

#endif
