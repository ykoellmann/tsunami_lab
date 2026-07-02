#include "GlobeView.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <netcdf.h>
#include <string>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace tsunami_lab {
namespace visualization {

void GlobeView::init(const char* i_gebcoPath) {
  m_terrShader.buildFromFiles(SHADER_DIR "/globe_terrain.vert",
                              SHADER_DIR "/globe_terrain.frag");
  m_selShader.buildFromFiles(SHADER_DIR "/globe_selection.vert",
                             SHADER_DIR "/globe_selection.frag");
  initSelectionVao();
  if (i_gebcoPath) {
    m_gebcoPath = i_gebcoPath;
    loadGebco(m_gebcoPath.c_str(), m_lonSamples);
  }
}

void GlobeView::setResolution(int i_lonSamples) {
  i_lonSamples =
      std::max(MIN_LON_SAMPLES, std::min(MAX_LON_SAMPLES, i_lonSamples));
  if (i_lonSamples == m_lonSamples || m_gebcoPath.empty())
    return;
  m_lonSamples = i_lonSamples;
  loadGebco(m_gebcoPath.c_str(), m_lonSamples);
}

GlobeView::~GlobeView() {
  if (m_terrVao) {
    glDeleteVertexArrays(1, &m_terrVao);
    glDeleteBuffers(1, &m_terrVbPos);
    glDeleteBuffers(1, &m_terrVbElv);
    glDeleteBuffers(1, &m_terrEbo);
  }
  if (m_selVao) {
    glDeleteVertexArrays(1, &m_selVao);
    glDeleteBuffers(1, &m_selVbo);
  }
}

void GlobeView::loadGebco(const char* i_path, int i_lonSamples) {
  // Globally subsample GEBCO so the longitude axis holds ~i_lonSamples points;
  // the latitude axis uses the same stride (half as many points over 180°).
  // GEBCO 2025: lat from +90→-90, lon from -180→+180, elevation(lat, lon).
  int ncid = -1;
  if (nc_open(i_path, NC_NOWRITE, &ncid) != NC_NOERR) {
    std::fprintf(stderr, "GlobeView: cannot open %s\n", i_path);
    return;
  }

  int latDim = -1, lonDim = -1, varid = -1;
  size_t nLat = 0, nLon = 0;
  if (nc_inq_dimid(ncid, "lat", &latDim) != NC_NOERR ||
      nc_inq_dimid(ncid, "lon", &lonDim) != NC_NOERR ||
      nc_inq_dimlen(ncid, latDim, &nLat) != NC_NOERR ||
      nc_inq_dimlen(ncid, lonDim, &nLon) != NC_NOERR ||
      nc_inq_varid(ncid, "elevation", &varid) != NC_NOERR || nLat < 2 ||
      nLon < 2) {
    std::fprintf(stderr, "GlobeView: GEBCO dimensions/'elevation' not found\n");
    nc_close(ncid);
    return;
  }

  const long stride =
      std::max<long>(1, ((long)nLon + i_lonSamples - 1) / i_lonSamples);
  const int kW = (int)(((long)nLon - 1) / stride + 1);
  const int kH = (int)(((long)nLat - 1) / stride + 1);

  std::vector<short> raw((size_t)kW * kH);
  size_t start[2] = {0, 0};
  size_t count[2] = {(size_t)kH, (size_t)kW};
  ptrdiff_t strd[2] = {stride, stride};

  if (nc_get_vars_short(ncid, varid, start, count, strd, raw.data()) !=
      NC_NOERR) {
    std::fprintf(stderr, "GlobeView: failed to read elevation data\n");
    nc_close(ncid);
    return;
  }
  nc_close(ncid);

  std::vector<float> elev((size_t)kW * kH);
  for (int k = 0; k < kW * kH; k++)
    elev[k] = (float)raw[k];

  std::fprintf(stderr, "[GEBCO] Welt-Gitter: %d×%d (Stride %ld).\n", kW, kH,
               stride);
  buildTerrainMesh(elev.data(), kW, kH);
}

void GlobeView::buildTerrainMesh(const float* i_elev, int i_w, int i_h) {
  const int nVerts = i_w * i_h;
  std::vector<float> pos((size_t)nVerts * 2);
  for (int j = 0; j < i_h; j++) {
    float lat = -90.0f + j * (180.0f / (i_h - 1));
    for (int i = 0; i < i_w; i++) {
      float lon = -180.0f + i * (360.0f / (i_w - 1));
      pos[(j * i_w + i) * 2 + 0] = lon;
      pos[(j * i_w + i) * 2 + 1] = lat;
    }
  }

  const int nQuadsW = i_w - 1;
  const int nQuadsH = i_h - 1;
  std::vector<unsigned int> idx((size_t)nQuadsW * nQuadsH * 6);
  size_t cursor = 0;
  for (int j = 0; j < nQuadsH; j++) {
    for (int i = 0; i < nQuadsW; i++) {
      unsigned int v00 = (unsigned int)(j * i_w + i);
      unsigned int v01 = v00 + 1;
      unsigned int v10 = v00 + (unsigned int)i_w;
      unsigned int v11 = v10 + 1;
      idx[cursor++] = v00;
      idx[cursor++] = v10;
      idx[cursor++] = v01;
      idx[cursor++] = v10;
      idx[cursor++] = v11;
      idx[cursor++] = v01;
    }
  }
  m_terrIdxCnt = (GLsizei)idx.size();

  // Free any previously built mesh so setResolution() can rebuild without leak.
  if (m_terrVao) {
    glDeleteVertexArrays(1, &m_terrVao);
    glDeleteBuffers(1, &m_terrVbPos);
    glDeleteBuffers(1, &m_terrVbElv);
    glDeleteBuffers(1, &m_terrEbo);
    m_terrVao = m_terrVbPos = m_terrVbElv = m_terrEbo = 0;
  }

  glGenVertexArrays(1, &m_terrVao);
  glGenBuffers(1, &m_terrVbPos);
  glGenBuffers(1, &m_terrVbElv);
  glGenBuffers(1, &m_terrEbo);

  glBindVertexArray(m_terrVao);

  glBindBuffer(GL_ARRAY_BUFFER, m_terrVbPos);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(pos.size() * sizeof(float)),
               pos.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, m_terrVbElv);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(nVerts * sizeof(float)), i_elev,
               GL_STATIC_DRAW);
  glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(1);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_terrEbo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               (GLsizeiptr)(idx.size() * sizeof(unsigned int)), idx.data(),
               GL_STATIC_DRAW);

  glBindVertexArray(0);
}

