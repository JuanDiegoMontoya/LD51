#include "Application.h"
#include "Renderer.h"
#include "Input.h"
#include "utils/EventBus.h"
#include "utils/Timer.h"
#include "ecs/Scene.h"
#include "ecs/systems/core/LifetimeSystem.h"
#include "ecs/systems/core/CollisionSystem.h"
#include "ecs/systems/game/ParticleSystem.h"
#include "ecs/systems/RenderingSystem.h"
#include "ecs/systems/DebugSystem.h"
#include <Fwog/Texture.h>
#include <GLFW/glfw3.h>
#include <utility>
#include <stdexcept>
#include <string>

// temporary includes
#include "ecs/Entity.h"
#include "ecs/components/core/Sprite.h"
#include "ecs/components/core/Transform.h"
#include "ecs/components/core/Collider.h"
#include "ecs/events/Collision.h"
#include "ecs/components/DebugDraw.h"
#include "ecs/events/AddParticles.h"
#include <glm/packing.hpp>
#include <stb_image.h>

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
  auto lifetimeSystem = ecs::LifetimeSystem(_scene, _eventBus);
  auto collisionSystem = ecs::CollisionSystem(_scene, _eventBus);
  auto renderingSystem = ecs::RenderingSystem(_scene, _eventBus, _window, &renderer);
  auto debugSystem = ecs::DebugSystem(_scene, _eventBus, _window, &renderer);
  auto particleSystem = ecs::ParticleSystem(_scene, _eventBus, &renderer);

  // TODO: temp
  std::vector<ecs::Particle> particles;
  for (int x = 0; x < 5000; x++)
  {
    for (int y = 0; y < 1000; y++)
    {
      glm::vec4 em = { 1.0f, 2.0f, 1.0f, 1.0f };
      ecs::Particle particle
      {
        .position = { x / 5000.0 - .5, y / 1000.0 - .5 },
        .emissive = { glm::packHalf2x16({ em.r, em.g }), glm::packHalf2x16({ em.b, em.a }) },
        .velocity = glm::packHalf2x16({ 0, 0 }),
        .flags = 1
      };
      particles.push_back(particle);
    }
  }

  _eventBus->Publish(ecs::AddParticles{ .particles = particles });

  Timer timer;
  double simulationAccum = 0;
  while (!glfwWindowShouldClose(_window))
  {
    double dt = timer.Elapsed_s();
    timer.Reset();

    // If dt is above a threshold, then the application was probably paused for debugging.
    // We don't want the simulation to suddenly advance super far, so let's clamp and log the lag spike.
    // If it was a legitimate lag spike, then we want to log it anyways.
    if (dt > 1.0)
    {
      dt = 1.0;
      // log something
    }

    // TODO: Add something to prevent the death spiral
    simulationAccum += dt;
    if (simulationAccum > _simulationTick)
    {
      _input->PollEvents(_simulationTick);
      lifetimeSystem.Update(_simulationTick);
      collisionSystem.Update(_simulationTick);
      particleSystem.Update(_simulationTick);

      //simulationAccum -= _simulationTick;
      simulationAccum = 0;
    }

    if (glfwGetKey(_window, GLFW_KEY_ESCAPE))
    {
      glfwSetWindowShouldClose(_window, true);
    }

    renderingSystem.Update(dt);
    particleSystem.Draw();

    debugSystem.Update(dt);

    glfwSwapBuffers(_window);
  }
}