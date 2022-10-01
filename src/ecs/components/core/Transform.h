#pragma once
#include <glm/vec2.hpp>

namespace ecs
{
  struct Transform
  {
    glm::vec2 translation = { 0, 0 };
    float rotation = 0; // radians
    glm::vec2 scale = { 1, 1 };
  };
}