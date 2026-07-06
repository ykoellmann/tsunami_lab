/**
 * @author Yannik Köllmann
 * @author Jan Vogt
 * @author Mika Brückner
 * @section DESCRIPTION
 * Two-dimensional wave propagation patch.
 **/

#include "WavePropagation2d.h"
#include "../../solvers/fwave/FWave.h"
#include <cmath>

tsunami_lab::patches::WavePropagation2d::WavePropagation2d(t_idx i_nCells_x,
                                                           t_idx i_nCells_y) {
  m_nCells_x = i_nCells_x;
  m_nCells_y = i_nCells_y;

  t_idx l_size = (m_nCells_x + 2) * (m_nCells_y + 2);

  // Allocate without zero-init; touch pages in parallel (NUMA first-touch) so
  // each page is mapped on the NUMA node of the thread that will own it during
  // the sweeps.
  m_h = new t_real[2 * l_size];
  m_hu = new t_real[2 * l_size];
  m_hv = new t_real[2 * l_size];
  m_b = new t_real[l_size];

#pragma omp parallel for schedule(static)
  for (t_idx l_i = 0; l_i < l_size; l_i++) {
    m_h[l_i] = 0;
    m_h[l_size + l_i] = 0;
    m_hu[l_i] = 0;
    m_hu[l_size + l_i] = 0;
    m_hv[l_i] = 0;
    m_hv[l_size + l_i] = 0;
    m_b[l_i] = 0;
  }
}

tsunami_lab::patches::WavePropagation2d::~WavePropagation2d() {
  delete[] m_h;
  delete[] m_hu;
  delete[] m_hv;
  delete[] m_b;
}

void tsunami_lab::patches::WavePropagation2d::setGhostOutflow() {
  t_idx l_nY = m_nCells_y + 2;
  t_idx l_stepSize = (m_nCells_x + 2) * l_nY;

  t_real* l_h = m_h + m_step * l_stepSize;
  t_real* l_hu = m_hu + m_step * l_stepSize;
  t_real* l_hv = m_hv + m_step * l_stepSize;

  // Left ghost column (x=0): copy from x=1
  for (t_idx l_iy = 0; l_iy < l_nY; l_iy++) {
    t_idx l_ghost = getCoordinates(0, l_iy);
    t_idx l_inner = getCoordinates(1, l_iy);
    l_h[l_ghost] = l_h[l_inner];
    l_hu[l_ghost] = l_hu[l_inner];
    l_hv[l_ghost] = l_hv[l_inner];
    m_b[l_ghost] = m_b[l_inner];
  }

  // Right ghost column (x = m_nCells_x+1): copy from x = m_nCells_x
  for (t_idx l_iy = 0; l_iy < l_nY; l_iy++) {
    t_idx l_ghost = getCoordinates(m_nCells_x + 1, l_iy);
    t_idx l_inner = getCoordinates(m_nCells_x, l_iy);
    l_h[l_ghost] = l_h[l_inner];
    l_hu[l_ghost] = l_hu[l_inner];
    l_hv[l_ghost] = l_hv[l_inner];
    m_b[l_ghost] = m_b[l_inner];
  }

  // Bottom ghost row (y=0): copy from y=1
  for (t_idx l_ix = 0; l_ix < m_nCells_x + 2; l_ix++) {
    t_idx l_ghost = getCoordinates(l_ix, 0);
    t_idx l_inner = getCoordinates(l_ix, 1);
    l_h[l_ghost] = l_h[l_inner];
    l_hu[l_ghost] = l_hu[l_inner];
    l_hv[l_ghost] = l_hv[l_inner];
    m_b[l_ghost] = m_b[l_inner];
  }

  // Top ghost row (y = m_nCells_y+1): copy from y = m_nCells_y
  for (t_idx l_ix = 0; l_ix < m_nCells_x + 2; l_ix++) {
    t_idx l_ghost = getCoordinates(l_ix, m_nCells_y + 1);
    t_idx l_inner = getCoordinates(l_ix, m_nCells_y);
    l_h[l_ghost] = l_h[l_inner];
    l_hu[l_ghost] = l_hu[l_inner];
    l_hv[l_ghost] = l_hv[l_inner];
    m_b[l_ghost] = m_b[l_inner];
  }
}

void tsunami_lab::patches::WavePropagation2d::setGhost(
    BoundaryCondition i_left,
    BoundaryCondition i_right,
    BoundaryCondition i_upper,
    BoundaryCondition i_lower) {
  t_idx l_nY = m_nCells_y + 2;
  t_idx l_stepSize = (m_nCells_x + 2) * l_nY;
  t_real* l_h = m_h + m_step * l_stepSize;
  t_real* l_hu = m_hu + m_step * l_stepSize;
  t_real* l_hv = m_hv + m_step * l_stepSize;

  for (t_idx l_iy = 0; l_iy < l_nY; l_iy++) {
    t_idx l_ghost = getCoordinates(0, l_iy);
    t_idx l_inner = getCoordinates(1, l_iy);
    l_h[l_ghost] = l_h[l_inner];
    l_hu[l_ghost] = (i_left == BoundaryCondition::Reflecting) ? -l_hu[l_inner]
                                                              : l_hu[l_inner];
    l_hv[l_ghost] = l_hv[l_inner];
    m_b[l_ghost] = m_b[l_inner];
  }

  for (t_idx l_iy = 0; l_iy < l_nY; l_iy++) {
    t_idx l_ghost = getCoordinates(m_nCells_x + 1, l_iy);
    t_idx l_inner = getCoordinates(m_nCells_x, l_iy);
    l_h[l_ghost] = l_h[l_inner];
    l_hu[l_ghost] = (i_right == BoundaryCondition::Reflecting) ? -l_hu[l_inner]
                                                               : l_hu[l_inner];
    l_hv[l_ghost] = l_hv[l_inner];
    m_b[l_ghost] = m_b[l_inner];
  }

  for (t_idx l_ix = 0; l_ix < m_nCells_x + 2; l_ix++) {
    t_idx l_ghost = getCoordinates(l_ix, 0);
    t_idx l_inner = getCoordinates(l_ix, 1);
    l_h[l_ghost] = l_h[l_inner];
    l_hu[l_ghost] = l_hu[l_inner];
    l_hv[l_ghost] = (i_lower == BoundaryCondition::Reflecting) ? -l_hv[l_inner]
                                                               : l_hv[l_inner];
    m_b[l_ghost] = m_b[l_inner];
  }

  for (t_idx l_ix = 0; l_ix < m_nCells_x + 2; l_ix++) {
    t_idx l_ghost = getCoordinates(l_ix, m_nCells_y + 1);
    t_idx l_inner = getCoordinates(l_ix, m_nCells_y);
    l_h[l_ghost] = l_h[l_inner];
    l_hu[l_ghost] = l_hu[l_inner];
    l_hv[l_ghost] = (i_upper == BoundaryCondition::Reflecting) ? -l_hv[l_inner]
                                                               : l_hv[l_inner];
    m_b[l_ghost] = m_b[l_inner];
  }
}

