#include "displacement/GaussianDisplacement.h"
#include "visualization/Camera.h"
#include "visualization/Gebco.h"
#include "visualization/GlobeView.h"
#include "visualization/RegionView.h"
#include "visualization/SimBuffer.h"
#include "visualization/SolverThread.h"
#include "visualization/Ui.h"
#include "visualization/Window.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <future>
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

// Application state

enum class AppState { REGION_SELECT, REGION_PREVIEW };

static AppState g_state = AppState::REGION_SELECT;

// Global input state (filled by GLFW callbacks)

static tsunami_lab::visualization::Camera* g_camera = nullptr;
static tsunami_lab::visualization::GlobeView* g_globeView = nullptr;
static tsunami_lab::visualization::RegionView* g_regionView = nullptr;

// Path to the local GEBCO grid (resolved/downloaded at startup).
static std::string g_gebcoPath;
// Set when a region read fails, so the UI can show feedback.
static std::string g_regionError;
// Max samples per axis when reading GEBCO for the bathymetry view.
// 0 = native resolution (no stride); positive = cap (faster for large areas).
static int g_bathMaxDim = 0;
// Longitude sample count for the whole-globe terrain mesh (bound to the
// resolution slider in the globe view).
static int g_globeMaxDim =
    tsunami_lab::visualization::GlobeView::DEFAULT_LON_SAMPLES;
// Last selection that was successfully loaded (used by the region-view reload).
static tsunami_lab::visualization::BBox g_loadedSel;

static bool g_mouseLeft = false;
static bool g_mouseMiddle = false;
static double g_lastX = 0, g_lastY = 0;
static int g_screenW = 1280, g_screenH = 720;

// Distinguishes a click (place displacement) from a drag (rotate) in the
// region preview.
static double g_pressX = 0, g_pressY = 0;
static float g_dragDist = 0.0f;

// Gaussian displacement parameters, adjustable in the preview UI.
static float g_displAmplitude = 5.0f; // peak uplift (m)
static float g_displSigma = 15000.0f; // characteristic radius (m)
static double g_epiLon = 0, g_epiLat = 0;

// Live simulation (created on "Simulieren", destroyed on stop / leaving view).
static std::unique_ptr<tsunami_lab::visualization::SimBuffer> g_simBuf;
static std::unique_ptr<tsunami_lab::visualization::SolverThread> g_sim;
// Simulation resolution: edge length of one cell in metres. The cell count
// follows from the region size. Capped per axis (see simGridFor) so a fine
// resolution on a huge region cannot blow up the grid.
static float g_simCellSize = 1000.0f;
// Playback speed: simulated seconds advanced per real second.
static float g_simSpeed = 120.0f;
// When set, the playback speed follows the largest rate the CPU can sustain.
static bool g_simAutoSpeed = false;

// Intersect the ray through pixel (i_mx, i_my) with the y=0 sea-level plane and
// return the world (x, z) hit point.
static glm::vec2
regionUnproject(float i_mx,
                float i_my,
                int i_w,
                int i_h,
                const tsunami_lab::visualization::Camera& i_cam) {
  float l_aspect = (i_h > 0) ? (float)i_w / (float)i_h : 1.0f;
  float l_ndcX = (2.0f * i_mx / (float)i_w) - 1.0f;
  float l_ndcY = 1.0f - (2.0f * i_my / (float)i_h);
  glm::mat4 l_invVP = glm::inverse(i_cam.projection(l_aspect) * i_cam.view());
  glm::vec4 l_near = l_invVP * glm::vec4(l_ndcX, l_ndcY, -1.0f, 1.0f);
  glm::vec4 l_far = l_invVP * glm::vec4(l_ndcX, l_ndcY, 1.0f, 1.0f);
  l_near /= l_near.w;
  l_far /= l_far.w;
  glm::vec3 l_dir = glm::normalize(glm::vec3(l_far) - glm::vec3(l_near));
  if (std::abs(l_dir.y) < 1e-6f)
    return {0.0f, 0.0f};
  float l_t = -l_near.y / l_dir.y;
  glm::vec3 l_world = glm::vec3(l_near) + l_t * l_dir;
  return {l_world.x, l_world.z};
}

// Place a Gaussian displacement at the clicked spot in the region preview.
static void placeDisplacement(float i_mx, float i_my) {
  if (!g_regionView || !g_regionView->loaded() || !g_camera)
    return;
  glm::vec2 l_world =
      regionUnproject(i_mx, i_my, g_screenW, g_screenH, *g_camera);
  tsunami_lab::displacement::GaussianDisplacement l_model(g_displAmplitude,
                                                          g_displSigma);
  g_regionView->applyDisplacement(l_world.x, l_world.y, l_model);
  g_regionView->worldToLonLat(l_world.x, l_world.y, g_epiLon, g_epiLat);
}

// Live simulation setup

