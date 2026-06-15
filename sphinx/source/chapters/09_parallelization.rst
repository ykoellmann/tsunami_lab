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
adjacent *rows* share cells (edge :math:`(i_x, i_y)` writes to cell
:math:`(i_x, i_y{+}1)`, which is also written by edge
:math:`(i_x, i_y{+}1)`), so the outer loop must stay sequential.  The
inner loop over columns is parallelised instead — for a fixed :math:`i_y`,
different :math:`i_x` values write to distinct cells.  The implicit
barrier after each ``#pragma omp for`` prevents row :math:`i_y` and
:math:`i_y{+}1` from racing:

.. code-block:: c++

   for (t_idx l_iy = 0; l_iy <= m_nCells_y; l_iy++) {
     #pragma omp for schedule(static)
     for (t_idx l_ix = 1; l_ix <= m_nCells_x; l_ix++) {
       // ... FWave::netUpdates + cell updates
     }
   }

To answer the assignment's question directly: **parallelising the outer
loop is better for the X-sweep** (more coarse-grained work, better cache
reuse per thread), while **the inner loop must be parallelised for the
Y-sweep** due to the row-shared-cell dependency.

NUMA-aware initialisation (first-touch policy)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On multi-socket systems like NVIDIA Grace, each NUMA node has its own
memory.  The OS maps a page to the node of the *first* thread to touch it
(first-touch policy).  If all pages are touched by the main thread (e.g.
via ``new T[n]{}``), every access from a remote thread crosses a slow
inter-socket link.

To avoid this, the constructor allocates the arrays *without*
zero-initialisation and then touches the pages in parallel using the same
``schedule(static)`` that the sweeps use:

.. code-block:: c++

   m_h  = new t_real[2 * l_size];   // no {}
   // ...
   #pragma omp parallel for schedule(static)
   for (t_idx l_i = 0; l_i < 2 * l_size; l_i++) {
     m_h[l_i] = 0;  m_hu[l_i] = 0;  m_hv[l_i] = 0;
   }

Because the static block distribution assigns the same index range to the
same thread in both the initialisation and the sweep loops, each thread
processes pages that the OS has already mapped to its local NUMA node.

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
     - 47.37
     - 1.00
     - 1.00
     - 43.4
   * - 2
     - 24.90
     - 1.90
     - 0.95
     - 82.5
   * - 4
     - 13.27
     - 3.57
     - 0.89
     - 154.9
   * - 8
     - 7.24
     - 6.54
     - 0.82
     - 283.8
   * - 16
     - 4.41
     - 10.74
     - 0.67
     - 465.9
   * - 36
     - 3.85
     - **12.31**
     - 0.34
     - 534.0
   * - 72
     - 4.82
     - 9.83
     - 0.14
     - 426.7
   * - 108
     - 52.15
     - 0.91
     - 0.01
     - 39.4
   * - 144
     - 57.27
     - 0.83
     - 0.01
     - 35.9

The solver scales well up to about 16 threads (efficiency ≥ 0.67) and
reaches its **best absolute speedup of 12.3× at 36 threads** (534 Mcells/s).
Beyond that, throughput drops and at 108/144 threads it collapses to *below*
serial performance. The Grace Superchip is two 72-core dies on separate NUMA
nodes; once the thread team spans both dies, two effects dominate:

* The **Y-sweep parallelises the inner loop** (a barrier after every one of
  the 1667 rows per step). At 100+ threads this synchronisation — now across
  the inter-die link — costs far more than the work it guards.
* Pages mapped by the first-touch initialisation on die 0 are accessed
  remotely by threads on die 1.

The inner-loop Y-sweep is therefore the primary scalability bottleneck on
the full node; see :ref:`the outlook below <parallel-outlook>`.

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
     - **4.90**
     - 419
     - even chunks; preserves first-touch mapping
   * - ``dynamic,64``
     - 19.02
     - 108
     - load-balancing; invalidates first-touch mapping
   * - ``guided``
     - 32.79
     - 63
     - decreasing chunks; worst NUMA behaviour

``static`` wins by a wide margin (≈ 4× over ``dynamic``, ≈ 7× over
``guided``) because it preserves the first-touch mapping: the same thread
that touched a page during initialisation processes it during the sweep.
``dynamic`` and ``guided`` hand chunks to whichever thread is free, so most
accesses become remote — confirming that for this regular, uniform workload
NUMA locality matters far more than load balancing.

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
     - **4.98**
     - 412
     - 1.0× (best)
   * - ``spread``
     - ``cores``
     - 8.70
     - 236
     - 1.7×
   * - ``false``
     - ``cores``
     - 356.52
     - 5.8
     - 72×
   * - ``close``
     - ``sockets``
     - 376.31
     - 5.5
     - 76×
   * - ``spread``
     - ``sockets``
     - 376.30
     - 5.5
     - 76×

This is a textbook demonstration of NUMA effects. **Pinning threads to
cores (``close``/``cores``) is essential** — it keeps each thread on the
core whose NUMA node holds its first-touched pages. Disabling binding
(``OMP_PROC_BIND=false``) lets the OS migrate threads freely, destroying the
first-touch mapping and making nearly every access remote: a **70× slowdown**.
``OMP_PLACES=sockets`` is just as catastrophic, because threads may float
across all cores of a socket and lose locality. ``spread`` over cores is
correct but ~1.7× slower than ``close`` here, since spreading the team
across both dies again increases remote traffic for this memory-bound kernel.

**Best configuration:** ``OMP_PROC_BIND=close OMP_PLACES=cores
OMP_SCHEDULE=static`` — exactly the defaults used in the scaling sweep.

.. _parallel-outlook:

Outlook
~~~~~~~

The dominant remaining bottleneck is the **inner-loop parallelisation of the
Y-sweep** with its per-row barrier, which prevents scaling beyond one Grace
die. Replacing it with a parallel *outer* loop over independent column blocks
(or a red-black / two-buffer scheme that removes the row dependency) would cut
the barrier count from one-per-row to one-per-sweep and is expected to restore
scaling toward 144 threads.

Individual Contributions
-------------------------

- **Jan Vogt:**
- **Yannik Köllmann:**
- **Mika Brückner:**
