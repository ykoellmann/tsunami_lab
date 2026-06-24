10. Individual Phase Week 1 — Real-Time OpenGL Visualization
======================================================

Overview
--------

The individual phase extends the project with an interactive 3D visualization
tool.  The user selects a geographic region on a globe, inspects its bathymetry
in a 3D preview, and will eventually watch the tsunami simulation unfold in real
time, directly in the visualizer without writing NetCDF files.

A separate binary ``tsunami_lab_viz`` is built alongside the existing pipeline;
all tests and CI remain unaffected.

Implementation
---------------

The visualizer is structured in three views:

**Globe view.**
A flat world map rendered from the GEBCO global bathymetry grid.  The user
draws a selection rectangle by clicking and dragging.  A city search field
geocodes place names and centres the camera accordingly.

.. figure:: ../_images/individual_phase/globe_view.png
   :alt: Globe view with a selection rectangle over Southeast Asia
   :width: 80%

   Globe view, region selection over the Sumatra/Java subduction zone.

**GEBCO data.**
On first launch the visualizer ensures the GEBCO_2026 global bathymetry grid
(15 arc-second ice-surface, ~7.5 GB NetCDF) is available locally, downloading
and unzipping it automatically if the file is missing.  When a region is
selected, only that window is read straight from the grid as a NetCDF hyperslab
at native resolution — the multi-gigabyte file is never loaded into memory in
full — with adaptive striding so very large selections stay within a manageable
vertex budget.

**Region preview.**
The extracted region is rendered as a shaded 3D terrain mesh.  Its horizontal
extent is normalised to a fixed world size so that any selection fits the
camera, while vertical exaggeration is applied live in the vertex shader without
rebuilding the mesh.  Hill shading is derived per-fragment from screen-space
derivatives of the surface; an elevation colormap and a translucent sea-level
plane (both toggleable in the sidebar) make the coastline and water depth easy
to read.

.. figure:: ../_images/individual_phase/region_preview.png
   :alt: 3D bathymetry terrain preview of the selected region
   :width: 80%

   Region preview, GEBCO bathymetry rendered as a shaded 3D mesh.

**Seafloor displacement.**
A ``GaussianDisplacement`` model computes a radially symmetric vertical
displacement with configurable amplitude and spread.  Left-clicking the terrain
places the source at that position; the displacement field is evaluated per
vertex and uploaded to a separate GPU buffer.  Uplifted areas are tinted
orange-red, subsiding areas purple-blue.  A sidebar toggle switches between the
combined bathymetry view and a displacement-only view with a diverging colormap.

.. figure:: ../_images/individual_phase/displacement.png
   :alt: Region preview with a Gaussian seafloor displacement
   :width: 80%

   Gaussian displacement, uplifted seafloor
   highlighted in orange-red.

**Build integration.**
The project migrated from SCons to CMake to integrate the required OpenGL
libraries (GLFW, GLAD, ImGui, GLM).  The ``ENABLE_VIZ`` flag keeps CI builds
headless and fast.

Individual Contributions
-------------------------

- **Yannik Köllmann:** CMake build system, OpenGL foundation (window, camera,
  shaders), globe view with mouse-driven region selection and city search,
  keyboard shortcuts.
- **Jan Vogt:** Automatic GEBCO_2026 download and native-resolution region
  reader (NetCDF hyperslab, adaptive striding); 3D bathymetry terrain renderer
  with live vertical exaggeration, screen-space hill shading and a sea-level
  plane
- **Mika Brückner:** Project plan and pitch presentation; ``GaussianDisplacement``
  model with unit tests; click-to-place displacement in the region preview;
  bathymetry/displacement view toggle with diverging colormap.
