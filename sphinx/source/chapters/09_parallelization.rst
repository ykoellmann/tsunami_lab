9. Parallelization
==================

Implementation
---------------

Build flag
~~~~~~~~~~

OpenMP support is enabled via a new SCons option:

.. code-block:: bash

   scons omp=1          # release build with OpenMP
   scons omp=1 opt=o3   # combined with higher optimization level

The flag adds ``-fopenmp`` to both the compiler and linker invocations.
Without ``omp=1`` the pragmas are ignored and the solver remains serial
and fully reproducible.

Running with multiple threads
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The number of OpenMP threads is controlled through the standard
environment variable ``OMP_NUM_THREADS``:

.. code-block:: bash

   OMP_NUM_THREADS=8  build/tsunami_lab [args]   # 8 threads
   OMP_NUM_THREADS=144 build/tsunami_lab [args]  # all Grace cores

When ``OMP_NUM_THREADS`` is not set, OpenMP defaults to the number of
logical cores on the machine.

2D Solver (``WavePropagation2d``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The 2D time step consists of three phases executed inside a single
``#pragma omp parallel`` region so that the thread team is created only
once per call:

**Copy phase.** The current buffer is copied into the new buffer with a
flat parallel loop:

.. code-block:: c++

   #pragma omp for schedule(static)
   for (t_idx l_i = 0; l_i < l_size; l_i++) {
     l_hNew[l_i]  = l_hCur[l_i];
     l_huNew[l_i] = l_huCur[l_i];
     l_hvNew[l_i] = l_hvCur[l_i];
   }

**X-sweep** (vertical edges :math:`(i_x+\tfrac{1}{2},\, i_y)`).  The outer
loop over rows is parallelised.  Each row is independent: the left and
right net updates of edge :math:`(i_x, i_y)` only touch cells in row
:math:`i_y`, so no two threads write to the same cell:

.. code-block:: c++

   #pragma omp for schedule(static)
   for (t_idx l_iy = 1; l_iy <= m_nCells_y; l_iy++) {
     for (t_idx l_ix = 0; l_ix <= m_nCells_x; l_ix++) {
       // ... FWave::netUpdates + cell updates
     }
   }

**Y-sweep** (horizontal edges :math:`(i_x,\, i_y+\tfrac{1}{2})`).  Here
adjacent *rows* share cells: edge :math:`(i_x, i_y)` writes cell
:math:`(i_x, i_y{+}1)`, which edge :math:`(i_x, i_y{+}1)` also writes.
The naive fix — keep the outer ``i_y`` loop sequential and parallelise the
inner ``i_x`` loop — puts a barrier after *every one* of the 1667 rows, and
that synchronisation became the dominant cost on the full node (see
:ref:`results <parallel-outlook>`).

Instead we use a **red-black split over rows**.  Edges of the same parity
differ by :math:`\ge 2` and therefore write disjoint cells, so each parity
pass parallelises over rows (keeping the cache-friendly ``i_x``-inner order),
and the implicit barrier *between* the two passes separates the writes of
edge :math:`i_y` and edge :math:`i_y{-}1` to their shared cell.  This reduces
the barrier count from one-per-row to **two per sweep**:

.. code-block:: c++

   // even rows, then (barrier) odd rows
   #pragma omp for schedule(runtime)
   for (t_idx l_iy = 0; l_iy <= m_nCells_y; l_iy += 2)
     processYRow(l_iy);
   #pragma omp for schedule(runtime)
   for (t_idx l_iy = 1; l_iy <= m_nCells_y; l_iy += 2)
     processYRow(l_iy);

To answer the assignment's question directly: **the outer loop is the right
one to parallelise in both sweeps** — over rows in the X-sweep and over
same-colour rows in the Y-sweep — because it is coarse-grained and minimises
barriers.  Parallelising the *inner* loop (the naive Y-sweep) is correct but
far slower at scale due to the per-row barrier.

.. _parallel-firsttouch:

NUMA-aware initialisation (first-touch policy)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On multi-socket systems like NVIDIA Grace, each NUMA node has its own
memory.  The OS maps a page to the node of the *first* thread to touch it
(first-touch policy).  If all pages are touched by the main thread (e.g.
via ``new T[n]{}``), every access from a remote thread crosses a slow
inter-socket link.

To avoid this, the constructor allocates the arrays *without*
zero-initialisation and touches the pages in parallel with the same
``schedule(static)`` distribution the sweeps use.  A single loop over
``[0, l_size)`` touches the same flat index range in *every* array and in
*both* time buffers (``l_i`` and ``l_size + l_i``), so each thread owns the
identical range it later reads/writes in the copy and sweep phases:

.. code-block:: c++

   m_h = new t_real[2 * l_size];   // no {}
   // ...
   #pragma omp parallel for schedule(static)
   for (t_idx l_i = 0; l_i < l_size; l_i++) {
     m_h[l_i] = 0;  m_h[l_size + l_i] = 0;   // both buffers
     m_hu[l_i] = 0; m_hu[l_size + l_i] = 0;
     m_hv[l_i] = 0; m_hv[l_size + l_i] = 0;
     m_b[l_i] = 0;
   }

A flat loop over ``2 * l_size`` would split each buffer differently from the
per-buffer sweeps, leaving pages remote once the team spans more than one
node.  In practice this alignment is essential for correctness of the NUMA
story but, for the measured 5 M-cell problem, the kernel stays
synchronisation-bound past one socket, so it does not by itself remove the
108/144-thread drop (see :ref:`results <parallel-outlook>`).

1D Solver (``WavePropagation1d``)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The 1D edge loop cannot be parallelised directly: edge :math:`l` writes
the right-update to cell :math:`l{+}1`, while edge :math:`l{+}1` also
writes a left-update to cell :math:`l{+}1`, creating a write–write race.

The solution is a two-phase approach.  Pre-allocated per-edge arrays
(``m_netUpdatesL_h``, ``m_netUpdatesR_h``, etc.) store the updates from
each edge without conflicts:

.. code-block:: c++

   // Phase 1: compute — each edge writes to its own index (race-free)
   #pragma omp parallel for schedule(static)
   for (t_idx l_ed = 0; l_ed < m_nCells + 1; l_ed++) {
     // FWave/Roe::netUpdates -> m_netUpdatesL_*[l_ed], m_netUpdatesR_*[l_ed]
   }

   // Phase 2: apply — each cell is written exactly once
   #pragma omp parallel for schedule(static)
   for (t_idx l_ce = 1; l_ce <= m_nCells; l_ce++) {
     l_hNew[l_ce] = l_hOld[l_ce]
         - i_scaling * (m_netUpdatesR_h[l_ce-1] + m_netUpdatesL_h[l_ce]);
     // same for hu
   }

Cell :math:`l` receives the right-update of edge :math:`l{-}1` and the
left-update of edge :math:`l`, combining what was previously two serial
subtract-assignments into one parallel write.

Results & Benchmarks
---------------------

All benchmarks were run on a single **NVIDIA Grace** node with the Tohoku
2011 ``TsunamiEvent2d`` scenario, built with ``omp=1 opt=o3 arch=native``
(GCC). The grid was :math:`3000 \times 1667 \approx 5.0` million cells
(``-n 3000``, domain derived from the 250 m bathymetry file), simulated to
:math:`t = 600\,\text{s}` which is 411 time steps. The reported time is the
solver's own *compute wall-clock* (the time-stepping kernel only; NetCDF I/O
and the serial setup are excluded), driven by ``scripts/benchmark_omp.sh``.

