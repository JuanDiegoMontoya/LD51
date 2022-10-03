#include "Application.h"
#include "Renderer.h"
#include "Input.h"
#include "utils/EventBus.h"
#include "utils/Timer.h"
#include "ecs/Scene.h"
#include "ecs/systems/core/LifetimeSystem.h"
#include "ecs/systems/game/ParticleSystem.h"
#include "ecs/systems/RenderingSystem.h"
#include "ecs/systems/DebugSystem.h"
#include <Fwog/Texture.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <utility>
#include <stdexcept>
#include <string>
#include <queue>
#include <functional>

// temporary includes
#include "ecs/Entity.h"
#include "ecs/components/core/Sprite.h"
#include "ecs/components/core/Transform.h"
#include "ecs/components/DebugDraw.h"
#include "ecs/events/AddParticles.h"
#include <glm/packing.hpp>
#include <stb_image.h>

struct Milestone
{
  float time; // how long since the last milestone was reached
  std::function<void(void)> spawnMilestone;
};

struct MilestoneTracker
{

  bool Update(double dt)
  {
    _accumulator += dt;

    if (_milestones.empty())
    {
      return false;
    }

    Milestone milestone = _milestones.front();
    if (_accumulator >= milestone.time)
    {
      _accumulator = 0;
      _milestones.pop();
      milestone.spawnMilestone();
      //_eventBus->Publish(ecs::AddParticles{ .particles = std::move(milestone.particles) });

      //for (const auto& box : milestone.obstacles)
      //{
      //  _scene->CreateEntity("wall").AddComponent<ecs::DebugBox>() = box;
      //}

      return true;
    }

    return false;
  }

  void Reset(std::queue<Milestone> milestones)
  {
    _accumulator = 0;
    _milestones = milestones;
  }

  double _accumulator = 0;
  std::queue<Milestone> _milestones;
};

std::queue<Milestone> CreateDefaultMilestones(int startingParticles, EventBus* eventBus, ecs::Scene* scene)
{
  std::queue<Milestone> milestones;

  Milestone milestone0;
  milestone0.time = 0;
  milestone0.spawnMilestone = [eventBus, scene, startingParticles]
  {
    std::vector<ecs::Particle> particles;
    for (int x = 0; x < 5000; x++)
    {
      for (int y = 0; y < 1000; y++)
      {
        glm::vec4 em = { 0.1f * (y / 500.0f), 0.4f * (x / 2500.0f), 0.1f, 1.0f };
        ecs::Particle particle
        {
          .position = { x / 5000.0 - .5, y / 1000.0 - .5 },
          .emissive = { glm::packHalf2x16({ em.r, em.g }), glm::packHalf2x16({ em.b, em.a }) },
          .velocity = glm::packHalf2x16({ 0, 0 }),
          .lifetime = 9999
        };
        particles.push_back(particle);
      }
    }

    auto e = scene->CreateEntity("wall");
    auto& dbox = e.AddComponent<ecs::DebugBox>();
    glm::vec4 emissive = { 200, 0, 0, 0 };
    dbox.color16f.x = glm::packHalf2x16({ emissive.x, emissive.y });
    dbox.color16f.y = glm::packHalf2x16({ emissive.z, emissive.w });
    dbox.scale = { .25, .25 };
    dbox.translation = { .5, 0 };
    dbox.rotation = 0;

    eventBus->Publish(ecs::AddParticles{ .particles = particles });
  };
  milestones.push(milestone0);

  return milestones;
}

