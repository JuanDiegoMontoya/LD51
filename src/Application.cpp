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

#include "ecs/Entity.h"
#include "ecs/components/core/Sprite.h"
#include "ecs/components/core/Transform.h"
#include "ecs/components/DebugDraw.h"
#include "ecs/events/AddParticles.h"
#include <glm/packing.hpp>
#include <glm/integer.hpp>
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

glm::vec2 Hammersley(uint32_t i, uint32_t N)
{
  return glm::vec2(
    float(i) / float(N),
    float(glm::bitfieldReverse(i)) * 2.3283064365386963e-10
  );
}

void MakeParticles(EventBus* eventBus, uint32_t count, glm::vec2 position, float scaleColor, glm::vec4 baseColor = { 0.1f, 0.4f, 0.1f, 1.0f })
{
  std::vector<ecs::Particle> particles;
  for (uint32_t i = 0; i < count; i++)
  {
    auto xi = Hammersley(i + 1, count);
    float r = xi.x * 0.5f;
    float theta = xi.y * 6.283f;
    r *= .125;
    r = sqrtf(r);

    glm::vec2 pos = position + glm::vec2(r * cos(theta), r * sin(theta));
    pos = glm::clamp(pos, glm::vec2(-1), glm::vec2(1));
    glm::vec4 em = baseColor;
    em = em * scaleColor;
    ecs::Particle particle
    {
      .position = pos,
      .emissive = { glm::packHalf2x16({ em.r, em.g }), glm::packHalf2x16({ em.b, em.a }) },
      .velocity = glm::packHalf2x16({ 0, 0 }),
      .lifetime = 9999
    };
    particles.push_back(particle);
  }

  eventBus->Publish(ecs::AddParticles{ .particles = std::move(particles) });
}

void MakeStaticWall(ecs::Scene* scene, glm::vec2 position, glm::vec2 scale)
{
  auto ee = scene->CreateEntity("wall");
  auto& dbox = ee.AddComponent<ecs::DebugBox>();
  glm::vec4 emissive2 = { 0, 200, 0, 0 };
  dbox.color16f.x = glm::packHalf2x16({ emissive2.x, emissive2.y });
  dbox.color16f.y = glm::packHalf2x16({ emissive2.z, emissive2.w });
  dbox.scale = scale;
  dbox.translation = position;
  dbox.rotation = 0;
  dbox.active = false;
  ee.AddComponent<ecs::Flicker>().timeLeft = 3;
}

void MakeMovingWall(ecs::Scene* scene, glm::vec2 posA, glm::vec2 posB, double period, glm::vec2 scale)
{
  auto ee = scene->CreateEntity("wall");
  auto& dbox = ee.AddComponent<ecs::DebugBox>();
  glm::vec4 emissive2 = { 0, 200, 0, 0 };
  dbox.color16f.x = glm::packHalf2x16({ emissive2.x, emissive2.y });
  dbox.color16f.y = glm::packHalf2x16({ emissive2.z, emissive2.w });
  dbox.scale = scale;
  dbox.translation = { 0, 0 };
  dbox.rotation = 0;
  dbox.active = false;
  ee.AddComponent<ecs::Flicker>().timeLeft = 3;
  auto& move = ee.AddComponent<ecs::Movement>();
  move.period = period;
  move.posA = posA;
  move.posB = posB;
}

