#ifndef TSUNAMI_LAB_DISPLACEMENT_GAUSSIANDISPLACEMENT_H
#define TSUNAMI_LAB_DISPLACEMENT_GAUSSIANDISPLACEMENT_H

#include "DisplacementModel.h"

namespace tsunami_lab {
namespace displacement {

// Radially symmetric Gaussian uplift:
//   d(r) = amplitude * exp(-r^2 / (2 * sigma^2)), r = sqrt(east^2 + north^2)
// First simple approximation of co-seismic deformation, to be superseded by an
// Okada model.
class GaussianDisplacement : public DisplacementModel {
public:
  // i_amplitude: peak vertical displacement at the centre (metres, signed).
  // i_sigma:     characteristic radius (metres), clamped to a small positive
  //              value.
  GaussianDisplacement(double i_amplitude, double i_sigma);

  double verticalDisplacement(double i_east, double i_north) const override;

  double influenceRadius() const override;

private:
  double m_amplitude;
  double m_sigma;
};

} // namespace displacement
} // namespace tsunami_lab

#endif
