#!/usr/bin/env bash
##
# Detects this machine's CPU topology and derives the environment needed to
# run tsunami_lab_viz with the GUI thread pinned to one fixed core and
# OpenMP given one worker thread per remaining performance core (see
# sphinx/source/chapters/12_individual_week3.rst).
#
# - Hybrid CPUs (Intel P-core/E-core): the GUI gets one E-core, OpenMP gets
#   all P-cores.
# - Homogeneous CPUs (with or without SMT): one representative logical CPU
#   per physical core is chosen; the GUI gets the first, OpenMP gets one
#   thread per remaining physical core (SMT siblings are left idle).
#
# Usage:
#   eval "$(scripts/viz_pinning_env.sh)"        # then run the binary yourself
#   scripts/viz_pinning_env.sh CMD [ARGS...]    # detect + exec CMD with env set
#
# All diagnostics go to stderr so `eval "$(...)"` only ever sees the
# `export ...` lines on stdout.
##
set -euo pipefail

# Expands a cpulist like "0-2,5,8-9" (as found in sysfs/lscpu) into one id
# per line.
expand_cpulist() {
  local spec="$1" part lo hi i
  IFS=',' read -ra parts <<<"$spec"
  for part in "${parts[@]}"; do
    if [[ "$part" == *-* ]]; then
      lo=${part%-*}; hi=${part#*-}
      for ((i = lo; i <= hi; i++)); do echo "$i"; done
    else
      echo "$part"
    fi
  done
}

declare -a viz_cores=()
declare -a omp_cores=()

if [[ -r /sys/devices/cpu_core/cpus && -r /sys/devices/cpu_atom/cpus ]]; then
  echo "# hybrid CPU detected (cpu_core/cpu_atom): GUI -> E-core, OpenMP -> P-cores" >&2
  mapfile -t p_cores < <(expand_cpulist "$(cat /sys/devices/cpu_core/cpus)")
  mapfile -t e_cores < <(expand_cpulist "$(cat /sys/devices/cpu_atom/cpus)")
  if [[ ${#e_cores[@]} -eq 0 || ${#p_cores[@]} -eq 0 ]]; then
    echo "# cpu_core/cpu_atom present but empty -- falling back to homogeneous detection" >&2
  else
    viz_cores=("${e_cores[0]}")
    omp_cores=("${p_cores[@]}")
  fi
fi

if [[ ${#omp_cores[@]} -eq 0 ]]; then
  echo "# homogeneous CPU: grouping logical CPUs by physical core (lscpu -p)" >&2
  declare -A core_rep=()
  while IFS=, read -r cpu core online; do
    [[ "$cpu" == \#* ]] && continue
    [[ -n "${online:-Y}" && "${online:-Y}" != "Y" ]] && continue
    if [[ -z "${core_rep[$core]:-}" ]]; then
      core_rep[$core]=$cpu
    fi
  done < <(lscpu -p=CPU,CORE,ONLINE)

  mapfile -t reps < <(printf '%s\n' "${core_rep[@]}" | sort -n)
  if [[ ${#reps[@]} -lt 2 ]]; then
    echo "# only ${#reps[@]} physical core(s) found -- no core to spare for pinning," \
         "leaving OMP_NUM_THREADS/OMP_PLACES/TSUNAMI_VIZ_CORE unset" >&2
  else
    viz_cores=("${reps[0]}")
    omp_cores=("${reps[@]:1}")
  fi
fi

if [[ ${#omp_cores[@]} -gt 0 ]]; then
  places=$(printf '{%s},' "${omp_cores[@]}")
  places="${places%,}"
  echo "# GUI core: ${viz_cores[0]}   OpenMP cores (${#omp_cores[@]}): ${omp_cores[*]}" >&2

  export TSUNAMI_VIZ_CORE="${viz_cores[0]}"
  export OMP_NUM_THREADS="${#omp_cores[@]}"
  export OMP_PROC_BIND="close"
  export OMP_PLACES="$places"
fi

if [[ $# -gt 0 ]]; then
  exec "$@"
fi

if [[ ${#omp_cores[@]} -gt 0 ]]; then
  echo "export TSUNAMI_VIZ_CORE=$TSUNAMI_VIZ_CORE"
  echo "export OMP_NUM_THREADS=$OMP_NUM_THREADS"
  echo "export OMP_PROC_BIND=$OMP_PROC_BIND"
  echo "export OMP_PLACES=\"$OMP_PLACES\""
fi
