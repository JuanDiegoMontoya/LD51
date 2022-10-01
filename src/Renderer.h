#pragma once
#include "ecs/components/DebugDraw.h"
#include <string_view>
#include <vector>
#include <span>
#include <glm/mat3x2.hpp>
#include <glm/vec4.hpp>

struct GLFWwindow;

namespace Fwog
{
  class Texture;
}

struct RenderableSprite
{
  glm::mat3x2 transform;
  const Fwog::Texture* texture;
  glm::u8vec4 tint;
};

class Renderer
{
public:
  Renderer(GLFWwindow* window);
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer(Renderer&&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer& operator=(Renderer&&) = delete;

  void DrawBackground(const Fwog::Texture& texture);
  void DrawSprites(std::vector<RenderableSprite> sprites);

  // debug drawing utilities
  void DrawLines(std::span<const ecs::DebugLine> lines);
  void DrawBoxes(std::span<const ecs::DebugBox> boxes);
  void DrawCircles(std::span<const ecs::DebugCircle> circles);

  struct Resources;

private:
  Resources* _resources;
};