// Bilinear sample of a row-major w×h grid at fractional (i_fx, i_fy), clamped.
static float sampleGrid(
    const std::vector<float>& i_a, int i_w, int i_h, double i_fx, double i_fy) {
  i_fx = std::min(std::max(i_fx, 0.0), (double)(i_w - 1));
  i_fy = std::min(std::max(i_fy, 0.0), (double)(i_h - 1));
  int l_x0 = (int)i_fx, l_y0 = (int)i_fy;
  int l_x1 = std::min(l_x0 + 1, i_w - 1), l_y1 = std::min(l_y0 + 1, i_h - 1);
  double l_tx = i_fx - l_x0, l_ty = i_fy - l_y0;
  float l_v00 = i_a[(size_t)l_y0 * i_w + l_x0];
  float l_v01 = i_a[(size_t)l_y0 * i_w + l_x1];
  float l_v10 = i_a[(size_t)l_y1 * i_w + l_x0];
  float l_v11 = i_a[(size_t)l_y1 * i_w + l_x1];
  double l_top = l_v00 * (1 - l_tx) + l_v01 * l_tx;
  double l_bot = l_v10 * (1 - l_tx) + l_v11 * l_tx;
  return (float)(l_top * (1 - l_ty) + l_bot * l_ty);
}

// Metric width/height (metres) of a lon/lat box at its centre latitude.
static void regionMetres(double i_lonMin,
                         double i_lonMax,
                         double i_latMin,
                         double i_latMax,
                         double& o_widthM,
                         double& o_heightM) {
  const double l_latC = 0.5 * (i_latMin + i_latMax);
  const double l_mPerLat = 111132.0;
  const double l_mPerLon = 111320.0 * std::cos(l_latC * M_PI / 180.0);
  o_widthM = (i_lonMax - i_lonMin) * l_mPerLon;
  o_heightM = (i_latMax - i_latMin) * l_mPerLat;
}

// Grid (nx × ny) and effective cell size for a region of the given metric size
// at the requested resolution i_cellSize (m). If the resulting grid would
// exceed the per-axis cap, the cell size is coarsened so the grid stays square
// and bounded; o_dxy then reports the effective resolution actually used.
static void simGridFor(double i_widthM,
                       double i_heightM,
                       float i_cellSize,
                       tsunami_lab::t_idx& o_nx,
                       tsunami_lab::t_idx& o_ny,
                       double& o_dxy) {
  using tsunami_lab::t_idx;
  constexpr t_idx k_cap = 4000; // safety ceiling per axis
  o_dxy = std::max(1.0, (double)i_cellSize);
  o_nx = std::max<t_idx>(2, (t_idx)std::llround(i_widthM / o_dxy));
  o_ny = std::max<t_idx>(2, (t_idx)std::llround(i_heightM / o_dxy));
  if (o_nx > k_cap || o_ny > k_cap) {
    const double l_f = std::max((double)o_nx / k_cap, (double)o_ny / k_cap);
    o_dxy *= l_f;
    o_nx = std::max<t_idx>(2, (t_idx)std::llround(i_widthM / o_dxy));
    o_ny = std::max<t_idx>(2, (t_idx)std::llround(i_heightM / o_dxy));
  }
}

