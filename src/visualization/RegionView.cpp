#include "RegionView.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace tsunami_lab {
namespace visualization {

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------

// Terrain: y is reconstructed from elevation × (scaleY × exaggeration), so the
// exaggeration slider is live. The fragment stage derives a per-face normal
// from screen-space derivatives of the world position for cheap hill shading.
static const char* k_terrVert = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aXZ;    // (x, z) world position
layout(location = 1) in float aElev; // metres
layout(location = 2) in float aDisp; // vertical seafloor displacement, metres

uniform mat4 uVP;
uniform float uScaleY; // metres → world units (already incl. exaggeration)

out float vElev;
out float vDisp;
out vec3 vWorld;

void main() {
    // Displacement uses the same vertical scale as the relief.
    vec3 world = vec3(aXZ.x, (aElev + aDisp) * uScaleY, aXZ.y);
    vWorld = world;
    vElev = aElev;
    vDisp = aDisp;
    gl_Position = uVP * vec4(world, 1.0);
}
)GLSL";

static const char* k_terrFrag = R"GLSL(
#version 330 core
in float vElev;
in float vDisp;
in vec3 vWorld;
out vec4 fragColor;

vec3 colormap(float h) {
    if (h < 0.0) {
        float t = clamp(h / -6000.0, 0.0, 1.0);
        return mix(vec3(0.05, 0.18, 0.45), vec3(0.22, 0.55, 0.80), 1.0 - t);
    } else {
        float t = clamp(h / 3000.0, 0.0, 1.0);
        return mix(vec3(0.30, 0.58, 0.22), vec3(0.62, 0.45, 0.25), t);
    }
}

void main() {
    vec3 n = normalize(cross(dFdx(vWorld), dFdy(vWorld)));
    if (n.y < 0.0) n = -n;
    vec3 lightDir = normalize(vec3(0.35, 1.0, 0.25));
    float diff = max(dot(n, lightDir), 0.0) * 0.65 + 0.35;
    vec3 base = colormap(vElev) * diff;

    // Tint the deformed seafloor so the source is visible: warm for uplift,
    // cool for subsidence, fading out where the displacement is negligible.
    float m = clamp(abs(vDisp) / 5.0, 0.0, 1.0);
    vec3 tint = (vDisp >= 0.0) ? vec3(0.95, 0.35, 0.15)
                               : vec3(0.55, 0.20, 0.85);
    fragColor = vec4(mix(base, tint, 0.55 * m), 1.0);
}
)GLSL";

static const char* k_seaVert = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aXZ;
uniform mat4 uVP;
void main() {
    gl_Position = uVP * vec4(aXZ.x, 0.0, aXZ.y, 1.0);
}
)GLSL";

static const char* k_seaFrag = R"GLSL(
#version 330 core
out vec4 fragColor;
void main() {
    fragColor = vec4(0.15, 0.45, 0.75, 0.35);
}
)GLSL";

// ---------------------------------------------------------------------------

void RegionView::init() {
  m_terrShader.build(k_terrVert, k_terrFrag);
  m_seaShader.build(k_seaVert, k_seaFrag);

  // Sea-level plane: covers a bit more than the normalised terrain extent.
  const float k_s = 130.0f;
  const float l_quad[8] = {-k_s, -k_s, k_s, -k_s, -k_s, k_s, k_s, k_s};
  glGenVertexArrays(1, &m_seaVao);
  glGenBuffers(1, &m_seaVbo);
  glBindVertexArray(m_seaVao);
  glBindBuffer(GL_ARRAY_BUFFER, m_seaVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(l_quad), l_quad, GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);
  glBindVertexArray(0);
}

