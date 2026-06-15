/**
 * @author Alexander Breuer (alex.breuer AT uni-jena.de)
 * @author Yannik Köllmann (yannik.koellmann AT uni-jena.de)
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 * @author Mika Brückner (mika.brueckner AT uni-jena.de)
 *
 * @section DESCRIPTION
 * One-dimensional wave propagation patch.
 **/
#include "WavePropagation1d.h"

#include <algorithm>
#include <iostream>
#include <ostream>
#include <string>

#include "../../solvers/fwave/FWave.h"
#include "../../solvers/roe/Roe.h"

tsunami_lab::patches::WavePropagation1d::WavePropagation1d(t_idx i_nCells) {
  m_nCells = i_nCells;

  for (unsigned short l_st = 0; l_st < 2; l_st++) {
    m_h[l_st] = new t_real[m_nCells + 2]{};
    m_hu[l_st] = new t_real[m_nCells + 2]{};
  }
  m_b = new t_real[m_nCells + 2]{};

  // pre-allocate per-edge net-update arrays for parallelized timeStep
  m_netUpdatesL_h = new t_real[m_nCells + 1];
  m_netUpdatesL_hu = new t_real[m_nCells + 1];
  m_netUpdatesR_h = new t_real[m_nCells + 1];
  m_netUpdatesR_hu = new t_real[m_nCells + 1];
}

tsunami_lab::patches::WavePropagation1d::~WavePropagation1d() {
  for (unsigned short l_st = 0; l_st < 2; l_st++) {
    delete[] m_h[l_st];
    delete[] m_hu[l_st];
  }
  delete[] m_b;
  delete[] m_netUpdatesL_h;
  delete[] m_netUpdatesL_hu;
  delete[] m_netUpdatesR_h;
  delete[] m_netUpdatesR_hu;
}

void tsunami_lab::patches::WavePropagation1d::timeStep(t_real i_scaling,
                                                       std::string i_mode) {
  t_real* l_hOld = m_h[m_step];
  t_real* l_huOld = m_hu[m_step];
  t_real* l_b = m_b;

  m_step = (m_step + 1) % 2;
  t_real* l_hNew = m_h[m_step];
  t_real* l_huNew = m_hu[m_step];

  std::transform(i_mode.begin(), i_mode.end(), i_mode.begin(), ::tolower);

  // Phase 1: compute net updates for all edges in parallel (no write conflicts)
  // schedule(runtime): selectable via OMP_SCHEDULE
#pragma omp parallel for schedule(runtime)
  for (t_idx l_ed = 0; l_ed < m_nCells + 1; l_ed++) {
    t_real l_netUpdates[2][2] = {{0}};
    t_idx l_ceL = l_ed;
    t_idx l_ceR = l_ed + 1;

    if (i_mode == "roe") {
      solvers::Roe::netUpdates(l_hOld[l_ceL], l_hOld[l_ceR], l_huOld[l_ceL],
                               l_huOld[l_ceR], l_netUpdates[0],
                               l_netUpdates[1]);
    } else if (i_mode == "fwave") {
      solvers::FWave::netUpdates(l_hOld[l_ceL], l_hOld[l_ceR], l_huOld[l_ceL],
                                 l_huOld[l_ceR], l_b[l_ceL], l_b[l_ceR],
                                 l_netUpdates[0], l_netUpdates[1]);
    } else {
      std::cerr << "invalid solver mode" << std::endl;
      exit(EXIT_FAILURE);
    }

    m_netUpdatesL_h[l_ed] = l_netUpdates[0][0];
    m_netUpdatesL_hu[l_ed] = l_netUpdates[0][1];
    m_netUpdatesR_h[l_ed] = l_netUpdates[1][0];
    m_netUpdatesR_hu[l_ed] = l_netUpdates[1][1];
  }

  // Phase 2: apply updates to interior cells in parallel (each cell written
  // once) Cell l_ce receives the right update of edge l_ce-1 and the left
  // update of edge l_ce.
#pragma omp parallel for schedule(runtime)
  for (t_idx l_ce = 1; l_ce <= m_nCells; l_ce++) {
    l_hNew[l_ce] = l_hOld[l_ce] - i_scaling * (m_netUpdatesR_h[l_ce - 1] +
                                               m_netUpdatesL_h[l_ce]);
    l_huNew[l_ce] = l_huOld[l_ce] - i_scaling * (m_netUpdatesR_hu[l_ce - 1] +
                                                 m_netUpdatesL_hu[l_ce]);
  }
}

void tsunami_lab::patches::WavePropagation1d::setGhostOutflow() {
  setGhost(BoundaryCondition::Outflow, BoundaryCondition::Outflow);
}

void tsunami_lab::patches::WavePropagation1d::setGhost(
    BoundaryCondition i_left, BoundaryCondition i_right) {
  t_real* l_h = m_h[m_step];
  t_real* l_hu = m_hu[m_step];
  t_real* l_b = m_b;

  // left boundary: copy h, negate hu for reflecting wall (Eq. 3.2.1)
  l_h[0] = l_h[1];
  l_hu[0] = (i_left == BoundaryCondition::Reflecting) ? -l_hu[1] : l_hu[1];
  l_b[0] = l_b[1];

  // right boundary: copy h, negate hu for reflecting wall (Eq. 3.2.1)
  l_h[m_nCells + 1] = l_h[m_nCells];
  l_hu[m_nCells + 1] = (i_right == BoundaryCondition::Reflecting)
                           ? -l_hu[m_nCells]
                           : l_hu[m_nCells];
  l_b[m_nCells + 1] = l_b[m_nCells];
}
