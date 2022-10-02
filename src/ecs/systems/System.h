#pragma once
#include <entt/fwd.hpp>

class EventBus;

namespace ecs
{
  class Scene;

  // abstract class that all systems should derive from
  class System
  {
  public:
    System(Scene* scene, EventBus* eventBus);
    virtual ~System();

    System(const System&) = delete;
    System(System&&) = delete;
    System& operator=(const System&) = delete;
    System& operator=(System&&) = delete;

    virtual void Update(double dt) = 0;
    virtual void Draw() {}

  protected:
    Scene* _scene;
    EventBus* _eventBus;
  };
}