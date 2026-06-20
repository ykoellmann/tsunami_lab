#ifndef TSUNAMI_LAB_VISUALIZATION_GLOBEVIEW_H
#define TSUNAMI_LAB_VISUALIZATION_GLOBEVIEW_H

#include "BBox.h"
#include "Camera.h"
#include "Shader.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <utility>

namespace tsunami_lab {
namespace visualization {

class GlobeView {
public:
  // Max side length of a selection box in degrees (adjustable via ImGui)
  static constexpr float DEFAULT_MAX_SEL_DEG = 20.0f;

  // Load GEBCO elevation data from i_gebcoPath and initialise GPU resources.
  // On failure the view still renders, but the map is blank (grey ocean).
  void init(const char* i_gebcoPath);

  // Draw the terrain mesh and, if a selection is in progress or finalised,
  // the highlighted selection rectangle.  Call after clearing the framebuffer.
  void draw(const glm::mat4& i_vp) const;

  // Mouse event handlers (screen coordinates, 0/0 at top-left).
  void
  onMousePress(float i_mx, float i_my, int i_w, int i_h, const Camera& i_cam);
  void onMouseRelease();
  void
  onMouseMove(float i_mx, float i_my, int i_w, int i_h, const Camera& i_cam);

  bool hasSelection() const { return m_hasSelection; }
  BBox getSelection() const;
  void clearSelection() {
    m_hasSelection = false;
    m_selecting = false;
  }

  // Maximum allowed selection side in degrees (configurable at runtime).
  float maxSelDeg = DEFAULT_MAX_SEL_DEG;

  // Geocode a city name via Nominatim (blocking, uses system curl).
  // Returns (lon, lat) in degrees, or (0,0) on failure.
  static std::pair<float, float> geocodeCity(const std::string& i_city);

  ~GlobeView();

private:
  // Convert screen pixel (i_mx, i_my) to world (lon, lat) via ray–plane
  // intersection with y=0.
  glm::vec2 unproject(
      float i_mx, float i_my, int i_w, int i_h, const Camera& i_cam) const;

  // Clamp a candidate selection end point so the box stays within max size
  // and within valid lon/lat ranges.
  glm::vec2 clampSelEnd(glm::vec2 i_end) const;

  // Upload the four corners of the current selection into m_selVbo.
  void uploadSelectionRect() const;

  void loadGebco(const char* i_path);
  void buildTerrainMesh(const float* i_elev, int i_w, int i_h);
  void initSelectionVao();

  Shader m_terrShader;
  Shader m_selShader;

  // Terrain mesh
  GLuint m_terrVao = 0;
  GLuint m_terrVbPos = 0; // (lon, lat) pairs
  GLuint m_terrVbElv = 0; // float elevation
  GLuint m_terrEbo = 0;
  GLsizei m_terrIdxCnt = 0;

  // Selection rectangle: 4 corner vertices (SW, SE, NE, NW) in (lon, lat)
  GLuint m_selVao = 0;
  GLuint m_selVbo = 0;

  bool m_selecting = false;
  bool m_hasSelection = false;
  glm::vec2 m_selA{0, 0}; // anchor (lon, lat) set on mouse press
  glm::vec2 m_selB{0, 0}; // current drag end (clamped)
};

} // namespace visualization
} // namespace tsunami_lab

#endif
