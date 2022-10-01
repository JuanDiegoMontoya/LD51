#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace ecs
{
  struct DebugLine
  {
    glm::vec2 p0 = { 0, 0 };
    glm::u8vec4 color0 = { 0, 0, 0, 0 };
    glm::vec2 p1 = { 0, 0 };
    glm::u8vec4 color1 = { 0, 0, 0, 0 };
  };

  struct DebugBox
  {
    glm::vec2 translation = { 0, 0 };
    float rotation = 0;
    glm::vec2 scale = { 0, 0 };
    glm::u8vec4 color = { 0, 0, 0, 0 };
  };

  struct DebugCircle
  {
    glm::vec2 translation = { 0, 0 };
    float radius = 1;
    glm::u8vec4 color = { 0, 0, 0, 0 };
  };
}