void SpawnParticles()
{
  //std::vector<ecs::Particle> particles;
  //for (int x = 0; x < 5000; x++)
  //{
  //  for (int y = 0; y < 1000; y++)
  //  {
  //    glm::vec4 em = { 0.1f * (y / 500.0f), 0.4f * (x / 2500.0f), 0.1f, 1.0f };
  //    ecs::Particle particle
  //    {
  //      .position = { x / 5000.0 - .5, y / 1000.0 - .5 },
  //      .emissive = { glm::packHalf2x16({ em.r, em.g }), glm::packHalf2x16({ em.b, em.a }) },
  //      .velocity = glm::packHalf2x16({ 0, 0 }),
  //      .lifetime = 9999
  //    };
  //    particles.push_back(particle);
  //  }
  //}

  //auto e = _scene->CreateEntity("wall");
  //auto& dbox = e.AddComponent<ecs::DebugBox>();
  //glm::vec4 emissive = { 200, 0, 0, 0 };
  //dbox.color16f.x = glm::packHalf2x16({ emissive.x, emissive.y });
  //dbox.color16f.y = glm::packHalf2x16({ emissive.z, emissive.w });
  //dbox.scale = { 2.1, .05 };
  //dbox.translation = { 0, -1 };
  //dbox.rotation = 0;

  //_eventBus->Publish(ecs::AddParticles{ .particles = particles });
}

Application::Application(std::string title, ecs::Scene* scene, EventBus* eventBus)
  : _title(std::move(title)),
    _scene(scene),
    _eventBus(eventBus)
{
  // init everything
  if (!glfwInit())
  {
    throw std::runtime_error("Failed to initialize GLFW");
  }

  glfwSetErrorCallback([](int, const char* desc)
    {
      throw std::runtime_error(std::string("GLFW error: ") + desc + '\n');
    });

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_MAXIMIZED, false); // TODO: load from config
  glfwWindowHint(GLFW_DECORATED, true); // TODO: load from config
  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

#ifndef NDEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#else
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_FALSE);
#endif

  // TODO: load parameters from config
  _window = glfwCreateWindow(800,
                             800,
                             _title.c_str(),
                             nullptr,
                             nullptr);

  if (!_window)
  {
    throw std::runtime_error("Failed to create _window");
  }

  glfwMakeContextCurrent(_window);
  glfwSwapInterval(1);

  _input = new input::InputManager(_window, _eventBus);
}

Application::~Application()
{
  delete _input;
  glfwTerminate();
}

