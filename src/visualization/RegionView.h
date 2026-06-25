#ifndef TSUNAMI_LAB_VISUALIZATION_REGIONVIEW_H
#define TSUNAMI_LAB_VISUALIZATION_REGIONVIEW_H

#include "../constants.h"
#include "Gebco.h"
#include "Shader.h"
#include "displacement/DisplacementModel.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

namespace tsunami_lab {
namespace visualization {

// Renders a single GEBCO region extract as a shaded 3D bathymetry surface.
// Horizontal extent is normalised so the larger side spans ~200 world units
// (keeps it inside the shared camera's near/far planes regardless of how many
// degrees were selected); vertical exaggeration is applied live in the shader.
class RegionView {
public:
  // Which field the relief surface visualises.
  enum class Field { Bathymetry, Displacement };

  // Build GPU programs and the static sea-level plane. Needs a current GL
  // context.
  void init();

  // Upload i_region as the terrain mesh. Returns false if the region is empty.
  bool load(const gebco::Region& i_region);

  // Draw terrain (and, if enabled, the translucent sea-level plane).
  void draw(const glm::mat4& i_vp) const;

  bool loaded() const { return m_idxCnt > 0; }

  // Add the displacement of i_model to every terrain vertex, with the model's
  // local east/north frame centred on world point (i_worldX, i_worldZ).
  // Replaces any previously applied displacement.
  void applyDisplacement(float i_worldX,
                         float i_worldZ,
                         const displacement::DisplacementModel& i_model);

  void clearDisplacement();

  bool hasDisplacement() const { return m_hasDispl; }

  // ── Live simulation overlay ───────────────────────────────────────────────
  // Build a water surface at the simulation resolution (i_nx × i_ny) spanning
  // the current region footprint. i_bath holds the sim-grid bathymetry in
  // metres (row-major, contiguous), used to reconstruct the surface elevation
  // (bathymetry + water column) per vertex. Call after load().
  void beginSimulation(t_idx i_nx, t_idx i_ny, const float* i_bath);

  // Upload a new water-height frame (i_h: nx*ny contiguous water columns in
  // metres). No-op unless beginSimulation() ran.
  void updateWater(const float* i_h);

  // Stop drawing the water surface (returns to the static bathymetry view).
  void endSimulation() { m_simulating = false; }

  bool simulating() const { return m_simulating; }

  // Convert a world (x, z) point to geographic (lon, lat) degrees.
  void worldToLonLat(float i_worldX,
                     float i_worldZ,
                     double& o_lon,
                     double& o_lat) const;

  // Vertical exaggeration, applied per-frame as a uniform (no mesh rebuild).
  float vertExaggeration = 25.0f;
  // Draw a translucent plane at y = 0 (sea level) for coastline reference.
  bool showSea = true;
  // Selects whether draw() shows the bathymetry or the displacement field.
  Field field = Field::Bathymetry;

  // Geographic bounds of the currently loaded region (for the UI).
  double lonMin = 0, lonMax = 0, latMin = 0, latMax = 0;
  int gridW = 0, gridH = 0;

  ~RegionView();

private:
  Shader m_terrShader;
  Shader m_seaShader;

  GLuint m_vao = 0;
  GLuint m_vbXZ = 0;   // vec2 (x, z) world position (horizontal)
  GLuint m_vbElv = 0;  // float elevation in metres
  GLuint m_vbDisp = 0; // float vertical displacement in metres (per vertex)
  GLuint m_ebo = 0;
  GLsizei m_idxCnt = 0;

  // Horizontal vertex positions (world x, z) and metres to world scale, kept
  // for (re)evaluating a displacement model.
  std::vector<float> m_xz;
  float m_scaleXZ = 1.0f;
  bool m_hasDispl = false;
  // Peak |displacement| in metres and the scale mapping it to a fixed on-screen
  // height, keeping the field visible regardless of amplitude.
  float m_dispPeak = 0.0f;
  float m_dispScaleY = 0.0f;

  GLuint m_seaVao = 0;
  GLuint m_seaVbo = 0;

  // True vertical scale (metres → world units) before exaggeration.
  float m_scaleY = 1.0f;

  // ── Live water surface (simulation overlay) ───────────────────────────────
  Shader m_waterShader;
  GLuint m_waterVao = 0;
  GLuint m_waterVbXZ = 0;   // vec2 (x, z) world position
  GLuint m_waterVbBath = 0; // float bathymetry (metres) per vertex
  GLuint m_waterVbH = 0;    // float water column (metres) per vertex, dynamic
  GLuint m_waterEbo = 0;
  GLsizei m_waterIdxCnt = 0;
  t_idx m_simNx = 0;
  t_idx m_simNy = 0;
  bool m_simulating = false;
  // Peak |surface anomaly| of the latest frame, for colour normalisation.
  float m_waterAnom = 1.0f;
  std::vector<float> m_waterH;    // scratch for the per-vertex height upload
  std::vector<float> m_simBath;   // sim-grid bathymetry (CPU copy, for anomaly)
};

} // namespace visualization
} // namespace tsunami_lab

#endif
