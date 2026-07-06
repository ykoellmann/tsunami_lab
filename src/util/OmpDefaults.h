/**
 * @author Yannik Koellmann
 *
 * The sweep loops use "schedule(runtime)"; if OMP_SCHEDULE is unset, libgomp
 * defaults to an unchunked dynamic schedule instead of static, which is ~37x
 * slower on this first-touch-initialised grid (see chapter 9 of the docs).
 **/
#ifndef TSUNAMI_LAB_UTIL_OMPDEFAULTS_H
#define TSUNAMI_LAB_UTIL_OMPDEFAULTS_H

namespace tsunami_lab {
namespace util {

// Sets the static schedule for "schedule(runtime)" loops, unless
// OMP_SCHEDULE is already set in the environment. No-op without OpenMP.
void applySaneOmpScheduleDefault();

} // namespace util
} // namespace tsunami_lab

#endif
