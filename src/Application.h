#pragma once
#include <string>

class EventBus;
struct GLFWwindow;

namespace ecs
{
  class Scene;
}

namespace input
{
  class InputManager;
}

class Application
{
public:
  Application(std::string title, ecs::Scene* scene, EventBus* eventBus);
  ~Application();

  Application(const Application&) = delete;
  Application(Application&&) = delete;
  Application& operator=(const Application&) = delete;
  Application& operator=(Application&&) = delete;

  void Run();

private:

  std::string _title;
  ecs::Scene* _scene;
  EventBus* _eventBus;
  GLFWwindow* _window;
  input::InputManager* _input;
  double _simulationTick = 1.0 / 60.0;
};