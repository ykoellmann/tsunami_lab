#ifndef TSUNAMI_LAB_VISUALIZATION_WINDOW_H
#define TSUNAMI_LAB_VISUALIZATION_WINDOW_H

// glad must come before GLFW so that GLFW does not pull in system GL headers
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace tsunami_lab {
namespace visualization {

class Window {
public:
  Window(int i_width, int i_height, const char* i_title);
  ~Window();

  bool shouldClose() const { return glfwWindowShouldClose(m_window); }
  void pollEvents() { glfwPollEvents(); }
  void swapBuffers() { glfwSwapBuffers(m_window); }
  GLFWwindow* handle() { return m_window; }
  // Framebuffer size in pixels (use for glViewport). On HiDPI/Retina this is
  // larger than the window size by the content scale factor.
  void getSize(int& o_w, int& o_h) const {
    glfwGetFramebufferSize(m_window, &o_w, &o_h);
  }

  // Window size in logical points — matches glfwGetCursorPos coordinates, so
  // use this (not the framebuffer size) for any mouse → world unprojection.
  void getWindowSize(int& o_w, int& o_h) const {
    glfwGetWindowSize(m_window, &o_w, &o_h);
  }

private:
  GLFWwindow* m_window = nullptr;
};

} // namespace visualization
} // namespace tsunami_lab

#endif
