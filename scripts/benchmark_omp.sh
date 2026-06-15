#!/usr/bin/env bash

##
# OpenMP benchmark driver for the tsunami solver.
#
# Runs three sweeps on a single shared-memory node (e.g. an NVIDIA Grace node)
# and writes one CSV per sweep plus a human-readable summary:
#
#   A) thread scaling   -> speedup S_p = T_1 / T_p              (task 2)
#   B) scheduling        -> static vs. dynamic vs. guided        (task 4)
#   C) pinning / NUMA    -> OMP_PROC_BIND x OMP_PLACES           (task 4)
#
# The reported time is the solver's own "compute wall-clock" (kernel only,
# I/O and setup excluded), parsed from the program output. File output is
# disabled with --io-steps 0 so the disk and I/O never distort the timing.
#
# Usage:
#   scripts/benchmark_omp.sh                 # build + run all sweeps (defaults)
#   BENCH_NO_BUILD=1 scripts/benchmark_omp.sh
#   BENCH_N=4000 BENCH_T=1200 scripts/benchmark_omp.sh
#   BENCH_THREADS="1 4 16 72 144" scripts/benchmark_omp.sh
#
# All knobs are environment variables (see CONFIG below).
##
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

# CONFIG (override via environment)
BIN="${BENCH_BIN:-build/tsunami_lab}"

# scenario: Tohoku 2011 via TsunamiEvent2d. Domain is derived from the bath
# file; -n sets the cell count per dimension (resolution = 2700km / n).
BATH="${BENCH_BATH:-ressources/tsunami_simulations/output/tohoku_gebco20_ucsb3_250m_bath.nc}"
DISPL="${BENCH_DISPL:-ressources/tsunami_simulations/output/tohoku_gebco20_ucsb3_250m_displ.nc}"

# problem size and simulated end time (seconds). Defaults give a few thousand
# compute iterations on a grid large enough to scale to 144 threads while still
# finishing quickly. Bump BENCH_N for a heavier, more scalable run.
N="${BENCH_N:-3000}"
T="${BENCH_T:-600}"

# thread counts for the scaling sweep (must start at 1 for the speedup baseline)
THREADS="${BENCH_THREADS:-1 2 4 8 16 36 72 108 144}"

# fixed thread count for the scheduling and pinning sweeps
THREADS_FIXED="${BENCH_THREADS_FIXED:-72}"

# scheduling strategies for sweep B (OMP_SCHEDULE syntax: kind[,chunk])
SCHEDULES="${BENCH_SCHEDULES:-static dynamic,64 guided}"

# pinning combinations for sweep C, given as "BIND:PLACES" pairs
PINNINGS="${BENCH_PINNINGS:-false:cores close:cores spread:cores close:sockets spread:sockets}"

STAMP="$(date +%Y%m%d_%H%M%S)"
OUTDIR="${BENCH_OUTDIR:-results/omp_bench_${STAMP}}"
SCRATCH="$(mktemp -d)"
trap 'rm -rf "$SCRATCH"' EXIT

mkdir -p "$OUTDIR"

# build
if [[ "${BENCH_NO_BUILD:-0}" != "1" ]]; then
  echo "=== building (omp=1 opt=o3 arch=native) ==="
  scons omp=1 opt=o3 arch=native -j"$(nproc 2>/dev/null || echo 4)"
fi

if [[ ! -x "$BIN" ]]; then
  echo "error: binary '$BIN' not found or not executable" >&2
  exit 1
fi
for f in "$BATH" "$DISPL"; do
  [[ -f "$f" ]] || { echo "error: input file '$f' not found" >&2; exit 1; }
done

# run_once <logfile> <env assignments...>
#   runs the solver once with the given OMP_* environment, prints
#   "<iterations> <cells> <compute_seconds>" parsed from the output.
run_once() {
  local log="$1"; shift
  env "$@" \
    "$BIN" -n "$N" -t "$T" --io-steps 0 -o "$SCRATCH/run" \
    -p TsunamiEvent2d "$BATH" "$DISPL" >"$log" 2>&1 || {
      echo "  !! run failed; see $log" >&2; tail -n 20 "$log" >&2; return 1; }
  rm -rf "$SCRATCH/run"
  local iters cells secs
  iters=$(awk '/iterations:/        {print $2}' "$log")
  cells=$(awk '/^  cells:/          {print $2}' "$log")
  secs=$( awk '/compute wall-clock:/{print $3}' "$log")
  echo "${iters:-0} ${cells:-0} ${secs:-0}"
}

echo
echo "config: N=$N  T=${T}s  bath=$(basename "$BATH")"
echo "output: $OUTDIR"
echo