// Resamples the selected region onto a square-cell grid and builds the initial
// bathymetry + water column (still water to sea level, plus the placed quake
// uplift). Returns false if the GEBCO read fails.
static bool buildSimSetup(tsunami_lab::t_idx& o_nx,
                          tsunami_lab::t_idx& o_ny,
                          float& o_dxy,
                          std::vector<float>& o_bath,
                          std::vector<float>& o_height) {
  using namespace tsunami_lab;
  namespace gv = tsunami_lab::visualization;

  if (g_gebcoPath.empty() || !g_loadedSel.valid())
    return false;

  gv::gebco::Region l_src;
  if (!gv::gebco::readRegion(g_gebcoPath, g_loadedSel, l_src, 1200) ||
      l_src.w < 2 || l_src.h < 2)
    return false;

  const double l_latC = 0.5 * (l_src.latMin + l_src.latMax);
  const double l_mPerLat = 111132.0;
  const double l_mPerLon = 111320.0 * std::cos(l_latC * M_PI / 180.0);
  double l_widthM = 0.0, l_heightM = 0.0;
  regionMetres(l_src.lonMin, l_src.lonMax, l_src.latMin, l_src.latMax, l_widthM,
               l_heightM);
  if (l_widthM <= 0.0 || l_heightM <= 0.0)
    return false;

  // Square cells at the requested resolution (cell size in metres); the grid
  // count follows from the region size, capped per axis for real-time stepping.
  double l_dxy = 0.0;
  t_idx l_nx = 0, l_ny = 0;
  simGridFor(l_widthM, l_heightM, g_simCellSize, l_nx, l_ny, l_dxy);

  o_nx = l_nx;
  o_ny = l_ny;
  o_dxy = (float)l_dxy;
  o_bath.assign((size_t)l_nx * l_ny, 0.0f);
  o_height.assign((size_t)l_nx * l_ny, 0.0f);

  const bool l_hasDisp = g_regionView && g_regionView->hasDisplacement();
  tsunami_lab::displacement::GaussianDisplacement l_model(g_displAmplitude,
                                                          g_displSigma);

  for (t_idx l_j = 0; l_j < l_ny; l_j++) {
    const double l_lat = l_src.latMin + (double)l_j *
                                            (l_src.latMax - l_src.latMin) /
                                            (double)(l_ny - 1);
    const double l_fy = (l_lat - l_src.latMin) / (l_src.latMax - l_src.latMin) *
                        (double)(l_src.h - 1);
    for (t_idx l_i = 0; l_i < l_nx; l_i++) {
      const double l_lon = l_src.lonMin + (double)l_i *
                                              (l_src.lonMax - l_src.lonMin) /
                                              (double)(l_nx - 1);
      const double l_fx = (l_lon - l_src.lonMin) /
                          (l_src.lonMax - l_src.lonMin) * (double)(l_src.w - 1);

      const float l_b = sampleGrid(l_src.elev, l_src.w, l_src.h, l_fx, l_fy);
      const size_t l_k = (size_t)l_j * l_nx + l_i;
      o_bath[l_k] = l_b;

      float l_h = (l_b < 0.0f) ? -l_b : 0.0f; // still water to sea level
      if (l_hasDisp && l_b < 0.0f) {
        const double l_east = (l_lon - g_epiLon) * l_mPerLon;
        const double l_north = (l_lat - g_epiLat) * l_mPerLat;
        l_h += (float)l_model.verticalDisplacement(l_east, l_north);
        if (l_h < 0.0f)
          l_h = 0.0f;
      }
      o_height[l_k] = l_h;
    }
  }
  return true;
}

static void stopSimulation() {
  if (g_sim) {
    g_sim->stop();
    g_sim.reset();
  }
  g_simBuf.reset();
  if (g_regionView)
    g_regionView->endSimulation();
}

static void startSimulation() {
  using namespace tsunami_lab;
  namespace gv = tsunami_lab::visualization;

  stopSimulation();

  t_idx l_nx = 0, l_ny = 0;
  float l_dxy = 0.0f;
  std::vector<float> l_bath, l_height;
  if (!buildSimSetup(l_nx, l_ny, l_dxy, l_bath, l_height)) {
    g_regionError = "Simulation: GEBCO-Setup fehlgeschlagen.";
    return;
  }

  g_simBuf.reset(new gv::SimBuffer(l_nx, l_ny));
  g_sim.reset(new gv::SolverThread(*g_simBuf, l_nx, l_ny, l_dxy));

  patches::WavePropagation2d& l_solver = g_sim->solver();
  for (t_idx l_j = 0; l_j < l_ny; l_j++)
    for (t_idx l_i = 0; l_i < l_nx; l_i++) {
      const size_t l_k = (size_t)l_j * l_nx + l_i;
      l_solver.setBathymetry(l_i, l_j, l_bath[l_k]);
      l_solver.setHeight(l_i, l_j, l_height[l_k]);
      l_solver.setMomentumX(l_i, l_j, 0.0f);
      l_solver.setMomentumY(l_i, l_j, 0.0f);
    }

  if (g_regionView)
    g_regionView->beginSimulation(l_nx, l_ny, l_bath.data());
  g_sim->setTimeScale(g_simSpeed);
  g_sim->start();
}

static bool simRunning() { return g_sim && g_sim->running(); }

static void onMouseButton(GLFWwindow*, int i_btn, int i_act, int) {
  if (ImGui::GetIO().WantCaptureMouse)
    return;

  if (i_btn == GLFW_MOUSE_BUTTON_LEFT) {
    bool pressed = (i_act == GLFW_PRESS);
    g_mouseLeft = pressed;

    if (g_state == AppState::REGION_SELECT && g_globeView) {
      if (pressed)
        g_globeView->onMousePress((float)g_lastX, (float)g_lastY, g_screenW,
                                  g_screenH, *g_camera);
      else
        g_globeView->onMouseRelease();
    } else if (g_state == AppState::REGION_PREVIEW) {
      if (pressed) {
        g_pressX = g_lastX;
        g_pressY = g_lastY;
        g_dragDist = 0.0f;
      } else if (g_dragDist < 5.0f && !simRunning()) {
        // Released without dragging: treat as a click and place a source.
        // Disabled while a simulation is running (the field is live then).
        placeDisplacement((float)g_lastX, (float)g_lastY);
      }
    }
  }
  if (i_btn == GLFW_MOUSE_BUTTON_MIDDLE)
    g_mouseMiddle = (i_act == GLFW_PRESS);
}

