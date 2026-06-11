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

Speedup on NVIDIA Grace
~~~~~~~~~~~~~~~~~~~~~~~~

The speedup :math:`S_p = T_1 / T_p` was measured on the NVIDIA Grace
cluster running the Tohoku 2011 scenario (250 m resolution, 1000 time
steps) for thread counts from 1 to 144.

.. list-table::
   :header-rows: 1
   :widths: 20 25 25 20

   * - Threads :math:`p`
     - Wall-clock :math:`T_p` (s)
     - Speedup :math:`S_p`
     - Efficiency :math:`S_p/p`
   * - 1
     - —
     - 1.0
     - 1.00
   * - 4
     - —
     - —
     - —
   * - 16
     - —
     - —
     - —
   * - 72
     - —
     - —
     - —
   * - 144
     - —
     - —
     - —

*(Numbers to be filled in after Grace benchmarks.)* TODO

Scheduling strategies
~~~~~~~~~~~~~~~~~~~~~

Three OpenMP scheduling strategies were compared on Grace.  All runs use
``opt=o3 arch=native omp=1``:

.. list-table::
   :header-rows: 1
   :widths: 35 30 35

   * - Strategy
     - :math:`T_p` at 72 threads (s)
     - Notes
   * - ``schedule(static)``
     - —
     - default; even chunks, predictable first-touch
   * - ``schedule(dynamic,64)``
     - —
     - load-balancing; invalidates first-touch mapping
   * - ``schedule(guided)``
     - —
     - decreasing chunks; mixed NUMA behaviour

*(Numbers to be filled in after Grace benchmarks.)* TODO

``static`` is expected to perform best because it preserves the first-touch
mapping: the same thread that touched a page during initialisation
processes that page during the sweep.  Dynamic scheduling can improve
load balance for irregular workloads but negates the NUMA optimisation.

Individual Contributions
-------------------------

- **Jan Vogt:**
- **Yannik Köllmann:**
- **Mika Brückner:**
