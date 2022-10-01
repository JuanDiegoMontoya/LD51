#pragma once
#include "Scene.h"
#include "GAssert.h"
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <utility>
#include <type_traits>

namespace ecs
{
  class Entity
  {
  public:
    Entity() = default;
    Entity(entt::entity entityHandle, Scene* scene)
      : _scene(scene),
        _entityHandle(entityHandle)
    {
    }

    Entity(const Entity&) noexcept = default;

    [[nodiscard]] bool operator==(const Entity&) const = default;
    [[nodiscard]] bool operator!=(const Entity&) const = default;

    operator bool() const { return _entityHandle != entt::null; }
    operator entt::entity() { return _entityHandle; }

    void Destroy();

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args)
    {
      G_ASSERT_MSG(!HasComponents<T>(), "Entity already has component");
      return _scene->Registry().emplace<T>(_entityHandle, std::forward<Args>(args)...);
    }

    // leverage entt's empty type optimization
    template<typename T>
    requires std::is_empty_v<T>
    void AddComponent()
    {
      _scene->Registry().emplace<T>(_entityHandle);
    }
    
    template<typename T, typename... Args>
    T& GetOrAddComponent(Args&&... args)
    {
      return _scene->Registry().get_or_emplace<T>(_entityHandle, std::forward<Args>(args)...);
    }

    template<typename T>
    [[nodiscard]] T& GetComponent()
    {
      G_ASSERT_MSG(HasComponent<T>(), "Entity missing component");
      return _scene->Registry().get<T>(_entityHandle);
    }

    template<typename T>
    [[nodiscard]] T& GetComponent() const
    {
      G_ASSERT_MSG(HasComponent<T>(), "Entity missing component");
      return _scene->Registry().get<T>(_entityHandle);
    }

    template<typename... Ts>
    [[nodiscard]] bool HasComponents() const
    {
      return _scene->Registry().all_of<Ts...>(_entityHandle);
    }

  private:
    Scene* _scene = nullptr;
    entt::entity _entityHandle = entt::null;
  };
}