static void onCursorPos(GLFWwindow*, double i_x, double i_y) {
  float dx = (float)(i_x - g_lastX);
  float dy = (float)(i_y - g_lastY);
  g_lastX = i_x;
  g_lastY = i_y;

  if (!g_camera)
    return;

  if (g_state == AppState::REGION_SELECT) {
    // Left drag → selection rectangle
    if (g_mouseLeft && g_globeView)
      g_globeView->onMouseMove((float)i_x, (float)i_y, g_screenW, g_screenH,
                               *g_camera);
    // Middle drag → flat-map pan
    if (g_mouseMiddle)
      g_camera->onMapPan(dx, dy);
  } else {
    // Simulation view: normal arcball
    if (g_mouseLeft) {
      g_dragDist += std::abs(dx) + std::abs(dy);
      g_camera->onMouseDrag(dx, dy);
    }
    if (g_mouseMiddle)
      g_camera->onMiddleDrag(dx, dy);
  }
}

static void onScroll(GLFWwindow*, double, double i_dy) {
  if (ImGui::GetIO().WantCaptureMouse)
    return;
  if (g_camera)
    g_camera->onScroll((float)i_dy);
}

// Camera helpers

static void setCameraGlobeView(tsunami_lab::visualization::Camera& cam) {
  // With azimuth=0 and the lat-negation in the vertex shader (Z = -lat):
  //   right = (+1,0,0) → east to the right  ✓
  //   screen-up ≈ (0, 0, -1) world dir → north (negative Z) at top  ✓
  cam.setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
  cam.setAzimuth(0.0f);
  cam.setElevation(1.5f); // nearly top-down (avoids lookAt singularity at π/2)
  cam.setDistance(300.0f);
}

static void setCameraRegionView(tsunami_lab::visualization::Camera& cam) {
  // The region mesh is normalised to ~200 world units across and centred at
  // the origin; an angled 3/4 view shows the relief.
  cam.setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
  cam.setAzimuth(0.0f);
  cam.setElevation(0.7f);
  cam.setDistance(330.0f);
}

// ImGui panels

static std::future<std::pair<float, float>> g_geocodeFuture;
static bool g_geocodeRunning = false;
static std::string g_geocodeError;

static void zoomToLocation(float i_lon, float i_lat, float i_zoom = 25.0f) {
  if (g_camera) {
    // World Z = -lat (see vertex shader negation)
    g_camera->setTarget(glm::vec3(i_lon, 0.0f, -i_lat));
    g_camera->setDistance(i_zoom);
  }
}

