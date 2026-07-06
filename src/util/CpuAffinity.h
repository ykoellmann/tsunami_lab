/**
 * @author Yannik Koellmann
 *
 * Pins the calling thread to a single CPU core. Used to keep the GUI/render
 * thread of tsunami_lab_viz off the cores reserved for OpenMP worker threads
 * (see sphinx/source/chapters/09_parallelization.rst, section "Pinning the
 * visualization GUI thread").
 **/
#ifndef TSUNAMI_LAB_UTIL_CPUAFFINITY_H
#define TSUNAMI_LAB_UTIL_CPUAFFINITY_H

namespace tsunami_lab {
namespace util {

// Pins the calling thread to logical CPU i_core. Returns false (and leaves
// the thread's affinity unchanged) if the platform doesn't support it or the
// underlying call fails. Linux-only; no-op elsewhere.
bool pinThreadToCore(int i_core);

} // namespace util
} // namespace tsunami_lab

#endif
