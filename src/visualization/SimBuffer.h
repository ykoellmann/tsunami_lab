/**
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Double buffer decoupling the solver thread (producer) from the render thread
 * (consumer). The producer writes whole water-height frames into the back
 * buffer; the consumer swaps the most recent frame to the front and reads it
 * without blocking the producer. Designed for exactly one producer and one
 * consumer.
 **/
#ifndef TSUNAMI_LAB_VISUALIZATION_SIMBUFFER
#define TSUNAMI_LAB_VISUALIZATION_SIMBUFFER

#include "../constants.h"
#include <algorithm>
#include <mutex>
#include <vector>

namespace tsunami_lab {
namespace visualization {

class SimBuffer {
public:
  /**
   * Allocates both buffers for an i_nx × i_ny grid, zero-initialised.
   *
   * @param i_nx number of cells in x-direction.
   * @param i_ny number of cells in y-direction.
   **/
  SimBuffer(t_idx i_nx, t_idx i_ny) : m_nx(i_nx), m_ny(i_ny) {
    m_buf[0].assign(i_nx * i_ny, t_real(0));
    m_buf[1].assign(i_nx * i_ny, t_real(0));
  }

  /**
   * Producer side: copies a fresh frame into the back buffer and marks it
   * ready. At most nx*ny values are copied; a shorter source is padded with
   * whatever was already in the buffer.
   *
   * @param i_src contiguous nx*ny water heights.
   * @param i_n   number of values in i_src.
   **/
  void write(const t_real* i_src, t_idx i_n) {
    std::lock_guard<std::mutex> l_lock(m_mtx);
    t_idx l_n = std::min<t_idx>(i_n, m_buf[m_back].size());
    std::copy(i_src, i_src + l_n, m_buf[m_back].begin());
    m_dirty = true;
  }

  /**
   * Consumer side: if a new frame is available, swaps it to the front.
   *
   * @return true if front() now points at a fresh frame, false otherwise.
   **/
  bool swap() {
    std::lock_guard<std::mutex> l_lock(m_mtx);
    if (!m_dirty)
      return false;
    std::swap(m_front, m_back);
    m_dirty = false;
    return true;
  }

  /**
   * Consumer side: the most recently swapped frame (nx*ny contiguous values).
   * Only mutated by swap(), which the consumer also calls, so reading it
   * without the lock is safe for the single-consumer use case.
   **/
  const t_real* front() const { return m_buf[m_front].data(); }

  t_idx nx() const { return m_nx; }
  t_idx ny() const { return m_ny; }

private:
  //! [front], [back] frame storage; indices swapped on publish.
  std::vector<t_real> m_buf[2];
  int m_front = 0;
  int m_back = 1;
  //! Guards the index swap and the back-buffer copy against each other.
  std::mutex m_mtx;
  //! Set by write(), cleared by swap(); true means an unread frame is pending.
  bool m_dirty = false;
  t_idx m_nx;
  t_idx m_ny;
};

} // namespace visualization
} // namespace tsunami_lab

#endif
