1. Riemann Solver
==================

Introduction
------------


Mathematical Background
------------------------


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

Steady-State Test
^^^^^^^^^^^^^^^^^^

Supersonic Case
^^^^^^^^^^^^^^^^


Results & Visualizations
-------------------------


Individual Contributions
-------------------------

- **Yannik Köllmann:** Project setup (1.2), including forking the repository,
  build configuration with SCons, Doxygen setup, initial Catch2 integration and .gitignore setup.
  Implementation of the ``flux`` and ``waveStrengths`` methods for the f-wave solver.
- **Jan Vogt:** ...
- **Mika Brückner:** ...