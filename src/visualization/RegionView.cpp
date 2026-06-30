#include "RegionView.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace tsunami_lab {
namespace visualization {

void RegionView::init() {
  m_terrShader.buildFromFiles(SHADER_DIR "/region_terrain.vert",
                              SHADER_DIR "/region_terrain.frag");
  m_seaShader.buildFromFiles(SHADER_DIR "/region_sea.vert",
                             SHADER_DIR "/region_sea.frag");
  m_waterShader.buildFromFiles(SHADER_DIR "/region_water.vert",
                               SHADER_DIR "/region_water.frag");

  const float k_s = 100.0f;
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
  m_scaleY = (float)l_scaleXZ;
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

  glBindBuffer(GL_ARRAY_BUFFER, m_vbDisp);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(l_disp.size() * sizeof(float)),
               l_disp.data(), GL_DYNAMIC_DRAW);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               (GLsizeiptr)(l_idx.size() * sizeof(unsigned int)), l_idx.data(),
               GL_STATIC_DRAW);

  // Resize the sea-level plane to the exact horizontal footprint of this
  // region.
  const float l_seaHalfX = (float)(0.5 * l_widthM * l_scaleXZ);
  const float l_seaHalfZ = (float)(0.5 * l_heightM * l_scaleXZ);
  const float l_sea[8] = {-l_seaHalfX, -l_seaHalfZ, l_seaHalfX, -l_seaHalfZ,
                          -l_seaHalfX, l_seaHalfZ,  l_seaHalfX, l_seaHalfZ};
  glBindBuffer(GL_ARRAY_BUFFER, m_seaVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(l_sea), l_sea, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

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
  float l_peak = 0.0f;
  for (size_t l_v = 0; l_v < l_n; l_v++) {
    // Local east/north offset from the click point, in metres. World Z runs
    // south to north as -lat, so +north = -dz.
    double l_east = ((double)m_xz[l_v * 2 + 0] - i_worldX) / m_scaleXZ;
    double l_north = -((double)m_xz[l_v * 2 + 1] - i_worldZ) / m_scaleXZ;
    l_disp[l_v] = (float)i_model.verticalDisplacement(l_east, l_north);
    l_peak = std::max(l_peak, std::abs(l_disp[l_v]));
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_vbDisp);
  glBufferSubData(GL_ARRAY_BUFFER, 0,
                  (GLsizeiptr)(l_disp.size() * sizeof(float)), l_disp.data());

  // Map the peak to a fixed on-screen height so the field is always visible.
  constexpr float k_targetHeight = 30.0f;
  m_dispPeak = l_peak;
  m_dispScaleY = (l_peak > 0.0f) ? k_targetHeight / l_peak : 0.0f;
  m_hasDispl = true;
}

void RegionView::clearDisplacement() {
  if (m_vao == 0 || m_xz.empty())
    return;
  std::vector<float> l_zero(m_xz.size() / 2, 0.0f);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbDisp);
  glBufferSubData(GL_ARRAY_BUFFER, 0,
                  (GLsizeiptr)(l_zero.size() * sizeof(float)), l_zero.data());
  m_dispPeak = 0.0f;
  m_dispScaleY = 0.0f;
  m_hasDispl = false;
}