static void drawGlobeUi(tsunami_lab::visualization::GlobeView& globeView) {
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
  ImGui::Begin("##globe_panel", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::TextColored(ImVec4(1, 0.85f, 0.1f, 1), "Gebiet auswählen");
  ImGui::Separator();

  ImGui::TextWrapped("Linksklick + Ziehen:  Gebiet zeichnen\n"
                     "Mittelklick + Ziehen: Karte verschieben\n"
                     "WASD / Pfeiltasten:   Karte verschieben\n"
                     "Scrollrad:            Zoom");
  ImGui::Spacing();

  ImGui::SeparatorText("Stadtsuche");
  static char cityBuf[128] = {};
  bool doSearch = false;
  ImGui::SetNextItemWidth(200);
  if (ImGui::InputText("##city", cityBuf, sizeof(cityBuf),
                       ImGuiInputTextFlags_EnterReturnsTrue))
    doSearch = true;
  ImGui::SameLine();
  if (g_geocodeRunning) {
    ImGui::BeginDisabled();
    ImGui::Button("...");
    ImGui::EndDisabled();
  } else {
    if (ImGui::Button("Suchen"))
      doSearch = true;
  }

  if (doSearch && !g_geocodeRunning && cityBuf[0] != '\0') {
    g_geocodeError = "";
    g_geocodeRunning = true;
    std::string city = cityBuf;
    g_geocodeFuture = std::async(std::launch::async, [city]() {
      return tsunami_lab::visualization::GlobeView::geocodeCity(city);
    });
  }

  // Poll async result
  if (g_geocodeRunning && g_geocodeFuture.valid()) {
    if (g_geocodeFuture.wait_for(std::chrono::seconds(0)) ==
        std::future_status::ready) {
      std::pair<float, float> l_res = g_geocodeFuture.get();
      g_geocodeRunning = false;
      if (std::abs(l_res.first) > 0.001f || std::abs(l_res.second) > 0.001f)
        zoomToLocation(l_res.first, l_res.second, 20.0f);
      else
        g_geocodeError = "Stadt nicht gefunden.";
    }
  }

  if (!g_geocodeError.empty())
    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", g_geocodeError.c_str());

  ImGui::Spacing();
  ImGui::SeparatorText("Einstellungen");
  ImGui::SliderFloat("Max. Ausdehnung (°)", &globeView.maxSelDeg, 2.0f, 500.0f);

  using GV = tsunami_lab::visualization::GlobeView;
  ImGui::SliderInt("Detailgrad (Welt)", &g_globeMaxDim, GV::MIN_LON_SAMPLES,
                   GV::MAX_LON_SAMPLES);
  ImGui::TextDisabled("Welt-Gitter: bis ~%d Punkte breit", g_globeMaxDim);
  if (g_globeMaxDim != globeView.resolution()) {
    if (ImGui::Button("Auflösung anwenden", ImVec2(-1, 0)))
      globeView.setResolution(g_globeMaxDim);
  }

  ImGui::Spacing();
  ImGui::SeparatorText("Auswahl");
  if (globeView.hasSelection()) {
    tsunami_lab::visualization::BBox sel = globeView.getSelection();
    ImGui::Text("Lon: %.1f° – %.1f°  (%.1f°)", sel.lonMin, sel.lonMax,
                sel.lonSpan());
    ImGui::Text("Lat: %.1f° – %.1f°  (%.1f°)", sel.latMin, sel.latMax,
                sel.latSpan());

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.65f, 0.2f, 1));
    if (ImGui::Button("Bathymetrie laden  >>", ImVec2(-1, 0))) {
      g_regionError.clear();
      if (g_gebcoPath.empty()) {
        g_regionError = "Keine GEBCO-Daten verfügbar.";
      } else {
        tsunami_lab::visualization::gebco::Region l_reg;
        if (tsunami_lab::visualization::gebco::readRegion(
                g_gebcoPath, sel, l_reg, g_bathMaxDim) &&
            g_regionView && g_regionView->load(l_reg)) {
          g_loadedSel = sel;
          g_state = AppState::REGION_PREVIEW;
          if (g_camera)
            setCameraRegionView(*g_camera);
        } else {
          g_regionError = "Laden der Bathymetrie fehlgeschlagen.";
        }
      }
    }
    ImGui::PopStyleColor();

    if (!g_regionError.empty())
      ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", g_regionError.c_str());

    ImGui::Spacing();
    if (ImGui::Button("Auswahl löschen", ImVec2(-1, 0)))
      globeView.clearSelection();
  } else {
    ImGui::TextDisabled("(noch keine Auswahl)");
  }

  ImGui::End();
}

