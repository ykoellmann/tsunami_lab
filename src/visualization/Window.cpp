#include "Window.h"
#include <cstdio>
#include <cstdlib>

static void glfwErrorCallback( int i_error, const char* i_desc ) {
  std::fprintf( stderr, "GLFW error %d: %s\n", i_error, i_desc );
}

namespace tsunami_lab {
namespace visualization {

Window::Window( int i_width, int i_height, const char* i_title ) {
  glfwSetErrorCallback( glfwErrorCallback );

  if ( !glfwInit() ) {
    std::fprintf( stderr, "glfwInit() failed\n" );
    std::exit( 1 );
  }

  glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
  glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3 );
  glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
#ifdef __APPLE__
  glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
#endif

  m_window = glfwCreateWindow( i_width, i_height, i_title, nullptr, nullptr );
  if ( !m_window ) {
    std::fprintf( stderr, "glfwCreateWindow() failed\n" );
    glfwTerminate();
    std::exit( 1 );
  }

  glfwMakeContextCurrent( m_window );
  glfwSwapInterval( 1 );

  if ( !gladLoadGLLoader( (GLADloadproc)glfwGetProcAddress ) ) {
    std::fprintf( stderr, "gladLoadGLLoader() failed\n" );
    std::exit( 1 );
  }
}

Window::~Window() {
  if ( m_window ) glfwDestroyWindow( m_window );
  glfwTerminate();
}

} // namespace visualization
} // namespace tsunami_lab