void GlobeView::initSelectionVao() {
  glGenVertexArrays(1, &m_selVao);
  glGenBuffers(1, &m_selVbo);

  glBindVertexArray(m_selVao);
  glBindBuffer(GL_ARRAY_BUFFER, m_selVbo);
  // 4 vertices × 2 floats, dynamic (updated every frame during selection)
  glBufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof(float), nullptr,
               GL_DYNAMIC_DRAW);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glEnableVertexAttribArray(0);
  glBindVertexArray(0);
}

void GlobeView::draw(const glm::mat4& i_vp) const {
  if (m_terrVao) {
    m_terrShader.use();
    m_terrShader.setMat4("uVP", i_vp);
    glBindVertexArray(m_terrVao);
    glDrawElements(GL_TRIANGLES, m_terrIdxCnt, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
  }

  if ((m_selecting || m_hasSelection) && m_selVao) {
    uploadSelectionRect();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    m_selShader.use();
    m_selShader.setMat4("uVP", i_vp);

    glBindVertexArray(m_selVao);

    m_selShader.setVec4("uColor", glm::vec4(1.0f, 0.85f, 0.1f, 0.22f));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glLineWidth(2.0f);
    m_selShader.setVec4("uColor", glm::vec4(1.0f, 0.9f, 0.0f, 1.0f));
    // Vertices: SW=0, SE=1, NW=2, NE=3; outline order: SW→SE→NE→NW→SW
    const unsigned int lineIdx[5] = {0, 1, 3, 2, 0};
    glDrawElements(GL_LINE_STRIP, 5, GL_UNSIGNED_INT, lineIdx);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
  }
}

void GlobeView::uploadSelectionRect() const {
  float lonMin = std::min(m_selA.x, m_selB.x);
  float lonMax = std::max(m_selA.x, m_selB.x);
  float latMin = std::min(m_selA.y, m_selB.y);
  float latMax = std::max(m_selA.y, m_selB.y);

  // Corners stored for GL_TRIANGLE_STRIP: SW, SE, NW, NE
  float verts[8] = {
      lonMin, latMin, // SW
      lonMax, latMin, // SE
      lonMin, latMax, // NW
      lonMax, latMax, // NE
  };
  glBindBuffer(GL_ARRAY_BUFFER, m_selVbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
}

glm::vec2 GlobeView::unproject(
    float i_mx, float i_my, int i_w, int i_h, const Camera& i_cam) const {
  float aspect = (i_h > 0) ? (float)i_w / (float)i_h : 1.0f;
  float ndcX = (2.0f * i_mx / (float)i_w) - 1.0f;
  float ndcY = 1.0f - (2.0f * i_my / (float)i_h);

  glm::mat4 invVP = glm::inverse(i_cam.projection(aspect) * i_cam.view());

  glm::vec4 nearP = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
  glm::vec4 farP = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
  nearP /= nearP.w;
  farP /= farP.w;

  glm::vec3 dir = glm::normalize(glm::vec3(farP) - glm::vec3(nearP));
  glm::vec3 orig = glm::vec3(nearP);

  if (std::abs(dir.y) < 1e-6f)
    return {0.0f, 0.0f};

  float t = -orig.y / dir.y;
  glm::vec3 world = orig + t * dir;
  // Z = -lat in world space (see vertex shader)
  return {world.x, -world.z};
}

glm::vec2 GlobeView::clampSelEnd(glm::vec2 i_end) const {
  auto clampAxis = [&](float anchor, float end, float maxHalf) {
    float diff = end - anchor;
    if (diff > maxHalf)
      diff = maxHalf;
    if (diff < -maxHalf)
      diff = -maxHalf;
    return anchor + diff;
  };

  float lon = clampAxis(m_selA.x, i_end.x, maxSelDeg);
  float lat = clampAxis(m_selA.y, i_end.y, maxSelDeg);

  lon = std::min(180.0f, std::max(-180.0f, lon));
  lat = std::min(90.0f, std::max(-90.0f, lat));
  return {lon, lat};
}

void GlobeView::onMousePress(
    float i_mx, float i_my, int i_w, int i_h, const Camera& i_cam) {
  glm::vec2 world = unproject(i_mx, i_my, i_w, i_h, i_cam);
  m_selA = world;
  m_selB = world;
  m_selecting = true;
  m_hasSelection = false;
}

void GlobeView::onMouseRelease() {
  if (m_selecting) {
    m_selecting = false;
    BBox sel = getSelection();
    m_hasSelection =
        sel.valid() && sel.lonSpan() > 0.1f && sel.latSpan() > 0.1f;
  }
}

void GlobeView::onMouseMove(
    float i_mx, float i_my, int i_w, int i_h, const Camera& i_cam) {
  if (!m_selecting)
    return;
  glm::vec2 world = unproject(i_mx, i_my, i_w, i_h, i_cam);
  m_selB = clampSelEnd(world);
}

BBox GlobeView::getSelection() const {
  BBox b;
  b.lonMin = std::min(m_selA.x, m_selB.x);
  b.lonMax = std::max(m_selA.x, m_selB.x);
  b.latMin = std::min(m_selA.y, m_selB.y);
  b.latMax = std::max(m_selA.y, m_selB.y);
  return b;
}

void GlobeView::setSelection(const BBox& i_bbox) {
  m_selA = {i_bbox.lonMin, i_bbox.latMin};
  m_selB = {i_bbox.lonMax, i_bbox.latMax};
  m_hasSelection = true;
  m_selecting = false;
  uploadSelectionRect();
}

std::pair<float, float> GlobeView::geocodeCity(const std::string& i_city) {
  // Basic URL encode: replace spaces with +
  std::string query = i_city;
  for (char& c : query)
    if (c == ' ')
      c = '+';

  std::string cmd = "curl -s --max-time 5 "
                    "'https://nominatim.openstreetmap.org/search?q=" +
                    query + "&format=json&limit=1' 2>/dev/null";

  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe)
    return {0.0f, 0.0f};

  char buf[4096] = {};
  size_t l_nRead = std::fread(buf, 1, sizeof(buf) - 1, pipe);
  (void)l_nRead;
  pclose(pipe);

  auto extractField = [&](const char* key) -> float {
    std::string search = std::string("\"") + key + "\":\"";
    const char* p = std::strstr(buf, search.c_str());
    if (!p)
      return 0.0f;
    p += search.size();
    try {
      return std::stof(p);
    } catch (...) {
      return 0.0f;
    }
  };

  float lat = extractField("lat");
  float lon = extractField("lon");
  return {lon, lat};
}

} // namespace visualization
} // namespace tsunami_lab
