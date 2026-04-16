/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * One-dimensional shock-shock problem.
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
 * 1d shock-shock setup.
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

public:
  /**
   * Constructor.
   *
   * @param i_height water height on both sides of the discontinuity.
   * @param i_momentum magnitude of initial momentum; left uses +, right uses -.
   * @param i_locationDis location (x-coordinate) of the initial discontinuity.
   **/
  ShockShock1d(t_real i_height, t_real i_momentum, t_real i_locationDis);

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
};

#endif