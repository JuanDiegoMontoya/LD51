#include "RenderingSystem.h"
#include "Renderer.h"
#include "ecs/components/core/Sprite.h"
#include "ecs/components/core/Transform.h"
#include "ecs/Scene.h"
#include "utils/EventBus.h"
#include <Fwog/Texture.h>
#include <entt/entity/registry.hpp>
#include <stb_image.h>
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glad/gl.h>
#include <vector>
#include <utility>
#include <algorithm>

namespace ecs
{
  RenderingSystem::RenderingSystem(ecs::Scene* scene, EventBus* eventBus, GLFWwindow* window, Renderer* renderer)
    : System(scene, eventBus),
      _renderer(renderer),
      _window(window)
  {
    //int x{};
    //int y{};
    //int nc{};
    //stbi_set_flip_vertically_on_load(true);
    //auto* pixels = stbi_load("assets/textures/doge.png", &x, &y, &nc, 4);

    //_backgroundTexture = std::make_unique<Fwog::Texture>(Fwog::CreateTexture2D({ static_cast<uint32_t>(x), static_cast<uint32_t>(y) }, Fwog::Format::R8G8B8A8_SRGB));
    //_backgroundTexture->SubImage({ .dimension = Fwog::UploadDimension::TWO,
    //                 .size = _backgroundTexture->Extent(),
    //                 .format = Fwog::UploadFormat::RGBA,
    //                 .type = Fwog::UploadType::UBYTE,
    //                 .pixels = pixels });

    //stbi_image_free(pixels);
  }

  void RenderingSystem::Update([[maybe_unused]] double dt)
  {
    glDisable(GL_FRAMEBUFFER_SRGB);
    return;

    //_renderer->DrawBackground(*_backgroundTexture);

    //std::vector<RenderableSprite> sprites;

    //auto view = _scene->Registry().view<ecs::Transform, ecs::Sprite>();

    //sprites.reserve(view.size_hint());

    //std::for_each(view.begin(), view.end(),
    //  [&sprites, view](auto entity)
    //  {
    //    const auto&& [transform, sprite] = view.get<ecs::Transform, ecs::Sprite>(entity);
    //    auto model = glm::mat3x2(1);
    //    model = glm::scale(glm::rotate(glm::translate(glm::mat3(1), transform.translation), transform.rotation), transform.scale);

    //    sprites.push_back(RenderableSprite{
    //      .transform = model,
    //      .texture = sprite.texture,
    //      .tint = sprite.tint
    //    });
    //  });

    //_renderer->DrawSprites(std::move(sprites));
  }
}