void Application::Run()
{
  auto renderer = Renderer(_window);
  auto renderingSystem = ecs::RenderingSystem(_scene, _eventBus, _window, &renderer);
  auto debugSystem = ecs::DebugSystem(_scene, _eventBus, _window, &renderer);
  auto particleSystem = ecs::ParticleSystem(_scene, _eventBus, &renderer);

  enum class GameState
  {
    MENU,
    RUNNING,
    PAUSED,
    END,
  };

  ///////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////// GAMEPLAY VARS /////////////////////////
  ///////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////////////
  double gameTime = 0;
  bool sandboxMode = false;
  GameState gameState = GameState::MENU;
  int startParticles = 1000;
  auto milestoneTracker = MilestoneTracker();

  struct Pause {};
  _input->AddActionBinding<Pause>(input::ActionInput{ .type{ input::Button::KEY_ESCAPE} });

  struct Handler
  {
    void PauseHandler(Pause&)
    {
      switch (gameState)
      {
      case GameState::RUNNING: gameState = GameState::PAUSED; break;
      case GameState::PAUSED: gameState = GameState::RUNNING; break;
      default:
        break;
      }
    }

    GameState& gameState;
  };

  Handler handler{gameState};

  _eventBus->Subscribe(&handler, &Handler::PauseHandler);

  Timer timer;
  double simulationAccum = 0;
  double inputAccum = 0;
  while (!glfwWindowShouldClose(_window))
  {
    double dt = timer.Elapsed_s();
    timer.Reset();

    inputAccum += dt;
    while (inputAccum > _simulationTick)
    {
      _input->PollEvents(_simulationTick);
      inputAccum -= _simulationTick;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // If dt is above a threshold, then the application was probably paused for debugging.
    // We don't want the simulation to suddenly advance super far, so let's clamp and log the lag spike.
    // If it was a legitimate lag spike, then we want to log it anyways.
    if (dt > 1.0)
    {
      dt = _simulationTick;
      // log something
    }

    switch (gameState)
    {
    case GameState::MENU:
    {
      ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
      ImGui::SetNextWindowSize(ImVec2(200, 200));
      ImGui::Begin("common", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration);

      if (ImGui::Button("Play Game", { -1, 0 }))
      {
        gameState = GameState::RUNNING;
        sandboxMode = false;
        particleSystem.Reset(true, startParticles << 12);
        milestoneTracker.Reset(CreateDefaultMilestones(startParticles, _eventBus, _scene));
      }

      if (ImGui::Button("Play Sandbox", { -1, 0 }))
      {
        gameState = GameState::RUNNING;
        sandboxMode = true;
        particleSystem.Reset(true, startParticles << 12);
        milestoneTracker.Reset(CreateDefaultMilestones(startParticles, _eventBus, _scene));
      }

      if (ImGui::TreeNode("Options"))
      {
        ImGui::SliderInt("Initial Particles", &startParticles, 100, 4000);
        int simHz = static_cast<int>(1.0 / _simulationTick);
        ImGui::SliderInt("Simulation Hz", &simHz, 15, 240);
        _simulationTick = 1.0 / simHz;

        ImGui::TreePop();
      }

      if (ImGui::Button("Quit Game", { -1, 0 }))
      {
        glfwSetWindowShouldClose(_window, true);
      }

      ImGui::End();
      break;
    }
    case GameState::RUNNING:
    {
      simulationAccum += dt;
      while (simulationAccum > _simulationTick)
      {
        // only check particle count each milestone to avoid lag spam
        if (milestoneTracker.Update(_simulationTick))
        {
          if (particleSystem.GetNumParticles() == 0)
          {
            gameState = GameState::END;
          }
        }

        particleSystem.Update(_simulationTick);
        gameTime += _simulationTick;

        simulationAccum -= _simulationTick;
        //simulationAccum = 0;
      }
      break;
    }
    case GameState::PAUSED:
    {
      ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
      ImGui::SetNextWindowSize(ImVec2(200, 200));
      ImGui::Begin("common", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration);

      if (ImGui::Button("Resume", { -1, 0 }))
      {
        gameState = GameState::RUNNING;
      }

      if (ImGui::Button("Exit to Menu", { -1, 0 }))
      {
        gameState = GameState::MENU;
        _scene->Registry().clear();
        particleSystem.Reset(false, 100);
      }

      ImGui::End();
      break;
    }
    case GameState::END:
    {
      ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
      ImGui::SetNextWindowSize(ImVec2(200, 200));
      ImGui::Begin("sandbox", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration);

      ImGui::Text("Framerate: %.0fHz", 1.0 / dt);

      ImGui::End();

      break;
    }
    default:
      G_UNREACHABLE;
      break;
    }

    // show sandbox menu
    if ((gameState == GameState::RUNNING || gameState == GameState::PAUSED) && sandboxMode == true)
    {
      ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
      ImGui::SetNextWindowSize(ImVec2(400, 200));
      ImGui::Begin("sandbox", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration);

      ImGui::Text("Framerate: %.0fHz", 1.0 / dt);
      ImGui::SliderFloat("Magnetism", &particleSystem.magnetism, 0, 5.0f);
      ImGui::SliderFloat("Friction", &particleSystem.friction, 0, 1.0f);
      ImGui::SliderFloat("Accel. constant", &particleSystem.accelerationConstant, 0, 5);
      ImGui::SliderFloat("Accel. min dist", &particleSystem.accelerationMinDistance, 0.1f, 5);

      int simHz = static_cast<int>(1.0 / _simulationTick);
      ImGui::SliderInt("Simulation Hz", &simHz, 15, 240);
      _simulationTick = 1.0 / simHz;

      if (ImGui::Button("Reset"))
      {
        particleSystem.Reset(false, startParticles << 12);
        milestoneTracker.Reset(CreateDefaultMilestones(startParticles, _eventBus, _scene));
      }

      ImGui::End();
    }
    
    renderingSystem.Update(dt);
    particleSystem.Draw();

    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui::EndFrame();

    glfwSwapBuffers(_window);
  }
}