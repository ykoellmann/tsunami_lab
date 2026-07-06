#ifndef TSUNAMI_LAB_VISUALIZATION_LOD_H
#define TSUNAMI_LAB_VISUALIZATION_LOD_H

#include <algorithm>
#include <cmath>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

namespace tsunami_lab {
namespace visualization {
namespace lod {

// Number of index-buffer levels built per mesh (vertex stride 1, 2, 4, …,
// 2^(k_maxLevels-1)). Coarser levels cost ~1/4 of the previous one, so the
// whole chain adds ~33 % to the stride-1 index memory.
const int k_maxLevels = 8;

// Builds a quad-grid index list over a w×h vertex grid, sampling every
// i_stride-th vertex, ordered row-major (all quads of a row are contiguous)
// so drawGridWindow() can render a rectangular sub-window as one contiguous
// index span per row. The last row/column is clamped into the final quads so
// every level spans the full mesh extent. Winding matches the stride-1 mesh.
inline void
buildIndices(int i_w, int i_h, int i_stride, std::vector<unsigned int>& o_idx) {
  o_idx.clear();
  for (int l_j = 0; l_j < i_h - 1; l_j += i_stride) {
    const int l_j2 = std::min(l_j + i_stride, i_h - 1);
    for (int l_i = 0; l_i < i_w - 1; l_i += i_stride) {
      const int l_i2 = std::min(l_i + i_stride, i_w - 1);
      const unsigned int l_v00 = (unsigned int)(l_j * i_w + l_i);
      const unsigned int l_v01 = (unsigned int)(l_j * i_w + l_i2);
      const unsigned int l_v10 = (unsigned int)(l_j2 * i_w + l_i);
      const unsigned int l_v11 = (unsigned int)(l_j2 * i_w + l_i2);
      o_idx.push_back(l_v00);
      o_idx.push_back(l_v10);
      o_idx.push_back(l_v01);
      o_idx.push_back(l_v10);
      o_idx.push_back(l_v11);
      o_idx.push_back(l_v01);
    }
  }
}

// Smallest level whose cells still project to >= ~1.5 px on screen. Rendering
// sub-pixel triangles adds no visible detail but multiplies the MSAA fill
// cost enough to stall the GPU when a dense mesh fills the view.
//
// i_cellWorld:   world-unit size of one stride-1 grid cell
// i_numLevels:   how many levels were actually built for this mesh
// i_camDistance: camera orbit distance, world units (45° vertical FOV assumed)
// i_viewportPx:  framebuffer height in pixels
inline int pickLevel(float i_cellWorld,
                     int i_numLevels,
                     float i_camDistance,
                     int i_viewportPx) {
  if (i_numLevels <= 1 || i_cellWorld <= 0.0f || i_viewportPx <= 0)
    return 0;
  // The viewport height spans 2 * distance * tan(fov/2) world units.
  const float l_px = i_cellWorld * (float)i_viewportPx /
                     (2.0f * std::max(1.0f, i_camDistance) * 0.41421356f);
  int l_level = 0;
  while (l_level + 1 < i_numLevels && l_px * (float)(1 << l_level) < 1.5f)
    l_level++;
  return l_level;
}

// World-space XZ bounding box of the view frustum's intersection with the
// horizontal slab y ∈ [i_minY, i_maxY] (the vertical extent of the mesh).
// Casts a ray through each viewport corner and clips it against the slab; a
// corner ray that misses the slab contributes its full near→far segment, so
// the result degrades to a conservative superset instead of dropping
// geometry. Used to cull off-screen grid rows/columns when zoomed in.
inline void frustumFootprintXZ(const glm::mat4& i_vp,
                               float i_minY,
                               float i_maxY,
                               float& o_minX,
                               float& o_maxX,
                               float& o_minZ,
                               float& o_maxZ) {
  const glm::mat4 l_inv = glm::inverse(i_vp);
  o_minX = o_minZ = 3.4e38f;
  o_maxX = o_maxZ = -3.4e38f;
  const float l_lo = std::min(i_minY, i_maxY) - 0.01f;
  const float l_hi = std::max(i_minY, i_maxY) + 0.01f;
  const float l_ndc[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
  for (int l_c = 0; l_c < 4; l_c++) {
    glm::vec4 l_n4 = l_inv * glm::vec4(l_ndc[l_c][0], l_ndc[l_c][1], -1, 1);
    glm::vec4 l_f4 = l_inv * glm::vec4(l_ndc[l_c][0], l_ndc[l_c][1], 1, 1);
    const glm::vec3 l_n = glm::vec3(l_n4) / l_n4.w;
    const glm::vec3 l_d = glm::vec3(l_f4) / l_f4.w - l_n;

    float l_t0 = 0.0f, l_t1 = 1.0f;
    if (std::abs(l_d.y) > 1e-9f) {
      const float l_ta = (l_lo - l_n.y) / l_d.y;
      const float l_tb = (l_hi - l_n.y) / l_d.y;
      l_t0 = std::max(0.0f, std::min(l_ta, l_tb));
      l_t1 = std::min(1.0f, std::max(l_ta, l_tb));
      if (l_t1 < l_t0) {
        l_t0 = 0.0f; // ray misses the slab: keep the full segment
        l_t1 = 1.0f; // (conservative, never culls visible geometry)
      }
    } else if (l_n.y < l_lo || l_n.y > l_hi) {
      l_t0 = 0.0f;
      l_t1 = 1.0f;
    }
    for (int l_e = 0; l_e < 2; l_e++) {
      const glm::vec3 l_p = l_n + (l_e == 0 ? l_t0 : l_t1) * l_d;
      o_minX = std::min(o_minX, l_p.x);
      o_maxX = std::max(o_maxX, l_p.x);
      o_minZ = std::min(o_minZ, l_p.z);
      o_maxZ = std::max(o_maxZ, l_p.z);
    }
  }
}

// Maps a world-coordinate window [i_lo, i_hi] to the covered vertex-index
// range of a grid axis whose vertices run linearly from i_a0 (vertex 0) to
// i_a1 (vertex i_n-1); i_a1 < i_a0 is fine (the region/globe Z axis is
// inverted). One extra vertex of margin on each side.
inline void visibleRange(float i_a0,
                         float i_a1,
                         int i_n,
                         float i_lo,
                         float i_hi,
                         int& o_0,
                         int& o_1) {
  if (i_n < 2 || i_a1 == i_a0) {
    o_0 = 0;
    o_1 = i_n - 1;
    return;
  }
  float l_f0 = (i_lo - i_a0) / (i_a1 - i_a0) * (float)(i_n - 1);
  float l_f1 = (i_hi - i_a0) / (i_a1 - i_a0) * (float)(i_n - 1);
  if (l_f0 > l_f1)
    std::swap(l_f0, l_f1);
  // Clamp before the int cast: an unbounded footprint is ±3.4e38.
  l_f0 = std::max(-2.0f, std::min((float)i_n + 1.0f, l_f0));
  l_f1 = std::max(-2.0f, std::min((float)i_n + 1.0f, l_f1));
  o_0 = std::max(0, (int)std::floor(l_f0) - 1);
  o_1 = std::min(i_n - 1, (int)std::ceil(l_f1) + 1);
}

// Draws the quads of LOD level log2(i_stride) that cover the stride-1 vertex
// window [i_i0..i_i1] × [i_j0..i_j1], as one glDrawElements per quad row (the
// row-major index layout makes each row's window a contiguous span). The
// matching LOD index buffer must be bound as GL_ELEMENT_ARRAY_BUFFER. This is
// what makes close zooms cheap: only on-screen rows/columns are submitted
// instead of the whole mesh.
inline void drawGridWindow(
    int i_w, int i_h, int i_stride, int i_i0, int i_i1, int i_j0, int i_j1) {
  if (i_i1 < i_i0 || i_j1 < i_j0 || i_w < 2 || i_h < 2)
    return;
  const int l_cols = (i_w - 2) / i_stride + 1; // quads per row at this level
  const int l_rows = (i_h - 2) / i_stride + 1;
  const int l_c0 = std::min(i_i0 / i_stride, l_cols - 1);
  const int l_c1 = std::min(i_i1 / i_stride, l_cols - 1);
  const int l_r0 = std::min(i_j0 / i_stride, l_rows - 1);
  const int l_r1 = std::min(i_j1 / i_stride, l_rows - 1);
  const GLsizei l_cnt = (GLsizei)((l_c1 - l_c0 + 1) * 6);
  for (int l_r = l_r0; l_r <= l_r1; l_r++)
    glDrawElements(GL_TRIANGLES, l_cnt, GL_UNSIGNED_INT,
                   (const void*)((size_t)((size_t)l_r * l_cols + l_c0) * 6 *
                                 sizeof(unsigned int)));
}

} // namespace lod
} // namespace visualization
} // namespace tsunami_lab

#endif