void tsunami_lab::patches::WavePropagation2d::timeStep(t_real i_scaling,
                                                       std::string) {
  t_idx l_size = (m_nCells_x + 2) * (m_nCells_y + 2);

  t_real* l_hCur = m_h + m_step * l_size;
  t_real* l_huCur = m_hu + m_step * l_size;
  t_real* l_hvCur = m_hv + m_step * l_size;

  m_step = 1 - m_step;

  t_real* l_hNew = m_h + m_step * l_size;
  t_real* l_huNew = m_hu + m_step * l_size;
  t_real* l_hvNew = m_hv + m_step * l_size;

  // One parallel region for all three phases to avoid repeated thread creation.
#pragma omp parallel
  {
#pragma omp for schedule(runtime)
    for (t_idx l_i = 0; l_i < l_size; l_i++) {
      l_hNew[l_i] = l_hCur[l_i];
      l_huNew[l_i] = l_huCur[l_i];
      l_hvNew[l_i] = l_hvCur[l_i];
    }

    // X-sweep: rows are independent, parallelise over l_iy.
#pragma omp for schedule(runtime)
    for (t_idx l_iy = 1; l_iy <= m_nCells_y; l_iy++) {
      for (t_idx l_ix = 0; l_ix <= m_nCells_x; l_ix++) {
        t_idx l_idxL = getCoordinates(l_ix, l_iy);
        t_idx l_idxR = getCoordinates(l_ix + 1, l_iy);
        t_real l_netUpdateL[2];
        t_real l_netUpdateR[2];

        solvers::FWave::netUpdates(
            l_hCur[l_idxL], l_hCur[l_idxR], l_huCur[l_idxL], l_huCur[l_idxR],
            m_b[l_idxL], m_b[l_idxR], l_netUpdateL, l_netUpdateR);

        l_hNew[l_idxL] -= i_scaling * l_netUpdateL[0];
        l_huNew[l_idxL] -= i_scaling * l_netUpdateL[1];
        l_hNew[l_idxR] -= i_scaling * l_netUpdateR[0];
        l_huNew[l_idxR] -= i_scaling * l_netUpdateR[1];
      }
    }
    // Y-sweep: adjacent edges share cells, so use red-black (even/odd) row
    // splitting — edges of the same parity write disjoint cells and can run
    // in parallel; the implicit barrier between passes orders the shared
    // writes.
    auto l_processYRow = [&](t_idx l_iy) {
      for (t_idx l_ix = 1; l_ix <= m_nCells_x; l_ix++) {
        t_idx l_idxB = getCoordinates(l_ix, l_iy);
        t_idx l_idxT = getCoordinates(l_ix, l_iy + 1);
        t_real l_netUpdateB[2];
        t_real l_netUpdateT[2];

        solvers::FWave::netUpdates(
            l_hCur[l_idxB], l_hCur[l_idxT], l_hvCur[l_idxB], l_hvCur[l_idxT],
            m_b[l_idxB], m_b[l_idxT], l_netUpdateB, l_netUpdateT);

        l_hNew[l_idxB] -= i_scaling * l_netUpdateB[0];
        l_hvNew[l_idxB] -= i_scaling * l_netUpdateB[1];
        l_hNew[l_idxT] -= i_scaling * l_netUpdateT[0];
        l_hvNew[l_idxT] -= i_scaling * l_netUpdateT[1];
      }
    };

#pragma omp for schedule(runtime)
    for (t_idx l_iy = 0; l_iy <= m_nCells_y; l_iy += 2) {
      l_processYRow(l_iy);
    }

#pragma omp for schedule(runtime)
    for (t_idx l_iy = 1; l_iy <= m_nCells_y; l_iy += 2) {
      l_processYRow(l_iy);
    }
    // clamp h >= 0 and reset any non-finite cell (NaN/Inf) to dry, so a
    // locally unstable cell can't keep infecting its neighbours via the
    // stencil and ghost cells.
#pragma omp for schedule(runtime)
    for (t_idx l_i = 0; l_i < l_size; l_i++) {
      bool l_invalid = l_hNew[l_i] < 0 || !std::isfinite(l_hNew[l_i]) ||
                       !std::isfinite(l_huNew[l_i]) ||
                       !std::isfinite(l_hvNew[l_i]);
      if (l_invalid) {
        l_hNew[l_i] = 0;
        l_huNew[l_i] = 0;
        l_hvNew[l_i] = 0;
      }
    }
  } // end parallel
}