static void drawRegionUi(tsunami_lab::visualization::RegionView& regionView) {
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_Always);
  ImGui::Begin("##region_panel", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::TextColored(ImVec4(0.2f, 0.8f, 1, 1), "Bathymetrie-Vorschau");
  ImGui::Separator();

  ImGui::Text("Lon: %.2f° – %.2f°", regionView.lonMin, regionView.lonMax);
  ImGui::Text("Lat: %.2f° – %.2f°", regionView.latMin, regionView.latMax);
  ImGui::Text("Gitter: %d × %d", regionView.gridW, regionView.gridH);
  ImGui::Spacing();

  ImGui::TextWrapped("Linksklick + Ziehen:  Drehen\n"
                     "Mittelklick + Ziehen: Verschieben\n"
                     "WASD / Pfeiltasten:   Verschieben\n"
                     "Scrollrad:            Zoom");
  ImGui::Spacing();

  ImGui::SeparatorText("Auflösung");
  {
    bool l_native = (g_bathMaxDim == 0);
    if (ImGui::Checkbox("Nativ (volle GEBCO-Auflösung)", &l_native))
      g_bathMaxDim = l_native ? 0 : 4000;
    if (!l_native) {
      ImGui::SetNextItemWidth(-1);
      ImGui::SliderInt("Max. Samples", &g_bathMaxDim, 500, 16000);
    }
    if (ImGui::Button("Neu laden", ImVec2(-1, 0)) && g_loadedSel.valid() &&
        !g_gebcoPath.empty()) {
      tsunami_lab::visualization::gebco::Region l_reg;
      if (tsunami_lab::visualization::gebco::readRegion(
              g_gebcoPath, g_loadedSel, l_reg, g_bathMaxDim) &&
          regionView.load(l_reg))
        regionView.clearDisplacement();
      else
        g_regionError = "Neu laden fehlgeschlagen.";
    }
    if (!g_regionError.empty())
      ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", g_regionError.c_str());
  }
  ImGui::Spacing();

  ImGui::SeparatorText("Darstellung");
  using Field = tsunami_lab::visualization::RegionView::Field;
  int l_field = (regionView.field == Field::Displacement) ? 1 : 0;
  ImGui::RadioButton("Bathymetrie", &l_field, 0);
  ImGui::SameLine();
  ImGui::RadioButton("Displacement", &l_field, 1);
  regionView.field = (l_field == 1) ? Field::Displacement : Field::Bathymetry;
  ImGui::SliderFloat("Überhöhung", &regionView.vertExaggeration, 1.0f, 100.0f,
                     "%.0f×");
  ImGui::SliderFloat("Wellen-Überhöhung", &regionView.waveExaggeration, 1.0f,
                     5000.0f, "%.0f×", ImGuiSliderFlags_Logarithmic);
  ImGui::Checkbox("Meeresspiegel anzeigen", &regionView.showSea);

  ImGui::Spacing();
  ImGui::SeparatorText("Erdbebenquelle");
  ImGui::TextWrapped("Klick auf das Gelände: Gauss-Auslenkung setzen");
  ImGui::SliderFloat("Amplitude (m)", &g_displAmplitude, -20.0f, 20.0f, "%.1f");
  ImGui::SliderFloat("Radius (m)", &g_displSigma, 1000.0f, 100000.0f, "%.0f");
  if (regionView.hasDisplacement()) {
    ImGui::Text("Epizentrum: %.2f°, %.2f°", g_epiLon, g_epiLat);
    if (ImGui::Button("Auslenkung entfernen", ImVec2(-1, 0)))
      regionView.clearDisplacement();
  } else {
    ImGui::TextDisabled("(noch keine Auslenkung)");
  }

  ImGui::Spacing();
  ImGui::SeparatorText("Simulation");
  ImGui::SliderFloat("Auflösung (m/Zelle)", &g_simCellSize, 50.0f, 20000.0f,
                     "%.0f m", ImGuiSliderFlags_Logarithmic);
  {
    // Live preview of the resulting grid from the current selection bounds.
    double l_w = 0.0, l_h = 0.0;
    regionMetres(regionView.lonMin, regionView.lonMax, regionView.latMin,
                 regionView.latMax, l_w, l_h);
    tsunami_lab::t_idx l_pnx = 0, l_pny = 0;
    double l_pdxy = 0.0;
    simGridFor(l_w, l_h, g_simCellSize, l_pnx, l_pny, l_pdxy);
    if (l_pdxy > g_simCellSize * 1.01)
      ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1),
                         "→ %zu × %zu Zellen (begrenzt: eff. %.0f m)",
                         (size_t)l_pnx, (size_t)l_pny, l_pdxy);
    else
      ImGui::TextDisabled("→ %zu × %zu Zellen", (size_t)l_pnx, (size_t)l_pny);
  }
  ImGui::Checkbox("Zeitraffer automatisch (CPU-Limit)", &g_simAutoSpeed);
  if (g_simAutoSpeed) {
    // Couple playback to the largest rate the hardware sustains, so the wave
    // never silently falls behind the requested speed.
    const double l_max = simRunning() ? g_sim->maxTimeScale() : 0.0;
    if (simRunning())
      g_sim->setTimeScale(l_max); // 0 until measured ⇒ uncapped warm-up
    ImGui::BeginDisabled();
    ImGui::SliderFloat("Zeitraffer (Sim-s/s)", &g_simSpeed, 1.0f, 2000.0f,
                       "%.0f×", ImGuiSliderFlags_Logarithmic);
    ImGui::EndDisabled();
    if (l_max > 0.0)
      ImGui::Text("läuft bei ~%.0f× (CPU-Limit)", l_max);
    else
      ImGui::TextDisabled("(CPU-Limit wird gemessen …)");
  } else {
    ImGui::SliderFloat("Zeitraffer (Sim-s/s)", &g_simSpeed, 1.0f, 2000.0f,
                       "%.0f×", ImGuiSliderFlags_Logarithmic);
    if (simRunning()) {
      g_sim->setTimeScale(g_simSpeed);
      const double l_max = g_sim->maxTimeScale();
      if (l_max > 0.0 && g_simSpeed > l_max * 1.05)
        ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1),
                           "CPU schafft nur ~%.0f× – Rest wird gekappt.",
                           l_max);
    }
  }
  if (!simRunning()) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.65f, 0.2f, 1));
    if (ImGui::Button("Simulieren  ▶", ImVec2(-1, 0)))
      startSimulation();
    ImGui::PopStyleColor();
  } else {
    ImGui::TextColored(ImVec4(0.2f, 0.8f, 1, 1), "läuft – %zu Schritte",
                       (size_t)g_sim->steps());
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.2f, 0.15f, 1));
    if (ImGui::Button("Stop  ■", ImVec2(-1, 0)))
      stopSimulation();
    ImGui::PopStyleColor();
  }

  ImGui::Spacing();
  if (ImGui::Button("← Zurück zur Gebietsauswahl", ImVec2(-1, 0))) {
    stopSimulation();
    g_state = AppState::REGION_SELECT;
    if (g_camera)
      setCameraGlobeView(*g_camera);
  }

  ImGui::End();
}

// One colour stop along a horizontal legend bar (t in [0,1] = left→right).
struct LegendStop {
  float t;
  ImU32 c;
};

