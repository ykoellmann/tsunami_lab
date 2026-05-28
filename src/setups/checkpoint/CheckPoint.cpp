/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Setup that restores its state from a netCDF checkpoint file's last
 * stored time step.
 **/
#include "CheckPoint.h"

#include <cmath>

namespace tsunami_lab {
namespace setups {

CheckPoint::CheckPoint(const char* i_path) {
  io::NetCDF::readCheckpoint(i_path, m_info, m_h, m_hu, m_hv, m_b);
}

CheckPoint::~CheckPoint() {
  delete[] m_h;
  delete[] m_hu;
  delete[] m_hv;
  delete[] m_b;
}

t_idx CheckPoint::cellIndex(t_real i_x, t_real i_y) const {
  // map point to cell index assuming centers at originX + (ix + 0.5) * dxy
  long l_ix =
      static_cast<long>(std::floor((i_x - m_info.originX) / m_info.dxy));
  long l_iy =
      static_cast<long>(std::floor((i_y - m_info.originY) / m_info.dxy));

  if (l_ix < 0)
    l_ix = 0;
  if (l_iy < 0)
    l_iy = 0;
  if (static_cast<t_idx>(l_ix) >= m_info.nx)
    l_ix = static_cast<long>(m_info.nx) - 1;
  if (static_cast<t_idx>(l_iy) >= m_info.ny)
    l_iy = static_cast<long>(m_info.ny) - 1;

  return static_cast<t_idx>(l_iy) * m_info.nx + static_cast<t_idx>(l_ix);
}

t_real CheckPoint::getHeight(t_real i_x, t_real i_y) const {
  return m_h[cellIndex(i_x, i_y)];
}

t_real CheckPoint::getMomentumX(t_real i_x, t_real i_y) const {
  return m_hu[cellIndex(i_x, i_y)];
}

t_real CheckPoint::getMomentumY(t_real i_x, t_real i_y) const {
  return m_hv[cellIndex(i_x, i_y)];
}

t_real CheckPoint::getBathymetry(t_real i_x, t_real i_y) const {
  return m_b[cellIndex(i_x, i_y)];
}

} // namespace setups
} // namespace tsunami_lab
