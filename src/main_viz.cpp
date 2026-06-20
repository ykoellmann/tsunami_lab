#include "visualization/Camera.h"
#include "visualization/GlobeView.h"
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

enum class AppState { REGION_SELECT, SIMULATING };

static AppState g_state = AppState::REGION_SELECT;

// ────────────────────────────────────────────────────────────────────────────
// Global input state (filled by GLFW callbacks)
// ────────────────────────────────────────────────────────────────────────────

static tsunami_lab::visualization::Camera* g_camera = nullptr;
static tsunami_lab::visualization::GlobeView* g_globeView = nullptr;

static bool g_mouseLeft = false;
static bool g_mouseMiddle = false;
static double g_lastX = 0, g_lastY = 0;
static int g_screenW = 1280, g_screenH = 720;

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
    if (g_mouseLeft)
      g_camera->onMouseDrag(dx, dy);
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
    if (ImGui::Button("Simulieren  >>", ImVec2(-1, 0))) {
      // TODO: trigger GebcoLoader + SolverThread here (Phase 4/5)
      g_state = AppState::SIMULATING;
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
    if (ImGui::Button("Auswahl löschen", ImVec2(-1, 0)))
      globeView.clearSelection();
  } else {
    ImGui::TextDisabled("(noch keine Auswahl)");
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
  tsunami_lab::visualization::Window l_window(1280, 720,
                                              "Tsunami Lab — Gebietsauswahl");
  tsunami_lab::visualization::Camera l_camera;
  tsunami_lab::visualization::GlobeView l_globe;
  tsunami_lab::visualization::Ui l_ui;

  g_camera = &l_camera;
  g_globeView = &l_globe;

  glfwSetMouseButtonCallback(l_window.handle(), onMouseButton);
  glfwSetCursorPosCallback(l_window.handle(), onCursorPos);
  glfwSetScrollCallback(l_window.handle(), onScroll);

  l_ui.init(l_window.handle());

  std::printf("OpenGL %s\n", glGetString(GL_VERSION));
  glEnable(GL_DEPTH_TEST);

  // Initialise globe view (loads GEBCO data)
  std::printf("Lade GEBCO-Daten …\n");
  l_globe.init("data/GEBCO_2025.nc");
  std::printf("Fertig.\n");

  // Set camera for flat-map globe view
  setCameraGlobeView(l_camera);

  while (!l_window.shouldClose()) {
    l_window.pollEvents();

    int l_w, l_h;
    l_window.getSize(l_w, l_h);
    g_screenW = l_w;
    g_screenH = l_h;
    glViewport(0, 0, l_w, l_h);
    glClearColor(0.06f, 0.09f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ── Build VP matrix ────────────────────────────────────────────────────
    float aspect = (l_h > 0) ? (float)l_w / (float)l_h : 1.0f;
    glm::mat4 vp = l_camera.projection(aspect) * l_camera.view();

    // ── Draw ───────────────────────────────────────────────────────────────
    if (g_state == AppState::REGION_SELECT)
      l_globe.draw(vp);

    // ── ImGui ──────────────────────────────────────────────────────────────
    l_ui.beginFrame();
    if (g_state == AppState::REGION_SELECT)
      drawGlobeUi(l_globe);
    else
      drawSimulatingUi();
    l_ui.endFrame();

    l_window.swapBuffers();
  }

  l_ui.shutdown();
  return 0;
}