# Sweep A: thread scaling -> speedup
CSV_A="$OUTDIR/scaling.csv"
echo "threads,iterations,cells,compute_s,speedup,efficiency,cell_updates_per_s" >"$CSV_A"
echo "=== Sweep A: thread scaling (OMP_PROC_BIND=close OMP_PLACES=cores OMP_SCHEDULE=static) ==="
printf "%8s %12s %12s %10s %10s\n" threads "compute_s" speedup eff "Mupd/s"
T1=""
for p in $THREADS; do
  read -r iters cells secs < <(run_once "$OUTDIR/scaling_p${p}.log" \
      OMP_NUM_THREADS="$p" OMP_PROC_BIND=close OMP_PLACES=cores OMP_SCHEDULE=static)
  [[ -z "$T1" ]] && T1="$secs"
  awk -v p="$p" -v it="$iters" -v c="$cells" -v s="$secs" -v t1="$T1" 'BEGIN{
    sp = (s>0)? t1/s : 0; eff = (p>0)? sp/p : 0;
    mu = (s>0)? c*it/s/1e6 : 0;
    printf "%8d %12.3f %12.3f %10.3f %10.1f\n", p, s, sp, eff, mu;
  }'
  awk -v p="$p" -v it="$iters" -v c="$cells" -v s="$secs" -v t1="$T1" 'BEGIN{
    sp = (s>0)? t1/s : 0; eff = (p>0)? sp/p : 0;
    upd = (s>0)? c*it/s : 0;
    printf "%d,%d,%d,%.4f,%.4f,%.4f,%.0f\n", p, it, c, s, sp, eff, upd;
  }' >>"$CSV_A"
done
echo "  -> $CSV_A"
echo

# Sweep B: scheduling strategies
CSV_B="$OUTDIR/scheduling.csv"
echo "schedule,threads,compute_s,cell_updates_per_s" >"$CSV_B"
echo "=== Sweep B: scheduling at $THREADS_FIXED threads (OMP_PROC_BIND=close OMP_PLACES=cores) ==="
printf "%16s %12s %12s\n" schedule "compute_s" "Mupd/s"
for sched in $SCHEDULES; do
  read -r iters cells secs < <(run_once "$OUTDIR/sched_${sched//,/_}.log" \
      OMP_NUM_THREADS="$THREADS_FIXED" OMP_PROC_BIND=close OMP_PLACES=cores \
      OMP_SCHEDULE="$sched")
  awk -v sc="$sched" -v it="$iters" -v c="$cells" -v s="$secs" 'BEGIN{
    mu=(s>0)?c*it/s/1e6:0; printf "%16s %12.3f %12.1f\n", sc, s, mu; }'
  awk -v sc="$sched" -v p="$THREADS_FIXED" -v it="$iters" -v c="$cells" -v s="$secs" 'BEGIN{
    upd=(s>0)?c*it/s:0; printf "%s,%d,%.4f,%.0f\n", sc, p, s, upd; }' >>"$CSV_B"
done
echo "  -> $CSV_B"
echo

# Sweep C: pinning / NUMA
CSV_C="$OUTDIR/pinning.csv"
echo "proc_bind,places,threads,compute_s,cell_updates_per_s" >"$CSV_C"
echo "=== Sweep C: pinning at $THREADS_FIXED threads (OMP_SCHEDULE=static) ==="
printf "%10s %10s %12s %12s\n" bind places "compute_s" "Mupd/s"
for pin in $PINNINGS; do
  bind="${pin%%:*}"; places="${pin##*:}"
  read -r iters cells secs < <(run_once "$OUTDIR/pin_${bind}_${places}.log" \
      OMP_NUM_THREADS="$THREADS_FIXED" OMP_PROC_BIND="$bind" OMP_PLACES="$places" \
      OMP_SCHEDULE=static)
  awk -v b="$bind" -v pl="$places" -v it="$iters" -v c="$cells" -v s="$secs" 'BEGIN{
    mu=(s>0)?c*it/s/1e6:0; printf "%10s %10s %12.3f %12.1f\n", b, pl, s, mu; }'
  awk -v b="$bind" -v pl="$places" -v p="$THREADS_FIXED" -v it="$iters" -v c="$cells" -v s="$secs" 'BEGIN{
    upd=(s>0)?c*it/s:0; printf "%s,%s,%d,%.4f,%.0f\n", b, pl, p, s, upd; }' >>"$CSV_C"
done
echo "  -> $CSV_C"
echo

echo "=== done. CSVs in $OUTDIR ==="
ls -1 "$OUTDIR"/*.csv
