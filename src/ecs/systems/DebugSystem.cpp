#include "DebugSystem.h"
#include "Renderer.h"
#include "ecs/Scene.h"
#include "ecs/components/DebugDraw.h"
#include "utils/Timer.h"
#include <entt/entity/registry.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glad/gl.h>

namespace ecs
{
  DebugSystem::DebugSystem(ecs::Scene* scene, EventBus* eventBus, GLFWwindow* window, Renderer* renderer)
    : System(scene, eventBus),
      _renderer(renderer),
      _window(window)
  {
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(_window, false);
    ImGui_ImplOpenGL3_Init("#version 450 core");
    ImGui::StyleColorsDark();
  }

  DebugSystem::~DebugSystem()
  {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }

  void DebugSystem::Update(double dt)
  {
    // draw debug primitives
    auto viewLine = _scene->Registry().view<ecs::DebugLine>();
    auto viewBox = _scene->Registry().view<ecs::DebugBox>();
    auto viewCircle = _scene->Registry().view<ecs::DebugCircle>();
    std::vector<ecs::DebugLine> lines;
    std::vector<ecs::DebugBox> boxes;
    std::vector<ecs::DebugCircle> circles;
    lines.reserve(viewLine.size());
    boxes.reserve(viewBox.size());
    circles.reserve(viewCircle.size());
    
    for (auto&& [_, line] : viewLine.each())
    {
      lines.push_back(line);
    }
    for (auto&& [_, box] : viewBox.each())
    {
      boxes.push_back(box);
    }
    for (auto&& [_, circle] : viewCircle.each())
    {
      circles.push_back(circle);
    }

    _renderer->DrawLines(lines);
    _renderer->DrawBoxes(boxes);
    _renderer->DrawCircles(circles);

    // draw debug GUI
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("hello");
    ImGui::Text("Framerate: %.0fHz", 1.0 / dt);
    ImGui::Text("Entities: %d", _scene->Registry().alive());
    if (ImGui::Button("Nuke Entities"))
    {
      _scene->Registry().clear();
    }
    ImGui::SliderInt("Fake Lag (ms)", &_fakeLag_ms, 0, 100);
    if (_fakeLag_ms > 0)
    {
      Timer timer;
      while (int(timer.Elapsed_ms()) < _fakeLag_ms)
      {
      }
    }

    ImGui::End();

    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();
  }
}