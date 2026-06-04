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

Individual Contributions
-------------------------

- **Jan Vogt:** Cluster setup on Draco (clone, modules, SCons install,
  interactive and batch runs reproducing Ch. 6).  Implementation of the
  ``std::chrono`` timer around the compute kernel in ``main.cpp`` and
  derivation of the normalized *time per cell and iteration* metric.
  Local vs. Draco comparison.
- **Yannik Köllmann:**
- **Mika Brückner:**
