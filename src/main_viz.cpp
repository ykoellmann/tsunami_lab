#include "visualization/Camera.h"
#include "visualization/Ui.h"
#include "visualization/Window.h"
#include <cstdio>

static tsunami_lab::visualization::Camera* g_camera = nullptr;
static bool g_mouseLeft = false;
static bool g_mouseMiddle = false;
static double g_lastX = 0, g_lastY = 0;

static void onMouseButton(GLFWwindow*, int i_btn, int i_act, int) {
  if (ImGui::GetIO().WantCaptureMouse)
    return;
  if (i_btn == GLFW_MOUSE_BUTTON_LEFT)
    g_mouseLeft = (i_act == GLFW_PRESS);
  if (i_btn == GLFW_MOUSE_BUTTON_MIDDLE)
    g_mouseMiddle = (i_act == GLFW_PRESS);
}
static void onCursorPos(GLFWwindow*, double i_x, double i_y) {
  float l_dx = (float)(i_x - g_lastX);
  float l_dy = (float)(i_y - g_lastY);
  g_lastX = i_x;
  g_lastY = i_y;
  if (!g_camera)
    return;
  if (g_mouseLeft)
    g_camera->onMouseDrag(l_dx, l_dy);
  if (g_mouseMiddle)
    g_camera->onMiddleDrag(l_dx, l_dy);
}
static void onScroll(GLFWwindow*, double, double i_dy) {
  if (ImGui::GetIO().WantCaptureMouse)
    return;
  if (g_camera)
    g_camera->onScroll((float)i_dy);
}

int main() {
  tsunami_lab::visualization::Window l_window(1280, 720, "Tsunami Lab");
  tsunami_lab::visualization::Camera l_camera;
  tsunami_lab::visualization::Ui l_ui;

  g_camera = &l_camera;

  glfwSetMouseButtonCallback(l_window.handle(), onMouseButton);
  glfwSetCursorPosCallback(l_window.handle(), onCursorPos);
  glfwSetScrollCallback(l_window.handle(), onScroll);
  l_ui.init(l_window.handle());

  std::printf("OpenGL %s\n", glGetString(GL_VERSION));
  glEnable(GL_DEPTH_TEST);

  while (!l_window.shouldClose()) {
    l_window.pollEvents();

    int l_w, l_h;
    l_window.getSize(l_w, l_h);
    glViewport(0, 0, l_w, l_h);
    glClearColor(0.08f, 0.10f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    l_ui.beginFrame();
    ImGui::Begin("Tsunami Lab");
    ImGui::Text("OpenGL %s", glGetString(GL_VERSION));
    ImGui::Separator();
    ImGui::Text("Linksklick + Drag  = Drehen");
    ImGui::Text("Mittelklick + Drag = Pan");
    ImGui::Text("Scroll             = Zoom");
    ImGui::End();
    l_ui.endFrame();

    l_window.swapBuffers();
  }

  l_ui.shutdown();
  return 0;
}
