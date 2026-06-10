8. Optimization
================

Implementation
---------------

Cluster Setup (1.1 & 1.2)
~~~~~~~~~~~~~~~~~~~~~~~~~~

The repository was cloned directly on the Draco login node and the Chapter 6
input data was fetched via ``wget``.  Compilation uses the same SCons-based
build as locally; SCons itself was installed once into the user environment
through ``module load tools/python/3.8`` followed by ``pip install --user
scons``.  Compilation runs on the login node, simulations strictly on
compute nodes — either through an interactive allocation
(``salloc --partition=short``) or via ``sbatch``.

Reproducing the Chapter 6 Chile event on Draco yields a NetCDF result that
matches the local run bit-for-bit (same domain, same :math:`\Delta t`, same
checkpoint), confirming that the build behaves identically on both machines.

Time Measurement (1.3)
~~~~~~~~~~~~~~~~~~~~~~~

To compare the cluster against the local machine we added a wall-clock
timer to ``main.cpp`` that wraps **only** the compute kernel
(``setGhost`` + ``timeStep``).  File I/O (NetCDF / CSV / station writes),
the initial cell sampling and all allocations are deliberately *outside* the
measured region, as requested by the task.

.. code:: c++

    std::chrono::nanoseconds l_computeDuration{0};
    tsunami_lab::t_idx l_iterCount = 0;
    while (l_simTime < l_endTime) {
      // ... I/O and station writes (not timed) ...
      auto l_tic = std::chrono::steady_clock::now();
      l_waveProp->setGhost(l_bcLeft, l_bcRight);
      l_waveProp->timeStep(l_scaling, l_solverMode);
      l_computeDuration += std::chrono::steady_clock::now() - l_tic;
      l_iterCount++;
      l_timeStep++;
      l_simTime += l_dt;
    }

After the loop the solver prints the totals and the normalized metric
*time per cell and iteration*:

.. math::

   t_{\text{cell,iter}}
   \;=\;
   \frac{t_{\text{compute}}}{n_x \cdot n_y \cdot N_{\text{iter}}}

This metric is independent of grid size and run length and is therefore the
quantity used to compare the two machines.

Results & Visualizations
--------------------------

Both runs use the same setup, the same input files and the same end time on
a checkpoint of the Chile scenario:

.. code-block:: bash

   ./build/tsunami_lab -n 500 -t 1000 -p TsunamiEvent2d \
       ressources/chile/output/chile_gebco20_usgs_250m_bath_fixed.nc \
       ressources/chile/output/chile_gebco20_usgs_250m_displ_fixed.nc

Resulting domain: :math:`500 \times 421 = 210{,}500` cells at a cell size of
:math:`7000\,\text{m}`.  Both runs resumed from the same checkpoint at
:math:`t \approx 931.63\,\text{s}` and executed 6 timesteps to reach
:math:`t = 1000\,\text{s}`.

.. list-table::
   :header-rows: 1
   :widths: 30 25 25 25

   * - Machine
     - Compute wall-clock
     - Iterations
     - :math:`t_{\text{cell,iter}}`
   * - Local (macOS)
     - :math:`0.1079\,\text{s}`
     - 6
     - :math:`\approx 85.5\,\text{ns}`
   * - Draco ``node016``
     - :math:`0.0683\,\text{s}`
     - 6
     - :math:`\approx 54.1\,\text{ns}`

The compute kernel runs roughly **1.58× faster** per cell and iteration on
the Draco compute node compared to the local machine.  Note that the sample
covers only 6 iterations because both runs were resumed from a checkpoint
close to the end time; a longer run gives a more stable estimate, but the
relative ordering is consistent across repetitions.

Instrumentation and Performance Counters (8.3)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

VTune Profiler was made available on Draco via:

.. code-block:: bash

   export PATH=/cluster/intel/oneapi/2025.0.0/vtune/2025.0/bin64:$PATH

The GUI was launched on the login node with X-forwarding (``ssh -X``).
A Hotspots analysis was configured for the solver binary and executed on a
compute node — interactively first, then as a batch job.  The solver was
recompiled with ``scons inline=0`` (enables ``-g -fno-inline``) for
finer-grained results.

.. list-table::
   :header-rows: 1
   :widths: 45 20 15

   * - Function
     - CPU Time
     - % of CPU Time
   * - ``tsunami_lab::solvers::FWave::netUpdates``
     - 16.288 s
     - 34.8 %
   * - ``tsunami_lab::solvers::FWave::waveStrengths``
     - 13.297 s
     - 28.4 %
   * - ``tsunami_lab::patches::WavePropagation2d::timeStep``
     - 11.925 s
     - 25.5 %
   * - ``tsunami_lab::solvers::FWave::waveSpeeds``
     - 3.000 s
     - 6.4 %
   * - ``NC_get_vara`` (libnetcdf)
     - 1.510 s
     - 3.2 %

The F-Wave solver functions dominate with ~95 % of CPU time — specifically
``netUpdates``, ``waveStrengths``, ``timeStep`` and ``waveSpeeds``.  This
was expected: ``netUpdates`` is called for every cell edge at every
timestep, making it the innermost hot loop of the simulation.  Physical
core utilization was 2.0 % (0.941 / 48), confirming the solver is
single-threaded.

The following optimizations target the hotspots directly:

- **Inlining** — with ``inline=0`` (profiling build), ``waveStrengths``,
  ``waveSpeeds``, ``flux`` and ``deltaXPsi`` are all separate call sites
  inside ``netUpdates``.  Re-enabling inlining folds all of them into a
  single function body, eliminating four function calls per edge per
  timestep at :math:`O(n_x \cdot n_y)` call sites.

- **Redundant ``sqrt``** — ``waveSpeeds`` computes three square roots:
  ``std::sqrt(i_hL)``, ``std::sqrt(i_hR)`` and ``std::sqrt(l_hRoe)``.
  The third can be replaced by :math:`0.5(\sqrt{h_L} + \sqrt{h_R})`,
  reusing the already-computed ``l_hSqrtL`` and ``l_hSqrtR`` and saving
  one ``sqrt`` call per edge.

- **Redundant division** — ``flux`` is called twice inside
  ``waveStrengths`` and each call computes ``i_hu / i_h``.  The
  velocities ``l_uL`` and ``l_uR`` are already computed in ``netUpdates``
  and could be passed into ``waveStrengths`` directly to avoid the
  redundant division.

- **SIMD vectorization** — building with ``arch=icelake-server`` enables
  AVX-512 on Draco's Ice Lake nodes.  The per-edge computation in
  ``timeStep`` is independent across edges; with inlining enabled the
  compiler can auto-vectorize the loop and process multiple edges per
  clock cycle.

Individual Contributions
-------------------------

- **Jan Vogt:** Cluster setup on Draco (clone, modules, SCons install,
  interactive and batch runs reproducing Ch. 6).  Implementation of the
  ``std::chrono`` timer around the compute kernel in ``main.cpp`` and
  derivation of the normalized *time per cell and iteration* metric.
  Local vs. Draco comparison.
- **Yannik Köllmann:** VTune Hotspots analysis (interactive and batch),
  profiling build (``scons inline=0``), hotspot interpretation and documentation of section 8.3.
- **Mika Brückner:**
