#pragma once
#include <ecs/systems/System.h>

struct GLFWwindow;

class Renderer;

namespace ecs
{
  class DebugSystem : public ecs::System
  {
  public:
    DebugSystem(ecs::Scene* scene, EventBus* eventBus, GLFWwindow* window, Renderer* renderer);
    ~DebugSystem();

    void Update(double dt) override;

  private:
    Renderer* _renderer;
    GLFWwindow* _window;

    int _fakeLag_ms = 0;
  };
}