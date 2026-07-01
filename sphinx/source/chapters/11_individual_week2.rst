11. Individual Phase Week 2 — Live Simulation & Visual Overhaul
================================================================

Overview
--------

The visualizer now runs the 2D solver in real time: a background thread
drives the simulation while the render loop reads each completed frame
without blocking.  Alongside this, the colormap and legend system was
overhauled, and the Okada fault-displacement model was studied in
preparation for replacing the Gaussian placeholder.

Implementation
--------------

Real-time wave simulation
~~~~~~~~~~~~~~~~~~~~~~~~~~

``SolverThread`` runs ``WavePropagation2d`` on a dedicated thread and
publishes each new frame into a lock-free ``SimBuffer`` ring.  The
renderer reads the latest complete frame without stalling the solver.
Time metadata (simulated time, step count) is propagated alongside the
cell data so the HUD can show live progress.

.. figure:: ../_images/individual_phase/live_simulation.png
   :alt: Region view with live wave simulation running
   :width: 80%

   Live simulation: wave height updated every rendered frame.

.. raw:: html

   <!-- VIDEO: live_simulation.mp4 -->

Jet wave colormap & hypsometric terrain
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The wave-height overlay now uses a **jet** (blue → cyan → green → yellow
→ red) scale with stable, data-driven bounds so the colour does not jump
between frames.  The bathymetry switched to a **hypsometric** palette
(deep-ocean blue → shelf → coastal green → highland brown) matching
standard cartographic convention.

Stacked legends
~~~~~~~~~~~~~~~~

The sidebar shows two stacked gradient legends — terrain elevation
(fixed hypsometric scale) and wave height (dynamic, centred on zero) —
each with labelled tick marks.

.. figure:: ../_images/individual_phase/stacked_legends.png
   :alt: Stacked terrain and wave-height legends
   :width: 40%

   Stacked legends: hypsometric bathymetry (top) and jet wave scale
   (bottom).

Globe view resolution control
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A resolution slider in the globe view controls mesh density, allowing
finer detail at high zoom and faster interaction at coarser settings.

Shader refactor
~~~~~~~~~~~~~~~~

All inline GLSL was extracted into ``.vert`` / ``.frag`` files under
``src/visualization/shaders/``.  A thin ``Shader.h`` helper loads and
compiles them at startup.

2D solver fixes
~~~~~~~~~~~~~~~~

Two bugs that caused value explosions in the 2D X/Y sweeps were fixed in
``WavePropagation2d`` and ``main.cpp``.  Dead code and stale comments
were removed as part of the same pass.

Okada model (research & documentation)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Okada (1985/1992) analytical fault-displacement model was studied as
the physically correct replacement for ``GaussianDisplacement``.  The
model computes static surface deformation from a rectangular fault
(strike, dip, rake, slip, depth) in an elastic half-space.  Key formulas
and parameter conventions are documented; implementation is planned for
next week.

Individual Contributions
-------------------------

- **Jan Vogt:** ``SolverThread`` / ``SimBuffer`` design and
  implementation with unit tests; jet wave colormap; hypsometric terrain
  colormap; stacked legend rendering; stable colour-scale bounds; globe
  resolution control; SCons artefact cleanup.
- **Yannik Köllmann:** 2D solver bug fixes (value explosions); shader
  extraction and ``Shader.h``; time metadata in ``SolverThread``; HUD
  display of simulated time.
- **Mika Brückner:** Okada fault-displacement model — literature study,
  parameter documentation, and chapter writeup.
