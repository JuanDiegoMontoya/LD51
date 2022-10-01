#pragma once
#include <Fwog/Texture.h>
#include <cstdint>
#include <glm/vec4.hpp>

namespace ecs
{
  struct Sprite
  {
    const Fwog::Texture* texture; // a 2D texture
    glm::u8vec4 tint = { 255, 255, 255, 255 };
  };
}