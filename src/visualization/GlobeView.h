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
  static constexpr float DEFAULT_MAX_SEL_DEG = 20.0f;

  // Full GEBCO 2025 is ~3.7 billion points, so native resolution is not an
  // option.
  static constexpr int DEFAULT_LON_SAMPLES = 8640; // ~0.042° (2.5 arc-min)
  static constexpr int MIN_LON_SAMPLES = 360;      // 1.0°
  static constexpr int MAX_LON_SAMPLES = 17280;    // ~0.021° (1.25 arc-min)

  void init(const char* i_gebcoPath);
  void setResolution(int i_lonSamples);
  int resolution() const { return m_lonSamples; }
  void draw(const glm::mat4& i_vp) const;

  void
  onMousePress(float i_mx, float i_my, int i_w, int i_h, const Camera& i_cam);
  void onMouseRelease();
  void
  onMouseMove(float i_mx, float i_my, int i_w, int i_h, const Camera& i_cam);

  bool hasSelection() const { return m_hasSelection; }
  BBox getSelection() const;
  void setSelection(const BBox& i_bbox);
  void clearSelection() {
    m_hasSelection = false;
    m_selecting = false;
  }

  float maxSelDeg = DEFAULT_MAX_SEL_DEG;

  // Returns (lon, lat) in degrees, or (0,0) on failure.
  static std::pair<float, float> geocodeCity(const std::string& i_city);

  ~GlobeView();

private:
  glm::vec2 unproject(
      float i_mx, float i_my, int i_w, int i_h, const Camera& i_cam) const;
  glm::vec2 clampSelEnd(glm::vec2 i_end) const;
  void uploadSelectionRect() const;
  void loadGebco(const char* i_path, int i_lonSamples);
  void buildTerrainMesh(const float* i_elev, int i_w, int i_h);
  void initSelectionVao();

  Shader m_terrShader;
  Shader m_selShader;

  GLuint m_terrVao = 0;
  GLuint m_terrVbPos = 0;
  GLuint m_terrVbElv = 0;
  GLuint m_terrEbo = 0;
  GLsizei m_terrIdxCnt = 0;

  GLuint m_selVao = 0;
  GLuint m_selVbo = 0;

  std::string m_gebcoPath;
  int m_lonSamples = DEFAULT_LON_SAMPLES;

  bool m_selecting = false;
  bool m_hasSelection = false;
  glm::vec2 m_selA{0, 0}; // anchor (lon, lat) set on mouse press
  glm::vec2 m_selB{0, 0}; // current drag end (clamped)
};

} // namespace visualization
} // namespace tsunami_lab

#endif
