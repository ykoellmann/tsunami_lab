/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 * @section DESCRIPTION
 * Two-dimensional wave propagation patch.
 **/

#ifndef TSUNAMI_LAB_PATCHES_WAVE_PROPAGATION_2D
#define TSUNAMI_LAB_PATCHES_WAVE_PROPAGATION_2D

#include <string>
#include <vector>

#include "../WavePropagation.h"

namespace tsunami_lab {
namespace patches {
class WavePropagation2d;
}
} // namespace tsunami_lab

class tsunami_lab::patches::WavePropagation2d : public WavePropagation {
private:
  //! current step that indicates the active values in the arrays below
  unsigned short m_step = 0;

  //! number of cells in x-direction discretizing the computational domain
  t_idx m_nCells_x = 0;

  //! number of cells in y-direction discretizing the computational domain
  t_idx m_nCells_y = 0;

  //! water heights for the current and next time step for all cells
  t_real* m_h = {nullptr};

  //! momenta for the current and next time step for all cells in x-direction
  t_real* m_hu = {nullptr};

  //! momenta for the current and next time step for all cells in y-direction
  t_real* m_hv = {nullptr};

  //! bathymetry for all cells
  t_real* m_b = nullptr;

  /**
   * Get the 2d Coordinates of the 1d array (x-y grid is being made flat into
   * one line)
   *
   * @param i_x x-coordinate
   * @param i_y y-coordinate
   * @return t_idx index in 1d-array
   */
  t_idx getCoordinates(t_idx i_x, t_idx i_y) {
    return i_x + i_y * getStride();
  };

public:
  /**
   *
   * Constructs the 2d wave propagation solver.
   *
   * @param i_nCells_x number of cells in x-direction.
   * @param i_nCells_y number of cells in y-direction.
   **/
  WavePropagation2d(t_idx i_nCells_x, t_idx i_nCells_y);

  /**
   * Destructor that frees all allocated memory.
   **/
  ~WavePropagation2d();

  /**
   * Performs a time step.
   *
   * @param i_scaling scaling of the time step (dt / dx).
   **/
  void timeStep(t_real i_scaling, std::string i_mode);

  /**
   * Sets the values of the ghost cells according to outflow boundary
   * conditions.
   **/
  void setGhostOutflow();

  /**
   * Sets the ghost cell values using independent boundary conditions on the
   * upper, lower, left and right side of the domain.
   *
   * For a reflecting boundary the ghost cell mirrors the adjacent interior
   * cell: water height (and bathymetry) are copied while the momentum is
   * negated so that the particle velocity at the boundary vanishes.
   *
   * @param i_left  boundary condition at the left side.
   * @param i_right boundary condition at the right side.
   * @param i_upper boundary condition at the upper side.
   * @param i_lower boundary condition at the lower side.
   **/
  void setGhost(BoundaryCondition i_left, BoundaryCondition i_right) {
    setGhost(i_left, i_right, BoundaryCondition::Outflow,
             BoundaryCondition::Outflow);
  }

  void setGhost(BoundaryCondition i_left,
                BoundaryCondition i_right,
                BoundaryCondition i_upper,
                BoundaryCondition i_lower);

  /**
   * Gets the stride in y-direction. x-direction is stride-1.
   *
   * @return stride in y-direction.
   **/
  t_idx getStride() { return m_nCells_y + 2; }

  /**
   * Gets cells' water heights.
   *
   * @return water heights.
   */
  t_real const* getHeight() {
    return m_h + m_step * (m_nCells_x + 2) * getStride() + getStride() + 1;
  }

  /**
   * Gets the cells' momenta in x-direction.
   *
   * @return momenta in x-direction.
   **/
  t_real const* getMomentumX() {
    return m_hu + m_step * (m_nCells_x + 2) * getStride() + getStride() + 1;
  }

  /**
   * Gets the cells' momenta in y-direction.
   *
   * @return momenta in y-direction.
   **/
  t_real const* getMomentumY() {
    return m_hv + m_step * (m_nCells_x + 2) * getStride() + getStride() + 1;
  }

  /**
   * Gets the cells' bathymetry.
   *
   * @return bathymetry.
   **/
  t_real const* getBathymetry() { return m_b + getStride() + 1; }

  /**
   * Sets the height of the cell to the given value.
   *
   * @param i_ix id of the cell in x-direction.
   * @param i_iy id of the cell in y-direction.
   * @param i_h water height.
   **/
  void setHeight(t_idx i_ix, t_idx i_iy, t_real i_h) {
    m_h[getCoordinates(i_ix + 1, i_iy + 1)] = i_h;
  }

  /**
   * Sets the momentum in x-direction to the given value.
   *
   * @param i_ix id of the cell in x-direction.
   * @param i_iy id of the cell in y-direction.
   * @param i_hu momentum in x-direction.
   **/
  void setMomentumX(t_idx i_ix, t_idx i_iy, t_real i_hu) {
    m_hu[getCoordinates(i_ix + 1, i_iy + 1)] = i_hu;
  }

  /**
   * Sets the momentum in y-direction to the given value.
   *
   * @param i_ix id of the cell in x-direction.
   * @param i_iy id of the cell in y-direction.
   * @param i_hu momentum in y-direction.
   **/
  void setMomentumY(t_idx i_ix, t_idx i_iy, t_real i_hv) {
    m_hv[getCoordinates(i_ix + 1, i_iy + 1)] = i_hv;
  }

  /**
   *   Set the bathymetry
   *
   * @param i_ix id of the cell in x-direction.
   * @param i_iy id of the cell in y-direction.
   * @param i_b bathymetry
   */
  void setBathymetry(t_idx i_ix, t_idx i_iy, t_real i_b) {
    m_b[getCoordinates(i_ix + 1, i_iy + 1)] = i_b;
  }
};

#endif