void RegionView::beginSimulation(t_idx i_nx, t_idx i_ny, const float* i_bath) {
  if (i_nx < 2 || i_ny < 2 || i_bath == nullptr)
    return;
  m_simNx = i_nx;
  m_simNy = i_ny;

  // Same lon/lat → world mapping as the terrain mesh, so the water aligns.
  const double l_latC = 0.5 * (latMin + latMax);
  const double l_lonC = 0.5 * (lonMin + lonMax);
  const double l_mPerLat = 111132.0;
  const double l_mPerLon = 111320.0 * std::cos(glm::radians(l_latC));

  const size_t l_n = (size_t)i_nx * i_ny;
  std::vector<float> l_xz(l_n * 2);
  m_simBath.assign(i_bath, i_bath + l_n);
  for (t_idx l_j = 0; l_j < i_ny; l_j++) {
    const double l_lat =
        latMin + (double)l_j * (latMax - latMin) / (double)(i_ny - 1);
    for (t_idx l_i = 0; l_i < i_nx; l_i++) {
      const double l_lon =
          lonMin + (double)l_i * (lonMax - lonMin) / (double)(i_nx - 1);
      const double l_x = (l_lon - l_lonC) * l_mPerLon * m_scaleXZ;
      const double l_z = -(l_lat - l_latC) * l_mPerLat * m_scaleXZ;
      const size_t l_k = (size_t)l_j * i_nx + l_i;
      l_xz[l_k * 2 + 0] = (float)l_x;
      l_xz[l_k * 2 + 1] = (float)l_z;
    }
  }

  std::vector<unsigned int> l_idx((size_t)(i_nx - 1) * (i_ny - 1) * 6);
  size_t l_c = 0;
  for (t_idx l_j = 0; l_j < i_ny - 1; l_j++) {
    for (t_idx l_i = 0; l_i < i_nx - 1; l_i++) {
      unsigned int l_v00 = (unsigned int)(l_j * i_nx + l_i);
      unsigned int l_v01 = l_v00 + 1;
      unsigned int l_v10 = l_v00 + (unsigned int)i_nx;
      unsigned int l_v11 = l_v10 + 1;
      l_idx[l_c++] = l_v00;
      l_idx[l_c++] = l_v10;
      l_idx[l_c++] = l_v01;
      l_idx[l_c++] = l_v10;
      l_idx[l_c++] = l_v11;
      l_idx[l_c++] = l_v01;
    }
  }
  m_waterIdxCnt = (GLsizei)l_idx.size();
  m_waterH.assign(l_n, 0.0f);

  if (m_waterVao == 0) {
    glGenVertexArrays(1, &m_waterVao);
    glGenBuffers(1, &m_waterVbXZ);
    glGenBuffers(1, &m_waterVbBath);
    glGenBuffers(1, &m_waterVbH);
    glGenBuffers(1, &m_waterEbo);
  }

  glBindVertexArray(m_waterVao);

  glBindBuffer(GL_ARRAY_BUFFER, m_waterVbXZ);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(l_xz.size() * sizeof(float)),
               l_xz.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, m_waterVbBath);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(m_simBath.size() * sizeof(float)),
               m_simBath.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ARRAY_BUFFER, m_waterVbH);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(m_waterH.size() * sizeof(float)),
               m_waterH.data(), GL_DYNAMIC_DRAW);
  glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_waterEbo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               (GLsizeiptr)(l_idx.size() * sizeof(unsigned int)), l_idx.data(),
               GL_STATIC_DRAW);

  glBindVertexArray(0);
  // Seed the colour scale from the earthquake displacement: the open-ocean
  // surface anomaly never much exceeds the initial uplift, so this gives a
  // stable reference from the very first frame instead of starting tiny and
  // rescaling upward.
  m_waterAnom = std::max(0.05f, m_dispPeak);
  m_simulating = true;
}

void RegionView::updateWater(const float* i_h) {
  if (!m_simulating || m_waterVao == 0 || i_h == nullptr)
    return;
  const size_t l_n = (size_t)m_simNx * m_simNy;

  float l_peak = 0.0f;
  for (size_t l_k = 0; l_k < l_n; l_k++) {
    m_waterH[l_k] = i_h[l_k];
    // Exclude shallow cells: shoaling produces extreme eta there and would
    // collapse the colour scale for the open-ocean signal.
    const float l_depth = -m_simBath[l_k];
    if (i_h[l_k] >= 0.01f && l_depth > 500.0f)
      l_peak = std::max(l_peak, std::abs(m_simBath[l_k] + i_h[l_k]));
  }
  // Latch the running maximum: the colour scale only ever grows, never
  // shrinks frame-to-frame. A scale that rescaled every frame made identical
  // wave heights flicker between colours as the global peak rose and fell.
  m_waterAnom = std::max(m_waterAnom, l_peak);

  glBindBuffer(GL_ARRAY_BUFFER, m_waterVbH);
  glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(l_n * sizeof(float)),
                  m_waterH.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RegionView::draw(const glm::mat4& i_vp) const {
  if (m_idxCnt == 0)
    return;

  m_terrShader.use();
  m_terrShader.setMat4("uVP", i_vp);
  m_terrShader.setFloat("uScaleY", m_scaleY * vertExaggeration);
  m_terrShader.setFloat("uDispScaleY",
                        m_dispScaleY * (vertExaggeration / 25.0f));
  m_terrShader.setFloat("uDispRange", m_dispPeak);
  m_terrShader.setInt("uMode", field == Field::Displacement ? 1 : 0);
  glBindVertexArray(m_vao);
  glDrawElements(GL_TRIANGLES, m_idxCnt, GL_UNSIGNED_INT, nullptr);
  glBindVertexArray(0);

  if (m_simulating && m_waterVao && field == Field::Bathymetry) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    m_waterShader.use();
    m_waterShader.setMat4("uVP", i_vp);
    m_waterShader.setFloat("uScaleY", m_scaleY * vertExaggeration);
    m_waterShader.setFloat("uWaveExagg", waveExaggeration);
    m_waterShader.setFloat("uAnom", m_waterAnom);
    glBindVertexArray(m_waterVao);
    glDrawElements(GL_TRIANGLES, m_waterIdxCnt, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
  } else if (showSea && m_seaVao && field == Field::Bathymetry) {
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
  if (m_waterVao) {
    glDeleteVertexArrays(1, &m_waterVao);
    glDeleteBuffers(1, &m_waterVbXZ);
    glDeleteBuffers(1, &m_waterVbBath);
    glDeleteBuffers(1, &m_waterVbH);
    glDeleteBuffers(1, &m_waterEbo);
  }
}

} // namespace visualization
} // namespace tsunami_lab