std::queue<Milestone> CreateDefaultMilestones(int startParticles, 
                                              EventBus* eventBus, 
                                              ecs::Scene* scene, 
                                              ecs::ParticleSystem* particleSystem)
{
  std::queue<Milestone> milestones;

  constexpr int interval = 2;

  milestones.push(Milestone
    {
      .time = 0,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, startParticles, { particleSystem->cursorX, particleSystem->cursorY}, 100);
      }
    });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, particleSystem->GetNumParticles(), { particleSystem->cursorX, particleSystem->cursorY}, 75);

        MakeStaticWall(scene, { 0, -1 }, { 2.1, .03 });
      }
    });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, particleSystem->GetNumParticles(), { particleSystem->cursorX, particleSystem->cursorY}, 50);

        MakeStaticWall(scene, { 0, 1 }, { 2.1, .03 });
      }
    });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, particleSystem->GetNumParticles(), { particleSystem->cursorX, particleSystem->cursorY}, 25, { .4, .2, .1, 0 });

        MakeMovingWall(scene, { -1.5, 0 }, { 1.5, 0 }, 10, { .25, .25 });
      }
    });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, particleSystem->GetNumParticles(), { particleSystem->cursorX, particleSystem->cursorY}, 12, { .4, .2, .1, 0 });

        MakeMovingWall(scene, { 0, -1.5 }, { 0, 1.5 }, 10, { .25, .25 });
      }
    });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, particleSystem->GetNumParticles(), { particleSystem->cursorX, particleSystem->cursorY}, 10, { .4, .2, .1, 0 });

        MakeStaticWall(scene, { -1, 0 }, { .03, 2.1 });
        MakeStaticWall(scene, { 1, 0 }, { .03, 2.1});
      }
    });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, particleSystem->GetNumParticles(), { particleSystem->cursorX, particleSystem->cursorY}, 10, { .1, .2, .4, 0 });

        MakeMovingWall(scene, { -.75, 0 }, { -1.11, 0 }, 10, { .4, 2.1 });
        MakeMovingWall(scene, { 1.11, 0 }, { .75, 0 }, 10, { .4, 2.1 });
      }
    });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, particleSystem->GetNumParticles(), { particleSystem->cursorX, particleSystem->cursorY}, 10, { .1, .2, .4, 0 });

        MakeMovingWall(scene, { 0, -.75 }, { 0, -1.11 }, 10, { 2.1, .4 });
        MakeMovingWall(scene, { 0, 1.11 }, { 0, .75 }, 10, { 2.1, .4 });
      }
    });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, particleSystem->GetNumParticles(), { particleSystem->cursorX, particleSystem->cursorY}, 10, { .1, .2, .4, 0 });

        MakeMovingWall(scene, { -.25, -.25 }, { .25, -.25 }, 8, { .125, .125 });
        MakeMovingWall(scene, { -.25, -.25 }, { -.25, .25 }, 8, { .125, .125 });
        MakeMovingWall(scene, { .25, .25 }, { -.25, .25 }, 8, { .125, .125 });
        MakeMovingWall(scene, { .25, .25 }, { .25, -.25 }, 8, { .125, .125 });
      }
    });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, particleSystem->GetNumParticles(), { particleSystem->cursorX, particleSystem->cursorY}, 10, { .5, .1, .5, 0 });

        MakeStaticWall(scene, { 0, 0 }, { .125, .125 });
      }
    });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, particleSystem->GetNumParticles(), { particleSystem->cursorX, particleSystem->cursorY}, 5, { .5, .1, .5, 0 });

        MakeMovingWall(scene, { 0, -2 }, { 0, 2 }, 10, { .125, 1 });
      }
    });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, particleSystem->GetNumParticles(), { particleSystem->cursorX, particleSystem->cursorY}, 5, { .5, .1, .5, 0 });

        MakeMovingWall(scene, { -1, 0 }, { 2, 0 }, 10, { 1, .125 });
      }
    });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
        MakeParticles(eventBus, particleSystem->GetNumParticles(), { particleSystem->cursorX, particleSystem->cursorY}, 5, { .2, .2, .2, 0 });
        scene->Registry().clear();
      } });

  milestones.push(Milestone
    {
      .time = interval,
      .spawnMilestone = [=]
      {
      // final milestone
      }
    });

  return milestones;
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
  glfwWindowHint(GLFW_MAXIMIZED, false);
  glfwWindowHint(GLFW_DECORATED, true);
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
  bool screenshotMode = false;
  GameState gameState = GameState::MENU;
  int startParticles = 1000;
  auto milestoneTracker = MilestoneTracker();

  struct Pause {};
  struct ScreenshotMode {};
  _input->AddActionBinding<Pause>(input::ActionInput{ .type{ input::Button::KEY_ESCAPE} });
  _input->AddActionBinding<ScreenshotMode>(input::ActionInput{ .type{ input::Button::KEY_F1} });

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

    void ScreenshotModeHandler(ScreenshotMode&)
    {
      screenshotMode = !screenshotMode;
    }

    GameState& gameState;
    bool& screenshotMode;
  };

  Handler handler{gameState, screenshotMode};

  _eventBus->Subscribe(&handler, &Handler::PauseHandler);
  _eventBus->Subscribe(&handler, &Handler::ScreenshotModeHandler);

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
      ImGui::SetNextWindowSize(ImVec2(300, 185));
      ImGui::Begin("common", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration);

      if (ImGui::Button("Play Game", { -1, 0 }))
      {
        gameState = GameState::RUNNING;
        sandboxMode = false;
        particleSystem.Reset(true, startParticles << 13);
        milestoneTracker.Reset(CreateDefaultMilestones(startParticles, _eventBus, _scene, &particleSystem));
      }

      if (ImGui::Button("Play Sandbox", { -1, 0 }))
      {
        gameState = GameState::RUNNING;
        sandboxMode = true;
        particleSystem.Reset(true, startParticles << 13);
        milestoneTracker.Reset(CreateDefaultMilestones(startParticles, _eventBus, _scene, &particleSystem));
      }

      if (ImGui::Button("Quit Game", { -1, 0 }))
      {
        glfwSetWindowShouldClose(_window, true);
      }

      if (ImGui::TreeNode("Options"))
      {
        ImGui::PushItemWidth(100);
        ImGui::SliderInt("Initial Particles", &startParticles, 100, 4000);
        int simHz = static_cast<int>(1.0 / _simulationTick);
        ImGui::SliderInt("Simulation Hz", &simHz, 15, 240);
        _simulationTick = 1.0 / simHz;

        ImGui::TreePop();
      }

      if (ImGui::TreeNode("Controls"))
      {
        ImGui::Text("Escape: pauses game");
        ImGui::Text("Cursor: control flock");

        ImGui::TreePop();
      }

      if (ImGui::TreeNode("How to Play"))
      {
        ImGui::Text("Keep your flock alive.");

        ImGui::TreePop();
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

          if (milestoneTracker._milestones.empty())
          {
            gameState = GameState::END;
          }
        }

        gameTime += dt;
        particleSystem.Update(_simulationTick);

        simulationAccum -= _simulationTick;
        //simulationAccum = 0;
      }
      break;
    }
    case GameState::PAUSED:
    {
      ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
      ImGui::SetNextWindowSize(ImVec2(200, 60));
      ImGui::Begin("common", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration);

      if (ImGui::Button("Resume", { -1, 0 }))
      {
        gameState = GameState::RUNNING;
      }

      if (ImGui::Button("Exit to Menu", { -1, 0 }))
      {
        gameState = GameState::MENU;
        _scene->Registry().clear();
        particleSystem.Reset(false, startParticles << 13);
      }

      ImGui::End();
      break;
    }
    case GameState::END:
    {
      ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
      ImGui::SetNextWindowSize(ImVec2(300, 200));
      ImGui::Begin("end", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration);

      auto survived = particleSystem.GetNumParticles();
      if (survived == 0)
      {
        ImGui::Text("Your flock has died.");
        ImGui::Text("You had %d seconds to go!", int(120 - gameTime));
      }
      else
      {
        ImGui::Text("Your flock survived!");
        ImGui::Text("Well, %.3f%% of it did anyways.", 100.0 * double(survived) / (uint64_t(startParticles) << 12u));
      }

      if (ImGui::Button("Free Play"))
      {
        gameState = GameState::RUNNING;
        sandboxMode = true;
      }

      if (ImGui::Button("Main Menu"))
      {
        gameState = GameState::MENU;
      }

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
      ImGui::SetNextWindowSize(ImVec2(400, 170));
      ImGui::Begin("sandbox", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDecoration);

      ImGui::Text("Framerate: %.0fHz", 1.0 / dt);
      ImGui::SliderFloat("Magnetism", &particleSystem.magnetism, 0, 5.0f);
      ImGui::SliderFloat("Friction", &particleSystem.friction, 0, 1.0f);
      ImGui::SliderFloat("Accel. constant", &particleSystem.accelerationConstant, 0, 5);
      ImGui::SliderFloat("Accel. min dist", &particleSystem.accelerationMinDistance, 0.1f, 5);

      int simHz = static_cast<int>(1.0 / _simulationTick);
      ImGui::SliderInt("Simulation Hz", &simHz, 15, 240);
      _simulationTick = 1.0 / simHz;

      if (ImGui::Button("Double Particles"))
      {
        MakeParticles(_eventBus, particleSystem.GetNumParticles(), { particleSystem.cursorX, particleSystem.cursorY }, 10, { .4, .2, .1, 0 });
      }
      ImGui::SameLine();
      if (ImGui::Button("Reset"))
      {
        _scene->Registry().clear();
        particleSystem.Reset(false, startParticles << 13);
        milestoneTracker.Reset(CreateDefaultMilestones(startParticles, _eventBus, _scene, &particleSystem));
      }
      ImGui::SameLine();
      if (ImGui::Button("Clear Walls"))
      {
        _scene->Registry().clear();
        std::queue<Milestone> ms;
        ms.push(Milestone{ .time = 9999, .spawnMilestone = []() {} });
        milestoneTracker.Reset(ms);
      }

      ImGui::End();
    }
    
    renderingSystem.Update(dt);
    particleSystem.Draw();

    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!screenshotMode)
    {
      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
    ImGui::EndFrame();

    glfwSwapBuffers(_window);
  }
}