The benchmark node has **two sockets of 72 cores each** (Grace, no SMT;
``nproc`` = 144, two NUMA nodes: cores 0–71 → node 0, 72–143 → node 1).

Speedup on NVIDIA Grace
~~~~~~~~~~~~~~~~~~~~~~~~

The speedup :math:`S_p = T_1 / T_p` and parallel efficiency :math:`S_p/p`
for thread counts from 1 to 144 (pinned with
``OMP_PROC_BIND=close OMP_PLACES=cores``, ``OMP_SCHEDULE=static``):

.. list-table::
   :header-rows: 1
   :widths: 15 25 20 20 20

   * - Threads :math:`p`
     - Wall-clock :math:`T_p` (s)
     - Speedup :math:`S_p`
     - Efficiency :math:`S_p/p`
     - Mcells/s
   * - 1
     - 46.90
     - 1.00
     - 1.00
     - 43.8
   * - 2
     - 23.84
     - 1.97
     - 0.98
     - 86.2
   * - 4
     - 12.01
     - 3.90
     - 0.98
     - 171.1
   * - 8
     - 6.06
     - 7.73
     - 0.97
     - 338.9
   * - 16
     - 3.06
     - 15.31
     - 0.96
     - 670.9
   * - 36
     - 1.44
     - 32.53
     - 0.90
     - 1425.5
   * - 72
     - 0.76
     - **62.06**
     - 0.86
     - 2720.0
   * - 108
     - 17.87
     - 2.62
     - 0.02
     - 115.0
   * - 144
     - 16.13
     - 2.91
     - 0.02
     - 127.4

Within a single socket the solver scales **near-ideally**: 62× on 72 threads
at 86 % efficiency (2.7 G cell-updates/s). With ``close``/``cores`` pinning,
threads ≤ 72 all land on socket 0, and the first-touch initialisation places
every page in node 0's memory, so all accesses stay local.

