/**
 * @author Yannik Koellmann
 **/
#include "OmpDefaults.h"

#include <cstdlib>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace tsunami_lab {
namespace util {

void applySaneOmpScheduleDefault() {
#ifdef _OPENMP
  if (!std::getenv("OMP_SCHEDULE")) {
    omp_set_schedule(omp_sched_static, 0);
  }
#endif
}

} // namespace util
} // namespace tsunami_lab
