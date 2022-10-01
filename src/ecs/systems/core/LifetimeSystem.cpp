#include "LifetimeSystem.h"
#include <ecs/components/core/Lifetime.h>
#include <ecs/Scene.h>
#include <entt/entity/registry.hpp>

namespace ecs
{
  LifetimeSystem::LifetimeSystem(Scene* scene, EventBus* eventBus)
    : System(scene, eventBus)
  {
  }

  void LifetimeSystem::Update(double dt)
  {
    for (auto&& [entity, deleteTicks] : _scene->Registry().view<DeleteInNTicks>().each())
    {
      if (--deleteTicks.ticks <= 0)
      {
        _scene->Registry().emplace<ecs::DeleteNextTick>(entity);
      }
    }

    auto deleteView = _scene->Registry().view<DeleteNextTick>();
    _scene->Registry().destroy(deleteView.begin(), deleteView.end());

    for (auto&& [entity, lifetime] : _scene->Registry().view<Lifetime>().each())
    {
      lifetime.microsecondsLeft -= static_cast<int>(dt * 1'000'000.0 + 0.5);
      if (lifetime.microsecondsLeft <= 0)
      {
        _scene->Registry().emplace<ecs::DeleteNextTick>(entity);
      }
    }
  }
}