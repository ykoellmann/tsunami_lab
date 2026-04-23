3. Bathymetry & Boundary Conditions
====================================

Implementation
--------------

Für reflektierende Ränder (3.2.1) haben wir einen
``BoundaryCondition``-Enum (``Outflow`` / ``Reflecting``) und eine
neue Methode ``setGhost(left, right)`` auf ``WavePropagation1d``
hinzugefügt. Eine reflektierende Ghost-Zelle kopiert :math:`h` und
negiert :math:`hu`; eine Outflow-Zelle kopiert beides.
``setGhostOutflow()`` ruft jetzt einfach
``setGhost(Outflow, Outflow)``.

Über die CLI-Flags ``--bc-left`` / ``--bc-right``
(``outflow`` / ``reflecting``, Default ``outflow``) wird der
Randtyp pro Seite gewählt.

Für 3.2.2 reicht ein normales ``DamBreak`` mit reflektierendem
rechten Rand — die Reflexion erzeugt das gleiche Verhalten wie ein
symmetrisches ShockShock-Problem an der Wand.

Unit Tests
----------

``[WaveProp1dReflecting]`` prüft die drei Randkombinationen
(``Reflecting/Reflecting``, ``Outflow/Reflecting``,
``setGhostOutflow`` ≡ ``Outflow/Outflow``) und verifiziert, dass
:math:`h` kopiert und :math:`hu` nur auf reflektierenden Seiten
negiert wird.

3.4. Bathymetry Data Extraction
--------------------------------

We extract bathymetry data from the GEBCO 2025 Grid using GMT.
The workflow is automated in ``scripts/extract_bathymetry.sh``:

.. code-block:: bash

   ./scripts/extract_bathymetry.sh tohoku --map --pdf

The script downloads the GEBCO data if not present, cuts the specified
region with ``gmt grdcut``, extracts a 1D profile via ``gmt grdtrack``,
and converts the output to CSV. Optionally it generates a map with
coastlines and the profile line overlaid.

For the domain between :math:`p_1 = (141.024949, 37.316569)` and
:math:`p_2 = (146.0, 37.316569)` at 250 m sampling, the extraction
yields 1903 data points. The profile CSV is stored in ``ressources/``,
map visualizations go to ``simulations/visualizations/<name>/``.

.. TODO: Add bathymetry profile/map figure.

Results & Visualizations
------------------------

Observations
""""""""""""

Setup: ``DamBreak 15 10`` (Dammposition je Szenario).
Beim Aufprall auf eine Wand kippt das Momentum **nicht** einfach
um — das Wasser staut sich kurz auf (:math:`u \to 0`), dann läuft
ein neuer Shock mit erhöhter Wasserhöhe zurück.

Results:
""""""""

Einseitige Reflexion (rechter Rand):

.. image:: ../../../simulations/visualizations/bathymetry_boundary_conditions/dam_reflect.gif
   :width: 40%

Beidseitige Reflexion (Wellen bouncen zwischen beiden Wänden):

.. image:: ../../../simulations/visualizations/bathymetry_boundary_conditions/dam_reflect_both_walls.gif
   :width: 40%

Hydraulic Jumps (3.3)
====================

**TODO Add visualizations**

Subcritical Flow
----------------

The subcritical setup uses a parabolic hump over the interval :math:`x \in (8, 12)`
on the domain :math:`(0, 25)` with bathymetry

.. math::

   b(x) = \begin{cases}
   -1.8 - 0.05 (x-10)^2 & \text{if } x \in (8,12) \\
   -2 & \text{else}
   \end{cases}

and initial conditions :math:`h(x, 0) = -b(x)`, :math:`hu(x, 0) = 4.42`.

**Maximum Froude number at** :math:`t = 0`

The Froude number is defined as

.. math::

   F(x) := \frac{u}{\sqrt{g h}} = \frac{hu}{h \sqrt{g h}}

Since :math:`hu = 4.42` is constant and :math:`h = -b(x)`, the Froude number is
maximized where :math:`h` is minimal, i.e., at the hump peak :math:`x = 10` where
:math:`h(10) = 1.8`.

.. math::

   F_{\max} = \frac{4.42}{1.8 \cdot \sqrt{9.80665 \cdot 1.8}}
            = \frac{4.42}{1.8 \cdot 4.201}
            \approx 0.584

Since :math:`F_{\max} < 1` everywhere, the flow remains subcritical throughout
the entire domain at :math:`t = 0`.

In the steady state the f-wave solver converges to a solution where the free
surface :math:`\eta = h + b` is approximately constant and the momentum
:math:`hu \approx 4.42` is preserved across the domain, consistent with
conservation of mass in a stationary 1D flow.

Supercritical Flow and Hydraulic Jump
--------------------------------------

The supercritical setup uses the same domain with bathymetry

.. math::

   b(x) = \begin{cases}
   -0.13 - 0.05 (x-10)^2 & \text{if } x \in (8,12) \\
   -0.33 & \text{else}
   \end{cases}

and initial conditions :math:`h(x, 0) = -b(x)`, :math:`hu(x, 0) = 0.18`.

**Maximum Froude number at** :math:`t = 0`

At the hump peak :math:`x = 10` the water height is :math:`h(10) = 0.13`.
Away from the hump the water height is :math:`h = 0.33`. The Froude numbers are:

.. math::

   F(10) = \frac{0.18}{0.13 \cdot \sqrt{9.80665 \cdot 0.13}} \approx 1.23
   \qquad
   F_{\text{flat}} = \frac{0.18}{0.33 \cdot \sqrt{9.80665 \cdot 0.33}} \approx 0.30

The flow transitions from subcritical (:math:`F < 1`) in the flat regions to
supercritical (:math:`F > 1`) over the hump peak. The location of the maximum
Froude number is :math:`x = 10` with :math:`F_{\max} \approx 1.23`.

**Hydraulic jump and failure to converge**

In the supercritical case a stationary shock — a hydraulic jump — forms
downstream of the hump where the flow transitions back from supercritical to
subcritical. Analytically, conservation of mass in a stationary 1D flow without
sources requires :math:`hu = \text{const} = 0.18` everywhere.

The f-wave solver, however, fails to converge to this analytical solution.
After running the simulation to :math:`t = 200` the momentum :math:`hu` is not
constant across the domain: a clear discontinuity persists near the hydraulic
jump location. Refining the grid does not resolve this discrepancy — the solver
does not converge to the expected constant momentum, demonstrating a known
limitation of the standard f-wave approach for supercritical flows with
hydraulic jumps.

Individual Contributions
------------------------

- **Yannik Köllmann:**
- **Jan Vogt:** Implementation von 3.2.1 (``BoundaryCondition``,
  ``setGhost``, CLI-Flags, ``[WaveProp1dReflecting]``-Tests),
  Setup und Visualisierungen für 3.2.2, sowie diese Doku.
- **Mika Brückner:** Integration of bathymetry support into the project (3.1).
Implementation of subcritical and subcritical flow setups (3.3) and related visualizations.
Computation of maximum Froude numbers and analysis of hydraulic jump convergence issues (3.3.1 / 3.3.3).



