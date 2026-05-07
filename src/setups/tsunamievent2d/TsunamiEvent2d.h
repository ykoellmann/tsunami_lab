/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Two-dimensional tsunami event setup reading bathymetry and displacement
 * from NetCDF files (COARDS convention). Implements Eq. 5.2.1 with
 * nearest-neighbour grid lookup.
 **/
#ifndef TSUNAMI_LAB_SETUPS_TSUNAMI_EVENT_2D_H
#define TSUNAMI_LAB_SETUPS_TSUNAMI_EVENT_2D_H

#include "../../constants.h"
#include "../Setup.h"

namespace tsunami_lab {
namespace setups {
class TsunamiEvent2d;
} // namespace setups
} // namespace tsunami_lab

class tsunami_lab::setups::TsunamiEvent2d : public Setup {
private:
  // bathymetry grid
  t_idx m_nxBath = 0, m_nyBath = 0;
  t_real* m_xBath = nullptr; // x coords, length m_nxBath
  t_real* m_yBath = nullptr; // y coords, length m_nyBath
  t_real* m_bath = nullptr;  // values, row-major [iy*nxBath + ix]

  // displacement grid
  t_idx m_nxDispl = 0, m_nyDispl = 0;
  t_real* m_xDispl = nullptr;
  t_real* m_yDispl = nullptr;
  t_real* m_displ = nullptr; // values, row-major [iy*nxDispl + ix]

  t_real m_delta;

  /**
   * Returns the index in a sorted array whose value is closest to i_val.
   * Clamps to [0, i_n-1].
   **/
  static t_idx closestIdx(t_real const* i_arr, t_idx i_n, t_real i_val);

  /**
   * Returns the raw bathymetry at (i_x, i_y) via nearest-neighbour lookup.
   * Clamped to the grid boundary outside the domain.
   **/
  t_real getBathymetryRaw(t_real i_x, t_real i_y) const;

  /**
   * Returns the vertical displacement at (i_x, i_y).
   * Zero if the point lies outside the displacement grid.
   **/
  t_real getDisplacement(t_real i_x, t_real i_y) const;

public:
  /**
   * Reads bathymetry and displacement from NetCDF files and constructs the
   * setup.
   *
   * @param i_bathPath  path to the bathymetry NetCDF file.
   * @param i_displPath path to the displacement NetCDF file.
   * @param i_bathVar   name of the 2D bathymetry variable (default "z").
   * @param i_displVar  name of the 2D displacement variable (default "z").
   * @param i_delta     small constant to avoid dry cells (default 20 m).
   **/
  TsunamiEvent2d(const char* i_bathPath,
                 const char* i_displPath,
                 const char* i_bathVar = "z",
                 const char* i_displVar = "z",
                 t_real i_delta = 20);

  ~TsunamiEvent2d();

  /** Domain extent derived from the bathymetry grid. **/
  t_real getDomainOriginX() const { return m_xBath[0]; }
  t_real getDomainOriginY() const { return m_yBath[0]; }
  t_real getDomainSizeX() const { return m_xBath[m_nxBath - 1] - m_xBath[0]; }
  t_real getDomainSizeY() const { return m_yBath[m_nyBath - 1] - m_yBath[0]; }

  /**
   * Water height at (i_x, i_y) from Eq. 5.2.1.
   **/
  t_real getHeight(t_real i_x, t_real i_y) const;

  /** Momentum in x-direction (always 0). **/
  t_real getMomentumX(t_real, t_real) const;

  /** Momentum in y-direction (always 0). **/
  t_real getMomentumY(t_real, t_real) const;

  /**
   * Bathymetry at (i_x, i_y) from Eq. 5.2.1.
   **/
  t_real getBathymetry(t_real i_x, t_real i_y) const;
};

#endif // TSUNAMI_LAB_SETUPS_TSUNAMI_EVENT_2D_H
