#include "GaussianDisplacement.h"

#include <algorithm>
#include <cmath>

namespace tsunami_lab {
namespace displacement {

GaussianDisplacement::GaussianDisplacement(double i_amplitude, double i_sigma)
    : m_amplitude(i_amplitude), m_sigma(std::max(i_sigma, 1e-6)) {}

double GaussianDisplacement::verticalDisplacement(double i_east,
                                                  double i_north) const {
  double l_r2 = i_east * i_east + i_north * i_north;
  return m_amplitude * std::exp(-l_r2 / (2.0 * m_sigma * m_sigma));
}

double GaussianDisplacement::influenceRadius() const { return 3.0 * m_sigma; }

} // namespace displacement
} // namespace tsunami_lab