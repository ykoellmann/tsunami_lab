1. Riemann Solver
==================

Implementation
---------------

The f-wave solver is split into separate private methods for each computation step:
``waveSpeeds``, ``waveStrengths``, ``flux``, and the public ``netUpdates`` method.

The ``flux`` method computes the flux function :math:`f(q) = [hu, \; hu^2 + \frac{1}{2}g h^2]^T`
for a given state. The ``waveStrengths`` method uses the flux difference
:math:`\Delta f = f(q_r) - f(q_l)` and decomposes it into the eigenvectors
by solving the 2x2 system to obtain the coefficients :math:`\alpha_1, \alpha_2`.

A key difference to the existing Roe solver is the wave computation:
the Roe solver multiplies the wave speed into the wave (e.g. :math:`\lambda \cdot \alpha`),
whereas the f-wave solver computes :math:`Z_p = \alpha_p \cdot r_p` directly,
i.e. :math:`Z_p[0] = \alpha_p` and :math:`Z_p[1] = \alpha_p \cdot \lambda_p`.


Unit Tests
-----------

Eigenvalue Verification
^^^^^^^^^^^^^^^^^^^^^^^^

Tested via ``[FWaveSpeeds]``: given :math:`h_l = 10, u_l = -3` and :math:`h_r = 9, u_r = 3`,
the Roe averages are computed as :math:`h^{Roe} = 9.5` and :math:`u^{Roe} = -0.079002...`,
yielding the eigenvalues:

.. math::

   \lambda_1 = -9.7311093998375095, \quad \lambda_2 = 9.5731051658991654

Additionally, ``[FWaveFlux]`` verifies the flux computation for
:math:`(h, hu) = (10, -30)` and :math:`(h, hu) = (9, 27)`,
and ``[FWaveStrengths]`` verifies the wave strength decomposition
:math:`[\alpha_1, \alpha_2]^T = R^{-1} \cdot \Delta f` for the same input values,
yielding :math:`\alpha_1 = 33.559...` and :math:`\alpha_2 = 23.441...`.


Steady-State Test
^^^^^^^^^^^^^^^^^^

Tested as part of ``[FWaveUpdates]``: given :math:`q_l = q_r = [10, 0]^T`,
the flux jump is zero:

.. math::

   \Delta f = f(q_r) - f(q_l) = 0

Therefore all wave strengths and net-updates are zero up to machine precision:

.. math::

   A^- \Delta Q = A^+ \Delta Q = 0


Supersonic Case
^^^^^^^^^^^^^^^^

Two supersonic cases are tested as part of ``[FWaveUpdates]``:

**Supersonic right** (:math:`\lambda_1, \lambda_2 > 0`): given :math:`h_l = 1, hu_l = 50`
and :math:`h_r = 2, hu_r = 100`, the Roe velocity :math:`u^{Roe} = 50` exceeds
the wave speed :math:`\sqrt{g \cdot h^{Roe}} = \sqrt{9.80665 \cdot 1.5} \approx 3.834`,
yielding :math:`\lambda_1 \approx 46.165 > 0` and :math:`\lambda_2 \approx 53.835 > 0`.
Both waves travel to the right, so the left net-update is zero:

.. math::

   A^- \Delta Q = 0, \quad A^+ \Delta Q = \Delta f

**Supersonic left** (:math:`\lambda_1, \lambda_2 < 0`): symmetric case with negated momentum
:math:`hu_l = -50, hu_r = -100`, yielding :math:`\lambda_1 \approx -53.835 < 0`
and :math:`\lambda_2 \approx -46.165 < 0`.
Both waves travel to the left, so the right net-update is zero:

.. math::

   A^+ \Delta Q = 0, \quad A^- \Delta Q = \Delta f


Results & Visualizations
-------------------------

All unit tests passed as expected.

Individual Contributions
-------------------------

- **Yannik Köllmann:** Project setup (1.2), including forking the repository,
  build configuration with SCons, Doxygen setup, initial Catch2 integration and .gitignore setup.
  Implementation of the ``flux`` and ``waveStrengths`` methods for the f-wave solver.
  Adding release of Sphinx and Doxygen to GitHub Pages using GitHub Actions.
- **Jan Vogt:** CI/CD pipeline setup using GitHub Actions, including automated style checking
  with clang-format (``clang-format --dry-run --Werror``).
  Added ``.clang-format`` configuration (LLVM style, indent width 2) and ``.clangd`` config
  for IDE support in CLion. Extended the Nix shell with ``clang-tools`` and ``bear`` for local style
  checking and IDE support.
- **Mika Brückner:** Implementation of the ``waveSpeeds`` and ``netUpdates`` methods
  for the f-wave solver (task 1.3.1). Build configuration to include the FWave solver
  and tests (scons). Implementation of unit tests for ``waveSpeeds``, ``flux``, ``waveStrengths``,
  and ``netUpdates``, including steady-state and supersonic cases (task 1.3.2).