At **108 and 144 threads the team spans both sockets and throughput
collapses** (≈ 17 s, i.e. only ~3× over serial). This is *not* a data-placement
problem — aligning the first-touch initialisation with the sweep distribution
(see :ref:`Implementation <parallel-firsttouch>`) did not change these numbers.
It is the **strong-scaling limit combined with cross-socket synchronisation**:
the 5 M-cell / 411-step problem is small, so each time step does little work
per thread, while every step contains ~4 implicit barriers (copy, X-sweep,
two Y-sweep colours). Up to 72 threads those barriers are intra-socket and
cheap; beyond 72 they cross the inter-socket link and their cost dwarfs the
shrinking per-thread work. The second socket therefore cannot pay off at this
problem size — a larger domain is expected to keep scaling past 72 (see
:ref:`outlook <parallel-outlook>`).

Scheduling strategies
~~~~~~~~~~~~~~~~~~~~~~~

Three OpenMP scheduling strategies at 72 threads
(``OMP_PROC_BIND=close OMP_PLACES=cores``):

.. list-table::
   :header-rows: 1
   :widths: 30 20 20 30

   * - Strategy
     - :math:`T_p` (s)
     - Mcells/s
     - Notes
   * - ``static``
     - **0.758**
     - 2712
     - even chunks; preserves first-touch mapping
   * - ``guided``
     - 0.791
     - 2598
     - large initial chunks; locality mostly preserved
   * - ``dynamic,64``
     - 7.67
     - 268
     - tiny round-robin chunks; destroys first-touch mapping

``static`` is best, with ``guided`` within ~4 % — once the Y-sweep no longer
has a per-row barrier, ``guided``'s large initial chunks keep most accesses
local. ``dynamic,64`` is **10× slower**: handing out tiny 64-iteration chunks
round-robin scatters each thread's work across the array, so the pages it
touches are mostly remote. For this regular, uniform workload NUMA locality
matters far more than load balancing.

Pinning and NUMA effects
~~~~~~~~~~~~~~~~~~~~~~~~~~

Thread pinning at 72 threads (``OMP_SCHEDULE=static``), varying
``OMP_PROC_BIND`` and ``OMP_PLACES``:

.. list-table::
   :header-rows: 1
   :widths: 20 20 20 20 20

   * - ``OMP_PROC_BIND``
     - ``OMP_PLACES``
     - :math:`T_p` (s)
     - Mcells/s
     - Slowdown
   * - ``close``
     - ``cores``
     - **0.758**
     - 2713
     - 1.0× (best)
   * - ``spread``
     - ``cores``
     - 0.887
     - 2318
     - 1.2×
   * - ``spread``
     - ``sockets``
     - 22.27
     - 92
     - 29×
   * - ``close``
     - ``sockets``
     - 26.90
     - 76
     - 36×
   * - ``false``
     - ``cores``
     - 28.41
     - 72
     - 37×

This is a textbook demonstration of NUMA effects. **Pinning threads to cores
(``close``/``cores``) is essential** — it keeps each thread on the core whose
NUMA node holds its first-touched pages, so all 72 threads stay on socket 0
with purely local memory. Every configuration that lets threads cross the
socket boundary collapses by ~30–37×:

* ``OMP_PROC_BIND=false`` lets the OS migrate threads freely across all 144
  cores, so the first-touch mapping is meaningless and most accesses are remote.
* ``OMP_PLACES=sockets`` binds a thread to a *socket* but lets it float over
  all 72 cores of that socket and, with 72 threads, spreads the team over both
  sockets — again losing locality.
* ``spread``/``cores`` is correct but distributes the 72 threads across *both*
  sockets (instead of packing socket 0), so it pays some remote traffic and is
  1.2× slower than ``close``.

**Best configuration:** ``OMP_PROC_BIND=close OMP_PLACES=cores
OMP_SCHEDULE=static`` — the defaults used in the scaling sweep.

.. _parallel-outlook:

Outlook
~~~~~~~

The remaining limiter is the **cross-socket strong-scaling regime** at
108/144 threads, where global barriers across the inter-socket link dominate
the small per-thread workload. Two directions would let the second socket
contribute:

* **Larger domains.** More cells per thread amortise the per-step barriers;
  a higher-resolution run (e.g. ``-n 8000``) is expected to keep scaling past
  72 threads.
* **Fewer / NUMA-local barriers.** A domain decomposition with explicit halo
  exchange, or per-socket sub-teams, would replace the global barriers with
  cheaper socket-local ones.

Individual Contributions
-------------------------

- **Jan Vogt:**
- **Yannik Köllmann:**
- **Mika Brückner:** Enhanced OpenMP parallelisation of the 2D solver (red-black Y-sweep and NUMA-aware, sweep-aligned first-touch initialisation).
Added the ``--io-steps`` CLI option and fixed the serial-build compiler error (``-Wno-unknown-pragmas``).
Wrote the benchmark script ``scripts/benchmark_omp.sh``, ran the benchmarks on the NVIDIA Grace node.
