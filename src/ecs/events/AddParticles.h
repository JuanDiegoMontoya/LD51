#pragma once
#include <glm/vec2.hpp>
#include <span>

namespace ecs
{
  struct Particle
  {
    glm::vec2 position;
    glm::uvec2 emissive; // packed 16-bit float RGBA
    glm::uint velocity; // packed 16-bit float XY
    float lifetime;
  };

  // event
  struct AddParticles
  {
    std::span<const Particle> particles;
  };
}