#ifndef TSUNAMI_LAB_VISUALIZATION_UI_H
#define TSUNAMI_LAB_VISUALIZATION_UI_H

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

namespace tsunami_lab {
namespace visualization {

class Ui {
public:
  void init( GLFWwindow* i_window ) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL( i_window, true );
    ImGui_ImplOpenGL3_Init( "#version 330 core" );
  }

  void beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
  }

  void endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );
  }

  void shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }
};

} // namespace visualization
} // namespace tsunami_lab

#endif