// Shared geometry so the two legends stack cleanly in the bottom-right corner.
static const float k_legW = 232.0f;
static const float k_legH = 80.0f;
static const float k_legPad = 12.0f;
static const float k_legGap = 8.0f;

// Horizontal colour-scale legend: title, gradient bar, end labels. Both the
// wave and terrain legends use this identical layout (only the colours and
// labels differ).
static void drawHLegend(const char* i_id,
                        ImVec2 i_pos,
                        const char* i_title,
                        const LegendStop* i_stops,
                        int i_n,
                        const char* i_left,
                        const char* i_right) {
  const float l_barW = 214.0f, l_barH = 16.0f;
  ImGui::SetNextWindowPos(i_pos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(k_legW, k_legH), ImGuiCond_Always);
  ImGui::Begin(i_id, nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

  ImGui::TextUnformatted(i_title);

  ImDrawList* l_dl = ImGui::GetWindowDrawList();
  const ImVec2 l_p = ImGui::GetCursorScreenPos();
  for (int l_i = 0; l_i < i_n - 1; l_i++) {
    const float l_x0 = l_p.x + i_stops[l_i].t * l_barW;
    const float l_x1 = l_p.x + i_stops[l_i + 1].t * l_barW;
    if (l_x1 <= l_x0)
      continue; // zero-width segment (e.g. the coastline at sea level)
    l_dl->AddRectFilledMultiColor(ImVec2(l_x0, l_p.y),
                                  ImVec2(l_x1, l_p.y + l_barH), i_stops[l_i].c,
                                  i_stops[l_i + 1].c, i_stops[l_i + 1].c,
                                  i_stops[l_i].c);
  }
  ImGui::Dummy(ImVec2(l_barW, l_barH));

  const float l_tw = ImGui::CalcTextSize(i_right).x;
  ImGui::TextUnformatted(i_left);
  ImGui::SameLine(l_barW - l_tw);
  ImGui::TextUnformatted(i_right);

  ImGui::End();
}

// Terrain colour scale, anchored to the bottom-right corner. Stops mirror
// colormap() in region_terrain.frag / globe_terrain.frag, positioned by their
// elevation across the fixed [-6000, +4000] m domain (sea level at t = 0.6).
static void drawBathyLegend(
    const tsunami_lab::visualization::RegionView& regionView) {
  using Field = tsunami_lab::visualization::RegionView::Field;
  if (!regionView.loaded() || regionView.field != Field::Bathymetry)
    return;

  static const LegendStop l_stops[] = {
      {0.00f, IM_COL32(8, 33, 97, 255)},     // -6000 deep ocean
      {0.30f, IM_COL32(23, 71, 148, 255)},   // -3000
      {0.50f, IM_COL32(46, 115, 194, 255)},  // -1000
      {0.58f, IM_COL32(84, 158, 212, 255)},  // -200 shelf
      {0.60f, IM_COL32(130, 189, 219, 255)}, // 0 shallow (coast)
      {0.60f, IM_COL32(69, 140, 69, 255)},   // 0 lowland green (coast)
      {0.61f, IM_COL32(115, 168, 82, 255)},  // 100
      {0.63f, IM_COL32(184, 186, 99, 255)},  // 300
      {0.66f, IM_COL32(199, 168, 115, 255)}, // 600
      {0.72f, IM_COL32(158, 122, 92, 255)},  // 1200
      {0.85f, IM_COL32(122, 102, 92, 255)},  // 2500
      {1.00f, IM_COL32(242, 242, 245, 255)}}; // 4000 snow
  const int l_n = (int)(sizeof(l_stops) / sizeof(l_stops[0]));

  const ImGuiIO& l_io = ImGui::GetIO();
  const ImVec2 l_pos(l_io.DisplaySize.x - k_legW - k_legPad,
                     l_io.DisplaySize.y - k_legH - k_legPad);
  drawHLegend("##bathy_legend", l_pos, "Höhe / Tiefe (m)", l_stops, l_n,
              "-6000 m", "+4000 m");
}

// Wave colour scale, stacked just above the terrain legend during simulation.
// The gradient mirrors jet() in region_water.frag.
static void drawWaveLegend(
    const tsunami_lab::visualization::RegionView& regionView) {
  using Field = tsunami_lab::visualization::RegionView::Field;
  if (!regionView.simulating() || regionView.field != Field::Bathymetry)
    return;

  static const LegendStop l_stops[] = {
      {0.0f, IM_COL32(33, 87, 204, 255)},  {0.2f, IM_COL32(26, 158, 224, 255)},
      {0.4f, IM_COL32(51, 204, 107, 255)}, {0.6f, IM_COL32(242, 224, 51, 255)},
      {0.8f, IM_COL32(247, 133, 31, 255)}, {1.0f, IM_COL32(209, 26, 23, 255)}};

  char l_buf[32];
  std::snprintf(l_buf, sizeof(l_buf), "%.2f m", regionView.waterAnom());

  const ImGuiIO& l_io = ImGui::GetIO();
  const ImVec2 l_pos(
      l_io.DisplaySize.x - k_legW - k_legPad,
      l_io.DisplaySize.y - 2.0f * k_legH - k_legGap - k_legPad);
  drawHLegend("##wave_legend", l_pos, "Wellenhöhe (Anomalie)", l_stops, 6,
              "0 m", l_buf);
}

// Main

int main() {
  // Resolve (and, on first run, download) the GEBCO grid before opening the
  // window — the one-time download prints progress to the terminal.
  g_gebcoPath = tsunami_lab::visualization::gebco::ensureAvailable();

  tsunami_lab::visualization::Window l_window(1280, 720,
                                              "Tsunami Lab — Gebietsauswahl");
  tsunami_lab::visualization::Camera l_camera;
  tsunami_lab::visualization::GlobeView l_globe;
  tsunami_lab::visualization::RegionView l_region;
  tsunami_lab::visualization::Ui l_ui;

  g_camera = &l_camera;
  g_globeView = &l_globe;
  g_regionView = &l_region;

  glfwSetMouseButtonCallback(l_window.handle(), onMouseButton);
  glfwSetCursorPosCallback(l_window.handle(), onCursorPos);
  glfwSetScrollCallback(l_window.handle(), onScroll);

  l_ui.init(l_window.handle());

  std::printf("OpenGL %s\n", glGetString(GL_VERSION));
  glEnable(GL_DEPTH_TEST);

  // Initialise globe overview (subsampled GEBCO) + region preview resources.
  std::printf("Lade GEBCO-Daten …\n");
  l_globe.init(g_gebcoPath.empty() ? nullptr : g_gebcoPath.c_str());
  l_region.init();
  std::printf("Fertig.\n");

  // Set camera for flat-map globe view
  setCameraGlobeView(l_camera);

  double g_lastFrameTime = glfwGetTime();

  while (!l_window.shouldClose()) {
    l_window.pollEvents();

    double l_now = glfwGetTime();
    float l_dt = (float)(l_now - g_lastFrameTime);
    g_lastFrameTime = l_now;

    if (!ImGui::GetIO().WantCaptureKeyboard && g_camera) {
      float l_speed = 200.0f * l_dt;
      float l_kx = 0.0f, l_ky = 0.0f;
      GLFWwindow* l_win = l_window.handle();
      if (glfwGetKey(l_win, GLFW_KEY_A) == GLFW_PRESS ||
          glfwGetKey(l_win, GLFW_KEY_LEFT) == GLFW_PRESS)
        l_kx = -l_speed;
      if (glfwGetKey(l_win, GLFW_KEY_D) == GLFW_PRESS ||
          glfwGetKey(l_win, GLFW_KEY_RIGHT) == GLFW_PRESS)
        l_kx = l_speed;
      if (glfwGetKey(l_win, GLFW_KEY_W) == GLFW_PRESS ||
          glfwGetKey(l_win, GLFW_KEY_UP) == GLFW_PRESS)
        l_ky = -l_speed;
      if (glfwGetKey(l_win, GLFW_KEY_S) == GLFW_PRESS ||
          glfwGetKey(l_win, GLFW_KEY_DOWN) == GLFW_PRESS)
        l_ky = l_speed;

      if (l_kx != 0.0f || l_ky != 0.0f) {
        if (g_state == AppState::REGION_SELECT ||
            g_state == AppState::REGION_PREVIEW)
          g_camera->onMapPan(-l_kx, -l_ky);
      }
    }

    // Framebuffer size (pixels) drives the viewport; window size (logical
    // points) drives mouse→world unprojection so it matches the cursor coords.
    int l_fbW, l_fbH;
    l_window.getSize(l_fbW, l_fbH);
    int l_winW, l_winH;
    l_window.getWindowSize(l_winW, l_winH);
    g_screenW = l_winW;
    g_screenH = l_winH;
    glViewport(0, 0, l_fbW, l_fbH);
    glClearColor(0.06f, 0.09f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (l_winH > 0) ? (float)l_winW / (float)l_winH : 1.0f;
    glm::mat4 vp = l_camera.projection(aspect) * l_camera.view();

    if (simRunning() && g_simBuf->swap())
      l_region.updateWater(g_simBuf->front());

    if (g_state == AppState::REGION_SELECT)
      l_globe.draw(vp);
    else
      l_region.draw(vp);

    l_ui.beginFrame();
    if (g_state == AppState::REGION_SELECT)
      drawGlobeUi(l_globe);
    else {
      drawRegionUi(l_region);
      drawBathyLegend(l_region);
      drawWaveLegend(l_region);
    }
    l_ui.endFrame();

    l_window.swapBuffers();
  }

  stopSimulation();
  l_ui.shutdown();
  return 0;
}
