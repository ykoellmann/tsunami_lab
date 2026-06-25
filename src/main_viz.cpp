#include "displacement/GaussianDisplacement.h"
#include "visualization/Camera.h"
#include "visualization/Gebco.h"
#include "visualization/GlobeView.h"
#include "visualization/RegionView.h"
#include "visualization/Ui.h"
#include "visualization/Window.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <future>
#include <imgui.h>
#include <string>

// ────────────────────────────────────────────────────────────────────────────
// Application state
// ────────────────────────────────────────────────────────────────────────────

enum class AppState { REGION_SELECT, REGION_PREVIEW, SIMULATING };

static AppState g_state = AppState::REGION_SELECT;

// ────────────────────────────────────────────────────────────────────────────
// Global input state (filled by GLFW callbacks)
// ────────────────────────────────────────────────────────────────────────────

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
      } else if (g_dragDist < 5.0f) {
        // Released without dragging: treat as a click and place a source.
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

// ────────────────────────────────────────────────────────────────────────────
// Camera helpers
// ────────────────────────────────────────────────────────────────────────────

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

// ────────────────────────────────────────────────────────────────────────────
// ImGui panels
// ────────────────────────────────────────────────────────────────────────────

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

  // ── City search ──────────────────────────────────────────────────────────
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

  // ── Selection size limit ─────────────────────────────────────────────────
  ImGui::Spacing();
  ImGui::SeparatorText("Einstellungen");
  ImGui::SliderFloat("Max. Ausdehnung (°)", &globeView.maxSelDeg, 2.0f, 45.0f);

  // ── Globe resolution ─────────────────────────────────────────────────────
  using GV = tsunami_lab::visualization::GlobeView;
  ImGui::SliderInt("Detailgrad (Welt)", &g_globeMaxDim, GV::MIN_LON_SAMPLES,
                   GV::MAX_LON_SAMPLES);
  ImGui::TextDisabled("Welt-Gitter: bis ~%d Punkte breit", g_globeMaxDim);
  if (g_globeMaxDim != globeView.resolution()) {
    if (ImGui::Button("Auflösung anwenden", ImVec2(-1, 0)))
      globeView.setResolution(g_globeMaxDim);
  }

  // ── Selection info ───────────────────────────────────────────────────────
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
  ImGui::SeparatorText("Weiter");
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.65f, 0.2f, 1));
  if (ImGui::Button("Simulieren  >>", ImVec2(-1, 0))) {
    // TODO: trigger SolverThread on this region's bathymetry (Phase 5)
    g_state = AppState::SIMULATING;
  }
  ImGui::PopStyleColor();

  ImGui::Spacing();
  if (ImGui::Button("← Zurück zur Gebietsauswahl", ImVec2(-1, 0))) {
    g_state = AppState::REGION_SELECT;
    if (g_camera)
      setCameraGlobeView(*g_camera);
  }

  ImGui::End();
}

static void drawSimulatingUi() {
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_Always);
  ImGui::Begin("##sim_panel", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::TextColored(ImVec4(0.2f, 0.8f, 1, 1), "Simulation");
  ImGui::Separator();
  ImGui::TextDisabled("(Simulation wird in Phase 5 implementiert)");
  ImGui::Spacing();
  if (ImGui::Button("← Zurück zur Gebietsauswahl", ImVec2(-1, 0))) {
    g_state = AppState::REGION_SELECT;
    if (g_camera)
      setCameraGlobeView(*g_camera);
  }

  ImGui::End();
}

// ────────────────────────────────────────────────────────────────────────────
// Main
// ────────────────────────────────────────────────────────────────────────────

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

    // ── Keyboard camera pan/orbit (WASD + arrow keys) ──────────────────────
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
            g_state == AppState::REGION_PREVIEW ||
            g_state == AppState::SIMULATING)
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

    // ── Build VP matrix ────────────────────────────────────────────────────
    float aspect = (l_winH > 0) ? (float)l_winW / (float)l_winH : 1.0f;
    glm::mat4 vp = l_camera.projection(aspect) * l_camera.view();

    // ── Draw ───────────────────────────────────────────────────────────────
    if (g_state == AppState::REGION_SELECT)
      l_globe.draw(vp);
    else if (g_state == AppState::REGION_PREVIEW)
      l_region.draw(vp);

    // ── ImGui ──────────────────────────────────────────────────────────────
    l_ui.beginFrame();
    if (g_state == AppState::REGION_SELECT)
      drawGlobeUi(l_globe);
    else if (g_state == AppState::REGION_PREVIEW)
      drawRegionUi(l_region);
    else
      drawSimulatingUi();
    l_ui.endFrame();

    l_window.swapBuffers();
  }

  l_ui.shutdown();
  return 0;
}
