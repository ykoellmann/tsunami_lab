/**
 * @author Jan Vogt (jan.vogt AT uni-jena.de)
 *
 * @section DESCRIPTION
 * Runs a WavePropagation2d solver on a background thread and publishes each new
 * water-height frame into a SimBuffer for the renderer. No file I/O happens in
 * the loop (unlike the batch main.cpp).
 *
 * Usage:
 *   SimBuffer    buf(nx, ny);
 *   SolverThread sim(buf, nx, ny, dxy);
 *   // configure the initial state on the owned solver:
 *   sim.solver().setBathymetry(...); sim.solver().setHeight(...);
 *   sim.start();           // launches the loop
 *   ... buf.swap(); buf.front() ... // renderer reads frames
 *   sim.stop();            // joins the thread (also done by the destructor)
 *
 * The solver must only be configured via solver() *before* start(); once the
 * loop runs it owns the solver exclusively.
 **/
#ifndef TSUNAMI_LAB_VISUALIZATION_SOLVERTHREAD
#define TSUNAMI_LAB_VISUALIZATION_SOLVERTHREAD

#include "../constants.h"
#include "../patches/WavePropagation2d/WavePropagation2d.h"
#include "SimBuffer.h"
#include <atomic>
#include <string>
#include <thread>

namespace tsunami_lab {
namespace visualization {

class SolverThread {
public:
  /**
   * @param io_buffer destination for published water-height frames.
   * @param i_nx number of cells in x-direction (use nx == ny; the solver's
   *             flat indexing is only collision-free for square grids).
   * @param i_ny number of cells in y-direction.
   * @param i_dxy cell size in metres (square cells), used for the CFL step.
   **/
  SolverThread(SimBuffer& io_buffer, t_idx i_nx, t_idx i_ny, t_real i_dxy);

  //! Stops and joins the background thread if still running.
  ~SolverThread();

  //! Access the owned solver for one-time setup before start().
  patches::WavePropagation2d& solver() { return m_solver; }

  /**
   * Derives a CFL-stable scaling (dt/dx) from the current state, then launches
   * the background loop. No-op if already running.
   **/
  void start();

  //! Signals the loop to stop and joins the thread. Safe to call repeatedly.
  void stop();

  bool running() const { return m_running.load(); }

  //! Number of time steps advanced so far (for diagnostics / tests).
  t_idx steps() const { return m_steps.load(); }

  //! Scaling (dt/dx) chosen by the last start(); 0 before the first start.
  t_real scaling() const { return m_scaling; }

  // Real-time pacing: how many simulated seconds to advance per wall-clock
  // second. 0 (default) runs as fast as possible (used by the unit tests);
  // a positive value throttles the loop so the wave is watchable.
  void setTimeScale(double i_simSecondsPerRealSecond) {
    m_timeScale.store(i_simSecondsPerRealSecond);
  }

  // Largest time scale the hardware can actually sustain, i.e. simulated
  // seconds per real second when computing flat out (= dt / wall-time-per-step,
  // smoothed). 0 until a few steps have been measured. Requesting a larger
  // time scale than this just makes the loop fall behind.
  double maxTimeScale() const {
    const double l_step = m_stepSeconds.load();
    if (l_step <= 0.0)
      return 0.0;
    return ((double)m_scaling * (double)m_dxy) / l_step;
  }

  t_idx nx() const { return m_nx; }
  t_idx ny() const { return m_ny; }
  //! Cell size in metres (square cells); needed by the renderer's mesh.
  t_real dxy() const { return m_dxy; }

private:
  //! Background loop: setGhost → timeStep → publish, until stop().
  void run();

  //! max(|u|, |v|) + sqrt(g*h) over all wet cells of the current state.
  t_real maxWaveSpeed();

  SimBuffer& m_buffer;
  patches::WavePropagation2d m_solver;
  t_idx m_nx;
  t_idx m_ny;
  t_real m_dxy;
  t_real m_scaling = 0;
  std::string m_mode = "fwave";

  std::thread m_thread;
  std::atomic<bool> m_running{false};
  std::atomic<t_idx> m_steps{0};
  //! Simulated seconds advanced per wall-clock second; 0 = uncapped.
  std::atomic<double> m_timeScale{0.0};
  //! Smoothed wall-clock seconds spent computing one step (excludes pacing).
  std::atomic<double> m_stepSeconds{0.0};
};

} // namespace visualization
} // namespace tsunami_lab

#endif
