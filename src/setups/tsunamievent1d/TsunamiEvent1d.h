/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * One-dimensional tsunami event setup using bathymetry data from CSV.
 **/

#ifndef TSUNAMI_LAB_TSUNAMIEVENT1D_H
#define TSUNAMI_LAB_TSUNAMIEVENT1D_H

#include "../../constants.h"
#include "../Setup.h"

namespace tsunami_lab {
namespace setups {
class TsunamiEvent1d;
}
} // namespace tsunami_lab

/**
 * 1d tsunami event setup.
 * Reads bathymetry from a CSV file and applies Eq. 3.4.1.
 **/
class tsunami_lab::setups::TsunamiEvent1d : public Setup {

private:
  //! number of bathymetry samples
  t_idx m_nSamples;

  //! sample positions in metres
  t_real* m_x;

  //! raw bathymetry values
  t_real* m_b;

  //! small constant to avoid dry cells (default: 20m)
  t_real m_delta;

  /**
   * Looks up the raw bathymetry value at position i_x
   * using linear interpolation between CSV data points.
   *
   * @param i_x position in metres.
   * @return interpolated raw bathymetry value.
   **/
  t_real getBathymetryRaw(t_real i_x) const;

  /**
   * Computes the vertical displacement at position i_x.
   *
   * @param i_x position in metres.
   * @return displacement value.
   **/
  t_real getDisplacement(t_real i_x) const;

public:
  /**
   * Constructor.
   *
   * @param i_filePath path to the bathymetry CSV file.
   * @param i_delta small constant to avoid dry cells (default: 20).
   **/
  TsunamiEvent1d(const char* i_filePath, t_real i_delta = 20);

  /**
   * Destructor. Frees bathymetry data arrays.
   **/
  ~TsunamiEvent1d();

  /**
   * Gets the water height at the given point.
   *
   * @param i_x x-coordinate of the queried point.
   * @return water height.
   **/
  t_real getHeight(t_real i_x, t_real) const;

  /**
   * Gets the momentum in x-direction.
   *
   * @return momentum in x-direction (always 0).
   **/
  t_real getMomentumX(t_real, t_real) const;

  /**
   * Gets the momentum in y-direction.
   *
   * @return momentum in y-direction (always 0).
   **/
  t_real getMomentumY(t_real, t_real) const;

  /**
   * Gets the bathymetry at the given point.
   *
   * @param i_x x-coordinate of the queried point.
   * @return bathymetry value.
   **/
  t_real getBathymetry(t_real i_x, t_real) const;
};

#endif // TSUNAMI_LAB_TSUNAMIEVENT1D_H
