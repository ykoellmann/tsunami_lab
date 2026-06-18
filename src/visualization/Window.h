#ifndef TSUNAMI_LAB_VISUALIZATION_WINDOW_H
#define TSUNAMI_LAB_VISUALIZATION_WINDOW_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace tsunami_lab {
namespace visualization {

class Window {
public:
  Window( int i_width, int i_height, const char* i_title );
  ~Window();

  bool        shouldClose() const { return glfwWindowShouldClose( m_window ); }
  void        pollEvents()        { glfwPollEvents(); }
  void        swapBuffers()       { glfwSwapBuffers( m_window ); }
  GLFWwindow* handle()            { return m_window; }
  void        getSize( int& o_w, int& o_h ) const { glfwGetFramebufferSize( m_window, &o_w, &o_h ); }

private:
  GLFWwindow* m_window = nullptr;
};

} // namespace visualization
} // namespace tsunami_lab

#endif
