#pragma once
#include "ecs/systems/System.h"
#include <Fwog/Texture.h>
#include <memory>

class Renderer;

struct GLFWwindow;

namespace ecs
{
  class RenderingSystem : public ecs::System
  {
  public:
    RenderingSystem(ecs::Scene* scene, EventBus* eventBus, GLFWwindow* window, Renderer* renderer);

    void Update(double dt) override;

  private:
    Renderer* _renderer;
    GLFWwindow* _window;
    std::unique_ptr<Fwog::Texture> _backgroundTexture;
  };
}