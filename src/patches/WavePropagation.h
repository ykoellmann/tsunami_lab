/**
 * @author Alexander Breuer (alex.breuer AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Base class of the wave propagation patches.
 **/
#ifndef TSUNAMI_LAB_PATCHES_WAVE_PROPAGATION
#define TSUNAMI_LAB_PATCHES_WAVE_PROPAGATION

#include "../constants.h"

namespace tsunami_lab {
namespace patches {
class WavePropagation;

//! Boundary condition flag passed to setGhost() for the left/right side.
enum class BoundaryCondition { Outflow, Reflecting };
} // namespace patches
} // namespace tsunami_lab

class tsunami_lab::patches::WavePropagation {
public:
  /**
   * Virtual destructor for base class.
   **/
  virtual ~WavePropagation() {}

  /**
   * Performs a time step.
   *
   * @param i_scaling scaling of the time step.
   **/
  virtual void timeStep(t_real i_scalingm, std::string i_mode) = 0;

  /**
   * Sets the values of the ghost cells according to outflow boundary
   * conditions.
   **/
  virtual void setGhostOutflow() = 0;

  /**
   * Sets the ghost cell values using independent boundary conditions on the
   * left and right side of the domain.
   *
   * For a reflecting boundary the ghost cell mirrors the adjacent interior
   * cell: water height (and bathymetry) are copied while the momentum is
   * negated so that the particle velocity at the boundary vanishes.
   *
   * @param i_left  boundary condition at the left side.
   * @param i_right boundary condition at the right side.
   **/
  virtual void setGhost(BoundaryCondition i_left,
                        BoundaryCondition i_right) = 0;

  /**
   * Gets the stride in y-direction. x-direction is stride-1.
   *
   * @return stride in y-direction.
   **/
  virtual t_idx getStride() = 0;

  /**
   * Gets cells' water heights.
   *
   * @return water heights.
   */
  virtual t_real const* getHeight() = 0;

  /**
   * Gets the cells' momenta in x-direction.
   *
   * @return momenta in x-direction.
   **/
  virtual t_real const* getMomentumX() = 0;

  /**
   * Gets the cells' momenta in y-direction.
   *
   * @return momenta in y-direction.
   **/
  virtual t_real const* getMomentumY() = 0;

  /**
   *
   * Gets the cells' bathymetry.
   *
   * @return bathymetry
   */
  virtual t_real const* getBathymetry() = 0;

  /**
   * Sets the height of the cell to the given value.
   *
   * @param i_ix id of the cell in x-direction.
   * @param i_iy id of the cell in y-direction.
   * @param i_h water height.
   **/
  virtual void setHeight(t_idx i_ix, t_idx i_iy, t_real i_h) = 0;

  /**
   * Sets the momentum in x-direction to the given value.
   *
   * @param i_ix id of the cell in x-direction.
   * @param i_iy id of the cell in y-direction.
   * @param i_hu momentum in x-direction.
   **/
  virtual void setMomentumX(t_idx i_ix, t_idx i_iy, t_real i_hu) = 0;

  /**
   * Sets the momentum in y-direction to the given value.
   *
   * @param i_ix id of the cell in x-direction.
   * @param i_iy id of the cell in y-direction.
   * @param i_hv momentum in y-direction.
   **/
  virtual void setMomentumY(t_idx i_ix, t_idx i_iy, t_real i_hv) = 0;

  /**
   * @brief Sets the Bathymetry
   *
   * @param i_ix id of the cell in x-direction.
   * @param i_iy id of the cell in y-direction.
   * @param i_b bathymetry
   */
  virtual void setBathymetry(t_idx i_ix, t_idx i_iy, t_real i_b) = 0;
};

#endif
