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

Individual Contributions
------------------------

- **Yannik Köllmann:**
- **Jan Vogt:** Implementation von 3.2.1 (``BoundaryCondition``,
  ``setGhost``, CLI-Flags, ``[WaveProp1dReflecting]``-Tests),
  Setup und Visualisierungen für 3.2.2, sowie diese Doku.
- **Mika Brückner:**
