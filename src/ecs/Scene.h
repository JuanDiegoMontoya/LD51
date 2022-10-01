#pragma once
#include <entt/entity/fwd.hpp>
#include <string_view>
#include <memory>

class EventBus;

namespace ecs
{
  class Entity;

  class Scene
  {
  public:
    Scene(EventBus* eventBus);
    ~Scene();

    Scene(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene& operator=(Scene&&) = delete;

    Entity CreateEntity(std::string_view name = "");

    // Returns an entity with given name.
    // If no entity with the name is found, a null entity is returned.
    // Time complexity: O(n)
    Entity FindEntity(std::string_view name);

    entt::registry& Registry();

  private:
    EventBus* _eventBus;
    std::unique_ptr<entt::registry> _registry;
  };
}