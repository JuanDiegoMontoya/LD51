#include "Application.h"
#include "Renderer.h"
#include "Input.h"
#include "utils/EventBus.h"
#include "utils/Timer.h"
#include "ecs/Scene.h"
#include "ecs/systems/core/LifetimeSystem.h"
#include "ecs/systems/core/CollisionSystem.h"
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
  _window = glfwCreateWindow(1280,
    720,
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

  int x{};
  int y{};
  int nc{};
  stbi_set_flip_vertically_on_load(true);
  auto* pixels = stbi_load("assets/textures/doge.png", &x, &y, &nc, 4);
  auto texture = Fwog::Texture(Fwog::CreateTexture2D({ static_cast<uint32_t>(x), static_cast<uint32_t>(y) }, Fwog::Format::R8G8B8A8_SRGB));
  texture.SubImage({ .dimension = Fwog::UploadDimension::TWO,
                   .size = texture.Extent(),
                   .format = Fwog::UploadFormat::RGBA,
                   .type = Fwog::UploadType::UBYTE,
                   .pixels = pixels });

  ecs::Entity entity = _scene->CreateEntity("hello");
  auto& transform = entity.AddComponent<ecs::Transform>();
  transform.scale = { 10, 10 };
  transform.translation = { 0, -3 };
  //transform.rotation = 3.14f / 4.0f;
  transform.rotation = 0;
  auto& sprite = entity.AddComponent<ecs::Sprite>();
  sprite.texture = &texture;
  sprite.tint = { 0, 255, 255, 255 };
  entity.AddComponent<ecs::Collider>();

  auto e1 = _scene->CreateEntity("other");
  e1.AddComponent<ecs::Transform>().translation = { 8, 0 };
  e1.AddComponent<ecs::Sprite>().texture = &texture;
  e1.AddComponent<ecs::Collider>();
  auto& line = e1.AddComponent<ecs::DebugLine>();
  line.p0 = { -5, 0 };
  line.p1 = { 5, 5 };
  line.color0 = { 255, 0, 0, 255 };
  line.color1 = { 0, 255, 0, 255 };
  auto& box = e1.AddComponent<ecs::DebugBox>();
  box.translation = { 5, -5 };
  box.scale = { 1, 1 };
  box.rotation = 0;
  box.color = { 0, 0, 255, 255 };
  auto& circle = e1.AddComponent<ecs::DebugCircle>();
  circle.translation = { 10, 0 };
  circle.radius = 5;
  circle.color = { 127, 255, 55, 255 };

  _eventBus->Publish(ecs::AddSprite{ .path = "assets/textures/spaceship.png" });
    
  struct Test
  {
    void handle(ecs::Collision& c)
    {
      printf("collision: %d, %d\n", entt::entity(c.entity0), entt::entity(c.entity1));
    }
  };

  Test test;
  _eventBus->Subscribe(&test, &Test::handle);

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
    while (simulationAccum > _simulationTick)
    {
      _input->PollEvents(_simulationTick);
      lifetimeSystem.Update(_simulationTick);
      collisionSystem.Update(_simulationTick);

      simulationAccum -= _simulationTick;
    }

    if (glfwGetKey(_window, GLFW_KEY_ESCAPE))
    {
      glfwSetWindowShouldClose(_window, true);
    }

    transform.rotation += float(3.14 * dt);
    //transform.scale *= .999f;
    //transform.translation.x += float(1.0 * dt);

    //line.p1.y += float(.5 * dt);
    //box.scale.y *= float(1.0f + dt);
    box.rotation += .01f;

    renderingSystem.Update(dt);
    debugSystem.Update(dt);

    glfwSwapBuffers(_window);
  }
}