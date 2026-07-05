#include "Window.h"
#include <cstdio>
#include <cstdlib>

static void glfwErrorCallback(int i_error, const char* i_desc) {
  std::fprintf(stderr, "GLFW error %d: %s\n", i_error, i_desc);
}

namespace tsunami_lab {
namespace visualization {

Window::Window(int i_width, int i_height, const char* i_title) {
  glfwSetErrorCallback(glfwErrorCallback);
  // Force X11/GLX on Linux; prevents GLFW 3.4 from attempting EGL first
  // which can fail even when a valid X11 display is available.
#if defined(__linux__)
  glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

  if (!glfwInit()) {
    std::fprintf(stderr, "glfwInit() failed\n");
    std::exit(1);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
  // 8x MSAA: without it the dense terrain meshes shimmer — brightly lit
  // facets flicker as white speckles/streaks depending on which triangle wins
  // each pixel. Affordable because the terrain/water meshes pick an LOD that
  // keeps triangles at roughly pixel size (see Lod.h); the driver falls back
  // to the closest supported sample count if 8 is unavailable.
  glfwWindowHint(GLFW_SAMPLES, 8);

  m_window = glfwCreateWindow(i_width, i_height, i_title, nullptr, nullptr);
  if (!m_window) {
    std::fprintf(stderr, "glfwCreateWindow() failed\n");
    glfwTerminate();
    std::exit(1);
  }

  glfwMakeContextCurrent(m_window);
  glfwSwapInterval(1);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::fprintf(stderr, "gladLoadGLLoader() failed\n");
    std::exit(1);
  }
}

Window::~Window() {
  if (m_window)
    glfwDestroyWindow(m_window);
  glfwTerminate();
}

} // namespace visualization
} // namespace tsunami_lab
