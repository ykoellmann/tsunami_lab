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

Compilers (8.2)
----------------

Generic Compiler Support (8.2.1)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The build script was extended to honour the ``CXX`` environment variable so
that any compiler can be selected without editing ``SConstruct``.  The
surrounding shell environment is forwarded into SCons (``ENV = os.environ``)
and, if present, ``CXX`` overrides the compiler:

.. code:: python

   env = Environment( variables = vars, ENV = os.environ )
   if 'CXX' in os.environ:
     env['CXX'] = os.environ['CXX']

A build with Clang is therefore a one-liner: ``CXX=clang++ scons``.  Four
further build options were added for the compiler study:

.. list-table::
   :header-rows: 1
   :widths: 35 65

   * - Option
     - Effect
   * - ``opt=o2|o3|ofast``
     - optimization level (``-O2`` / ``-O3`` / ``-Ofast``)
   * - ``arch=none|native|icelake-server``
     - target architecture (``-march=...``); enables AVX-512 on Draco
   * - ``report=0|1``
     - Clang optimization remarks (``-Rpass=.*`` …)
   * - ``inline=0|1``
     - toggle inlining (``-fno-inline``)

Optimization Switches and Numerical Implications (8.2.3)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``-O2`` and ``-O3`` are IEEE-754 conforming.  ``-Ofast`` additionally enables
``-ffast-math``, which relaxes the floating-point model: it allows
reassociation of operations, assumes no ``NaN``/``Inf`` operands
(``-ffinite-math-only``) and flushes denormals to zero.  This permits faster
code — vectorizable reductions, cheaper ``sqrt``/division — but can change
results in the last bits and breaks code that relies on IEEE corner cases.
The f-wave solver only operates on well-behaved positive water heights, so the
relaxed model is safe here and the simulation output is unaffected.

A subtle side effect surfaced during the study: under ``-ffast-math`` the LLVM
compilers (``clang++`` and ``icpx``) fold ``sin(pi*x)`` in the
``ArtificialTsunami2d`` setup into calls to ``sinpif``/``cospif`` from Intel's
math library ``libimf``.  ``icpx`` links ``libimf`` automatically, but plain
``clang++`` does not, producing an *undefined reference* at link time.  The
build script therefore links ``-limf`` explicitly for the LLVM compilers;
GCC + glibc never emit these symbols.

Compiler and Flag Comparison (Compilers 2 & 3)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

All three compilers available on Draco — GCC 12.2, the bundled upstream LLVM
``clang++`` and Intel's ``icpx`` — were benchmarked on an exclusive ``short``
node.  Each variant is recompiled from scratch and run back-to-back on the
*same* Tohoku scenario (250 m bathymetry, ``-n 1000``, ``-t 7200``) by a single
batch script, so the only difference is the compiler / flags.  Reported is the
normalized *time per cell and iteration* in nanoseconds (lower is better):

.. list-table::
   :header-rows: 1
   :widths: 22 14 14 16 24

   * - Compiler
     - ``-O2``
     - ``-O3``
     - ``-Ofast``
     - ``-Ofast -march=native``
   * - GCC 12.2
     - 46.08
     - 41.04
     - 37.01
     - **30.34**
   * - Clang (LLVM)
     - 45.09
     - 42.78
     - 40.31
     - 37.22
   * - icpx
     - 38.74
     - —
     - 40.42
     - 37.04

Observations:

- **Vectorization is the single largest lever.**  Adding ``-march=native``
  (AVX-512 on the Ice Lake nodes) cuts GCC from 37.01 to 30.34 ns (−18 %) and
  Clang/icpx by ≈ 8 %.  Without ``-march`` the compilers target the generic
  x86-64 baseline (SSE2) and cannot use the wide vector units.  This answers
  the task's question: *yes*, the per-edge time-step loop is auto-vectorizable,
  but only once a suitable architecture is specified.

- **GCC produces the fastest code** at its best setting — 30.34 ns vs. ≈ 37.2 ns
  for both LLVM compilers, a ≈ 19 % lead.

- **Optimization level scales as expected for GCC:** ``-O2`` → ``-O3`` gains
  ≈ 11 %, and the step to ``-Ofast`` another ≈ 10 % from ``-ffast-math``
  (reassociation and cheaper ``sqrt``/division in the solver).

- **icpx defaults to fast-math.**  Its ``-O2`` already uses the relaxed
  floating-point model, so ``icpx -O2`` (38.74 ns) is *not* directly comparable
  to the IEEE-strict ``gcc -O2`` / ``clang -O2``, and ``-Ofast`` brings it no
  further benefit.  The strict baselines are consistent at ≈ 45–46 ns.

Absolute numbers carry a few percent of run-to-run noise (e.g. ``gcc -O2``
measured 40–46 ns across batch jobs); the relative ordering above is stable
across repetitions.

Vectorization, Inlining and Reports (8.2.4)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Disabling inlining isolates its cost:

.. list-table::
   :header-rows: 1
   :widths: 55 25

   * - Variant
     - :math:`t_{\text{cell,iter}}`
   * - ``gcc opt=ofast``
     - 37.01 ns
   * - ``gcc opt=ofast inline=0``
     - 62.59 ns

Turning off inlining makes the solver **69 % slower**.  This confirms that the
f-wave solver *is* inlined under optimization: ``waveSpeeds``,
``waveStrengths``, ``flux`` and ``deltaXPsi`` are folded into ``netUpdates``,
removing four function calls per cell edge per timestep.  Inlining is also a
prerequisite for vectorization — only the inlined loop body exposes the
independent per-edge work that ``-march=native`` can then vectorize, which is
why the two largest speedups (inlining and SIMD) are coupled.

The ``report=1`` option adds Clang's ``-Rpass=.*``, ``-Rpass-missed=.*`` and
``-Rpass-analysis=.*`` remark flags.  Building with ``report=1`` was used to
inspect the time-step loop: with ``arch=native`` the inner loop is reported as
vectorized (``loop-vectorize``), whereas at the generic baseline the same loop
appears under ``-Rpass-missed`` — consistent with the measured AVX-512 speedup
and with the f-wave functions being inlined into the hot loop.

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
- **Mika Brückner:** Compiler support in the build script (``CXX``
  environment variable; the ``opt``, ``arch``, ``report`` and ``inline``
  options).  Compiler and optimization-flag study on Draco (GCC vs. Clang
  vs. icpx; ``-O2``/``-O3``/``-Ofast``; ``-march=native``/AVX-512; inlining),
  including the automated batch benchmark, the ``libimf``/``sinpif`` link
  fix and the analysis in the *Compilers* section.