bool RegionView::load(const gebco::Region& i_region) {
  const int l_w = i_region.w;
  const int l_h = i_region.h;
  if (l_w < 2 || l_h < 2)
    return false;

  lonMin = i_region.lonMin;
  lonMax = i_region.lonMax;
  latMin = i_region.latMin;
  latMax = i_region.latMax;
  gridW = l_w;
  gridH = l_h;

  // Metres per degree at the region's centre latitude.
  const double l_latC = 0.5 * (i_region.latMin + i_region.latMax);
  const double l_lonC = 0.5 * (i_region.lonMin + i_region.lonMax);
  const double l_mPerLat = 111132.0;
  const double l_mPerLon = 111320.0 * std::cos(glm::radians(l_latC));

  const double l_widthM = (i_region.lonMax - i_region.lonMin) * l_mPerLon;
  const double l_heightM = (i_region.latMax - i_region.latMin) * l_mPerLat;
  const double l_maxM = std::max(1.0, std::max(l_widthM, l_heightM));

  // Normalise so the larger horizontal side spans 200 world units.
  const double l_scaleXZ = 200.0 / l_maxM;
  m_scaleY = (float)l_scaleXZ; // exaggeration multiplied in at draw time
  m_scaleXZ = (float)l_scaleXZ;

  std::vector<float> l_xz((size_t)l_w * l_h * 2);
  for (int j = 0; j < l_h; j++) {
    double l_lat = i_region.latMin + (double)j *
                                         (i_region.latMax - i_region.latMin) /
                                         (double)(l_h - 1);
    for (int i = 0; i < l_w; i++) {
      double l_lon = i_region.lonMin + (double)i *
                                           (i_region.lonMax - i_region.lonMin) /
                                           (double)(l_w - 1);
      double l_x = (l_lon - l_lonC) * l_mPerLon * l_scaleXZ;
      // North (+lat) maps to -Z so north is "away" from a default camera.
      double l_z = -(l_lat - l_latC) * l_mPerLat * l_scaleXZ;
      l_xz[((size_t)j * l_w + i) * 2 + 0] = (float)l_x;
      l_xz[((size_t)j * l_w + i) * 2 + 1] = (float)l_z;
    }
  }

  // Two triangles per quad.
  std::vector<unsigned int> l_idx((size_t)(l_w - 1) * (l_h - 1) * 6);
  size_t l_c = 0;
  for (int j = 0; j < l_h - 1; j++) {
    for (int i = 0; i < l_w - 1; i++) {
      unsigned int l_v00 = (unsigned int)(j * l_w + i);
      unsigned int l_v01 = l_v00 + 1;
      unsigned int l_v10 = l_v00 + (unsigned int)l_w;
      unsigned int l_v11 = l_v10 + 1;
      l_idx[l_c++] = l_v00;
      l_idx[l_c++] = l_v10;
      l_idx[l_c++] = l_v01;
      l_idx[l_c++] = l_v10;
      l_idx[l_c++] = l_v11;
      l_idx[l_c++] = l_v01;
    }
  }
  m_idxCnt = (GLsizei)l_idx.size();

  // Keep the horizontal positions for displacement (re)evaluation; a fresh
  // region starts with no displacement applied.
  m_xz = l_xz;
  m_hasDispl = false;
  std::vector<float> l_disp((size_t)l_w * l_h, 0.0f);

  if (m_vao == 0) {
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbXZ);
    glGenBuffers(1, &m_vbElv);
    glGenBuffers(1, &m_vbDisp);
    glGenBuffers(1, &m_ebo);
  }

  glBindVertexArray(m_vao);

  glBindBuffer(GL_ARRAY_BUFFER, m_vbXZ);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(l_xz.size() * sizeof(float)),
               l_xz.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, m_vbElv);
  glBufferData(GL_ARRAY_BUFFER,
               (GLsizeiptr)(i_region.elev.size() * sizeof(float)),
               i_region.elev.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(1);

  // Per-vertex displacement, initialised to zero.
  glBindBuffer(GL_ARRAY_BUFFER, m_vbDisp);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(l_disp.size() * sizeof(float)),
               l_disp.data(), GL_DYNAMIC_DRAW);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               (GLsizeiptr)(l_idx.size() * sizeof(unsigned int)), l_idx.data(),
               GL_STATIC_DRAW);

  glBindVertexArray(0);
  return true;
}

void RegionView::worldToLonLat(float i_worldX,
                               float i_worldZ,
                               double& o_lon,
                               double& o_lat) const {
  const double l_latC = 0.5 * (latMin + latMax);
  const double l_lonC = 0.5 * (lonMin + lonMax);
  const double l_mPerLat = 111132.0;
  const double l_mPerLon = 111320.0 * std::cos(glm::radians(l_latC));
  // Inverse of the lon/lat → world mapping in load() (note world Z = -lat).
  o_lon = l_lonC + ((double)i_worldX / m_scaleXZ) / l_mPerLon;
  o_lat = l_latC - ((double)i_worldZ / m_scaleXZ) / l_mPerLat;
}

void RegionView::applyDisplacement(
    float i_worldX,
    float i_worldZ,
    const displacement::DisplacementModel& i_model) {
  if (m_vao == 0 || m_xz.empty())
    return;

  const size_t l_n = m_xz.size() / 2;
  std::vector<float> l_disp(l_n);
  for (size_t l_v = 0; l_v < l_n; l_v++) {
    // Local east/north offset from the click point, in metres. World Z runs
    // south→north as -lat, so +north = -Δz.
    double l_east = ((double)m_xz[l_v * 2 + 0] - i_worldX) / m_scaleXZ;
    double l_north = -((double)m_xz[l_v * 2 + 1] - i_worldZ) / m_scaleXZ;
    l_disp[l_v] = (float)i_model.verticalDisplacement(l_east, l_north);
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_vbDisp);
  glBufferSubData(GL_ARRAY_BUFFER, 0,
                  (GLsizeiptr)(l_disp.size() * sizeof(float)), l_disp.data());
  m_hasDispl = true;
}

void RegionView::clearDisplacement() {
  if (m_vao == 0 || m_xz.empty())
    return;
  std::vector<float> l_zero(m_xz.size() / 2, 0.0f);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbDisp);
  glBufferSubData(GL_ARRAY_BUFFER, 0,
                  (GLsizeiptr)(l_zero.size() * sizeof(float)), l_zero.data());
  m_hasDispl = false;
}

void RegionView::draw(const glm::mat4& i_vp) const {
  if (m_idxCnt == 0)
    return;

  m_terrShader.use();
  m_terrShader.setMat4("uVP", i_vp);
  m_terrShader.setFloat("uScaleY", m_scaleY * vertExaggeration);
  glBindVertexArray(m_vao);
  glDrawElements(GL_TRIANGLES, m_idxCnt, GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);

  if (showSea && m_seaVao) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    m_seaShader.use();
    m_seaShader.setMat4("uVP", i_vp);
    glBindVertexArray(m_seaVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  }
}

RegionView::~RegionView() {
  if (m_vao) {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbXZ);
    glDeleteBuffers(1, &m_vbElv);
    glDeleteBuffers(1, &m_vbDisp);
    glDeleteBuffers(1, &m_ebo);
  }
  if (m_seaVao) {
    glDeleteVertexArrays(1, &m_seaVao);
    glDeleteBuffers(1, &m_seaVbo);
  }
}

} // namespace visualization
} // namespace tsunami_lab
