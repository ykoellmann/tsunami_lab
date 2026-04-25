/**
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * One-dimensional tsunami event setup using bathymetry data from CSV.
 **/
#include "TsunamiEvent1d.h"
#include "../../io/Csv.h"
#include <algorithm>
#include <cmath>
#include <fstream>

tsunami_lab::setups::TsunamiEvent1d::TsunamiEvent1d(const char* i_filePath,
                                                    t_real i_delta)
    : m_nSamples(0), m_x(nullptr), m_b(nullptr), m_delta(i_delta) {
  std::ifstream l_file(i_filePath);
  if (!l_file.is_open()) {
    std::cerr << "error: could not open bathymetry file: " << i_filePath
              << std::endl;
    return;
  }

  io::Csv::read(l_file, m_nSamples, m_x, m_b);
  l_file.close();
}

tsunami_lab::setups::TsunamiEvent1d::~TsunamiEvent1d() {
  delete[] m_x;
  delete[] m_b;
}

tsunami_lab::t_real
tsunami_lab::setups::TsunamiEvent1d::getBathymetryRaw(t_real i_x) const {
  // clamp to data range
  if (i_x <= m_x[0]) {
    return m_b[0];
  }
  if (i_x >= m_x[m_nSamples - 1]) {
    return m_b[m_nSamples - 1];
  }

  // find interval: largest index where m_x[l_idx] <= i_x
  t_idx l_idx = 0;
  for (t_idx l_i = 1; l_i < m_nSamples; l_i++) {
    if (m_x[l_i] > i_x) {
      break;
    }
    l_idx = l_i;
  }

  // linear interpolation
  t_real l_t = (i_x - m_x[l_idx]) / (m_x[l_idx + 1] - m_x[l_idx]);
  return m_b[l_idx] + l_t * (m_b[l_idx + 1] - m_b[l_idx]);
}

tsunami_lab::t_real
tsunami_lab::setups::TsunamiEvent1d::getDisplacement(t_real i_x) const {
  if (i_x > 175000 && i_x < 250000) {
    return 10 * std::sin((i_x - 175000.0) / 37500.0 * M_PI + M_PI);
  }
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::TsunamiEvent1d::getHeight(t_real i_x, t_real) const {
  t_real l_bIn = getBathymetryRaw(i_x);

  return std::max(-l_bIn, m_delta);
}

tsunami_lab::t_real
tsunami_lab::setups::TsunamiEvent1d::getMomentumX(t_real, t_real) const {
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::TsunamiEvent1d::getMomentumY(t_real, t_real) const {
  return 0;
}

tsunami_lab::t_real
tsunami_lab::setups::TsunamiEvent1d::getBathymetry(t_real i_x, t_real) const {
  t_real l_bIn = getBathymetryRaw(i_x);
  t_real l_d = getDisplacement(i_x);

  return std::min(l_bIn, -m_delta) + l_d;
}
