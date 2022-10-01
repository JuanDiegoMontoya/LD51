#include "CollisionSystem.h"
#include <ecs/components/core/Collider.h>
#include <ecs/components/core/Transform.h>
#include <ecs/events/Collision.h>
#include <utils/EventBus.h>
#include <entt/entity/registry.hpp>
#include <glm/vector_relational.hpp>
#include <cmath>

namespace ecs
{
  CollisionSystem::CollisionSystem(Scene* scene, EventBus* eventBus)
    : System(scene, eventBus)
  {
  }

  void CollisionSystem::Update([[maybe_unused]] double dt)
  {
    // This crappy O(n^2) algorithm will do for now.
    auto view = _scene->Registry().view<ecs::Transform, ecs::Collider>();
    for (auto it0 = view.begin(); it0 != view.end();)
    {
      auto e0 = *it0;
      auto&& [transform0, collider0] = view.get(*it0);
      
      // Instead of doing a complicated OBB test, just expand the collider to fit the AABB of the rotated box.
      // https://stackoverflow.com/a/33867165
      glm::vec2 e0size = transform0.scale * .5f * collider0.scale;
      glm::vec2 re0size = e0size;
      if (transform0.rotation != 0)
      {
        re0size.x = e0size.x * std::abs(std::cos(transform0.rotation)) + e0size.y * std::abs(std::sin(transform0.rotation));
        re0size.y = e0size.x * std::abs(std::sin(transform0.rotation)) + e0size.y * std::abs(std::cos(transform0.rotation));
      }
      glm::vec2 e0max = transform0.translation + re0size;
      glm::vec2 e0min = transform0.translation - re0size;

      for (auto it1 = ++it0; it1 != view.end(); it1++)
      {
        auto&& [transform1, collider1] = view.get(*it1);

        glm::vec2 e1size = transform1.scale * .5f * collider1.scale;
        glm::vec2 re1size = e1size;
        if (transform1.rotation != 0)
        {
          re1size.x = e1size.x * std::abs(std::cos(transform1.rotation)) + e1size.y * std::abs(std::sin(transform1.rotation));
          re1size.y = e1size.x * std::abs(std::sin(transform1.rotation)) + e1size.y * std::abs(std::cos(transform1.rotation));
        }
        glm::vec2 e1max = transform1.translation + re1size;
        glm::vec2 e1min = transform1.translation - re1size;
        
        if (glm::all(glm::greaterThan(e0max, e1min)) && glm::all(glm::lessThan(e0min, e1max)))
        {
          _eventBus->Publish(ecs::Collision{ Entity(e0, _scene), Entity(*it1, _scene)});
        }
      }
    }
  }
}