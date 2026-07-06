/**
 * @author Yannik Koellmann
 **/
#include "CpuAffinity.h"

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace tsunami_lab {
namespace util {

bool pinThreadToCore(int i_core) {
#ifdef __linux__
  if (i_core < 0)
    return false;
  cpu_set_t l_set;
  CPU_ZERO(&l_set);
  CPU_SET(i_core, &l_set);
  return pthread_setaffinity_np(pthread_self(), sizeof(l_set), &l_set) == 0;
#else
  (void)i_core;
  return false;
#endif
}

} // namespace util
} // namespace tsunami_lab
