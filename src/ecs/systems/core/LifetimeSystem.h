#pragma once
#include <ecs/systems/System.h>

namespace ecs
{
  class LifetimeSystem : public System
  {
  public:
    LifetimeSystem(Scene* scene, EventBus* eventBus);

    void Update(double dt) override;

  private:
  };
}