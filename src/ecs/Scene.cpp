#include "Scene.h"
#include "ecs/Entity.h"
#include "ecs/components/core/Tag.h"
#include "utils/EventBus.h"
#include <entt/entity/registry.hpp>

namespace ecs
{
  Scene::Scene(EventBus* eventBus)
    : _eventBus(eventBus)
  {
    //_registry = new entt::registry;
    _registry = std::make_unique<entt::registry>();
  }

  Scene::~Scene()
  {
    //delete _registry;
  }

  Entity Scene::CreateEntity(std::string_view name)
  {
    auto entity = Entity(_registry->create(), this);
    auto& tag = entity.AddComponent<ecs::Tag>();
    tag.tag = name.empty() ? "Entity" : name;
    return entity;
  }

  Entity Scene::FindEntity(std::string_view name)
  {
    auto view = _registry->view<ecs::Tag>();

    for (auto entity : view)
    {
      const auto& tag = view.get<ecs::Tag>(entity);
      
      if (tag.tag == name)
      {
        return Entity(entity, this);
      }
    }

    return Entity{};
  }

  entt::registry& ecs::Scene::Registry()
  {
    G_ASSERT(_registry);
    return *_registry;
  }
}