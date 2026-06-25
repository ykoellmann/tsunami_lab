/**
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Double buffer decoupling the solver thread (producer) from the render thread
 * (consumer). One producer, one consumer; no blocking between them.
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
  SimBuffer(t_idx i_nx, t_idx i_ny) : m_nx(i_nx), m_ny(i_ny) {
    m_buf[0].assign(i_nx * i_ny, t_real(0));
    m_buf[1].assign(i_nx * i_ny, t_real(0));
  }

  // Producer: copies a frame into the back buffer and marks it ready.
  void write(const t_real* i_src, t_idx i_n) {
    std::lock_guard<std::mutex> l_lock(m_mtx);
    t_idx l_n = std::min<t_idx>(i_n, m_buf[m_back].size());
    std::copy(i_src, i_src + l_n, m_buf[m_back].begin());
    m_dirty = true;
  }

  // Consumer: swaps the latest frame to the front. Returns true if fresh.
  bool swap() {
    std::lock_guard<std::mutex> l_lock(m_mtx);
    if (!m_dirty)
      return false;
    std::swap(m_front, m_back);
    m_dirty = false;
    return true;
  }

  // Consumer: the most recently swapped frame. Safe to read without the lock
  // as long as only one consumer calls swap().
  const t_real* front() const { return m_buf[m_front].data(); }

  t_idx nx() const { return m_nx; }
  t_idx ny() const { return m_ny; }

private:
  std::vector<t_real> m_buf[2];
  int m_front = 0;
  int m_back = 1;
  std::mutex m_mtx;
  bool m_dirty = false;
  t_idx m_nx;
  t_idx m_ny;
};

} // namespace visualization
} // namespace tsunami_lab

#endif
