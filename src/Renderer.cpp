#include "Renderer.h"
#include "GAssert.h"
#include "utils/LoadFile.h"
#include <Fwog/Rendering.h>
#include <Fwog/Pipeline.h>
#include <Fwog/Texture.h>
#include <Fwog/Buffer.h>
#include <Fwog/Shader.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <execution>
#include <atomic>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>

#if !defined(NDEBUG)
static void GLAPIENTRY glErrorCallback(
  GLenum source,
  GLenum type,
  GLuint id,
  GLenum severity,
  [[maybe_unused]] GLsizei length,
  const GLchar* message,
  [[maybe_unused]] const void* userParam)
{
  if (id == 131169 || id == 131185 || id == 131218 || id == 131204 || id == 0)
    return;

  std::stringstream errStream;
  errStream << "OpenGL Debug message (" << id << "): " << message << '\n';

  switch (source)
  {
  case GL_DEBUG_SOURCE_API:             errStream << "Source: API"; break;
  case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   errStream << "Source: Window Manager"; break;
  case GL_DEBUG_SOURCE_SHADER_COMPILER: errStream << "Source: Shader Compiler"; break;
  case GL_DEBUG_SOURCE_THIRD_PARTY:     errStream << "Source: Third Party"; break;
  case GL_DEBUG_SOURCE_APPLICATION:     errStream << "Source: Application"; break;
  case GL_DEBUG_SOURCE_OTHER:           errStream << "Source: Other"; break;
  }

  errStream << '\n';

  switch (type)
  {
  case GL_DEBUG_TYPE_ERROR:               errStream << "Type: Error"; break;
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: errStream << "Type: Deprecated Behaviour"; break;
  case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  errStream << "Type: Undefined Behaviour"; break;
  case GL_DEBUG_TYPE_PORTABILITY:         errStream << "Type: Portability"; break;
  case GL_DEBUG_TYPE_PERFORMANCE:         errStream << "Type: Performance"; break;
  case GL_DEBUG_TYPE_MARKER:              errStream << "Type: Marker"; break;
  case GL_DEBUG_TYPE_PUSH_GROUP:          errStream << "Type: Push Group"; break;
  case GL_DEBUG_TYPE_POP_GROUP:           errStream << "Type: Pop Group"; break;
  case GL_DEBUG_TYPE_OTHER:               errStream << "Type: Other"; break;
  }

  errStream << '\n';

  switch (severity)
  {
  case GL_DEBUG_SEVERITY_HIGH:
    errStream << "Severity: high";
    break;
  case GL_DEBUG_SEVERITY_MEDIUM:
    errStream << "Severity: medium";
    break;
  case GL_DEBUG_SEVERITY_LOW:
    errStream << "Severity: low";
    break;
  case GL_DEBUG_SEVERITY_NOTIFICATION:
    errStream << "Severity: notification";
    break;
  }

  std::cout << errStream.str() << '\n';
}
#endif

namespace
{
  std::vector<glm::vec2> MakeBoxVertices()
  {
    // line strip
    return { {-0.5, -0.5}, {0.5, -0.5}, {0.5, 0.5}, {-0.5, 0.5}, {-0.5, -0.5} };
  }

  std::vector<glm::vec2> MakeCircleVertices([[maybe_unused]] int segments)
  {
    std::vector<glm::vec2> vertices;
    vertices.reserve(segments + 1);

    for (int i = 0; i < segments; i++)
    {
      float theta = float(i) * glm::two_pi<float>() / segments;
      vertices.push_back({std::cos(theta), std::sin(theta)});
    }

    vertices.push_back({ 1, 0 });
    return vertices;
  }

  constexpr std::uint32_t CIRCLE_SEGMENTS = 50;
}

struct SpriteUniforms
{
  glm::mat3x2 transform;
  glm::u8vec4 tint4x8;
  glm::uint _padding;
};

struct PrimitiveUniforms
{
  glm::mat3x2 transform;
  glm::u8vec4 color;
  glm::uint _padding;
};

struct FrameUniforms
{
  glm::mat4 viewProj;
};

struct Renderer::Resources
{
  // resources that need to be recreated when the window resizes
  struct Frame
  {
    uint32_t width{};
    uint32_t height{};
    Fwog::Texture output_ldr;
    float AspectRatio()
    {
      return static_cast<float>(width) / height;
    }
  };

  Frame frame;
  Fwog::GraphicsPipeline backgroundPipeline;
  Fwog::GraphicsPipeline spritePipeline;
  Fwog::TypedBuffer<SpriteUniforms> spritesUniformsBuffer;
  Fwog::TypedBuffer<FrameUniforms> frameUniformsBuffer;

  // for drawing debug boxes and circles
  Fwog::GraphicsPipeline primitivePipeline;
  Fwog::TypedBuffer<glm::vec2> boxVertexBuffer;
  Fwog::TypedBuffer<glm::vec2> circleVertexBuffer;
    
  // for drawing debug lines
  Fwog::GraphicsPipeline linesPipeline;
};

Renderer::Renderer(GLFWwindow* window)
{
  int version = gladLoadGL(glfwGetProcAddress);
  if (version == 0)
  {
    throw std::runtime_error("Failed to initialize OpenGL");
  }

#ifndef NDEBUG
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(glErrorCallback, nullptr);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#endif

  glEnable(GL_FRAMEBUFFER_SRGB);

  int iframebufferWidth{};
  int iframebufferHeight{};
  glfwGetFramebufferSize(window, &iframebufferWidth, &iframebufferHeight);
  uint32_t framebufferWidth = static_cast<uint32_t>(iframebufferWidth);
  uint32_t framebufferHeight = static_cast<uint32_t>(iframebufferHeight);

  _resources = new Resources(
    {
      .frame = {.width = framebufferWidth,
                .height = framebufferHeight,
                .output_ldr = Fwog::CreateTexture2D({framebufferWidth, framebufferHeight}, Fwog::Format::R8G8B8A8_SRGB) },
      .spritesUniformsBuffer = Fwog::TypedBuffer<SpriteUniforms>(1024, Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      .frameUniformsBuffer = Fwog::TypedBuffer<FrameUniforms>(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      .boxVertexBuffer = Fwog::TypedBuffer<glm::vec2>(MakeBoxVertices()),
      .circleVertexBuffer = Fwog::TypedBuffer<glm::vec2>(MakeCircleVertices(CIRCLE_SEGMENTS)),
    });

  auto view = glm::mat4(1);
  auto proj = glm::ortho<float>(-10 * _resources->frame.AspectRatio(), 10 * _resources->frame.AspectRatio(), -10, 10, -1, 1);
  auto viewproj = proj * view;
  _resources->frameUniformsBuffer.SubDataTyped({ viewproj });

  auto quad_vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, LoadFile("assets/shaders/QuadBatched.vert.glsl"));
  auto bg_vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, LoadFile("assets/shaders/FullScreenTri.vert.glsl"));
  auto tex_fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, LoadFile("assets/shaders/Texture.frag.glsl"));
  _resources->spritePipeline = Fwog::CompileGraphicsPipeline({ 
    .vertexShader = &quad_vs, 
    .fragmentShader = &tex_fs,
    .inputAssemblyState = {.topology = Fwog::PrimitiveTopology::TRIANGLE_FAN }
  });
  _resources->backgroundPipeline = Fwog::CompileGraphicsPipeline({ .vertexShader = &bg_vs, .fragmentShader = &tex_fs });

  // debug pipelines
  auto simpleColor_fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, LoadFile("assets/shaders/SimpleColor.frag.glsl"));

  auto colorBlend = Fwog::ColorBlendAttachmentState
  {
    .blendEnable = true,
    .srcColorBlendFactor = Fwog::BlendFactor::SRC_ALPHA,
    .dstColorBlendFactor = Fwog::BlendFactor::ONE_MINUS_SRC_ALPHA,
  };

  auto lines_vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, LoadFile("assets/shaders/LinesBatched.vert.glsl"));
  auto linePosDesc = Fwog::VertexInputBindingDescription
  {
    .location = 0,
    .binding = 0,
    .format = Fwog::Format::R32G32_FLOAT,
    .offset = 0,
  };
  auto lineColorDesc = Fwog::VertexInputBindingDescription
  {
    .location = 1,
    .binding = 0,
    .format = Fwog::Format::R8G8B8A8_UNORM,
    .offset = offsetof(ecs::DebugLine, color0),
  };
  auto lineInputDescs = { linePosDesc, lineColorDesc };

  _resources->linesPipeline = Fwog::CompileGraphicsPipeline({
    .vertexShader = &lines_vs,
    .fragmentShader = &simpleColor_fs,
    .inputAssemblyState = {.topology = Fwog::PrimitiveTopology::LINE_LIST },
    .vertexInputState = { lineInputDescs },
    .colorBlendState = {.attachments = std::span(&colorBlend, 1) }
  });

  auto primitive_vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, LoadFile("assets/shaders/PrimitiveBatched.vert.glsl"));
  _resources->primitivePipeline = Fwog::CompileGraphicsPipeline({
    .vertexShader = &primitive_vs,
    .fragmentShader = &simpleColor_fs,
    .inputAssemblyState = {.topology = Fwog::PrimitiveTopology::LINE_STRIP },
    .vertexInputState = { std::span(&linePosDesc, 1) },
    .colorBlendState = { .attachments = std::span(&colorBlend, 1) }
  });
}

Renderer::~Renderer()
{
  delete _resources;
}

void Renderer::DrawBackground(const Fwog::Texture& texture)
{
  G_ASSERT(texture.CreateInfo().imageType == Fwog::ImageType::TEX_2D);
  Fwog::BeginSwapchainRendering({ .viewport = {.drawRect = {.offset{}, .extent{1280, 720}}},
                                  .clearColorOnLoad = true,
                                  .clearColorValue = {.f = {.3f, .8f, .2f, 1.f}} });
  Fwog::Cmd::BindGraphicsPipeline(_resources->backgroundPipeline);
  Fwog::Cmd::BindSampledImage(0, texture, Fwog::Sampler(Fwog::SamplerState{}));
  Fwog::Cmd::Draw(3, 1, 0, 0);
  Fwog::EndRendering();
}

void Renderer::DrawSprites(std::vector<RenderableSprite> sprites)
{
  if (sprites.empty())
  {
    return;
  }

  // sort sprites by texture
  std::sort(std::execution::seq,
    sprites.begin(),
    sprites.end(),
    [](const RenderableSprite& lhs, const RenderableSprite& rhs)
    {
      return lhs.texture < rhs.texture;
    });

  std::vector<SpriteUniforms> spritesUniforms;

  spritesUniforms.resize(sprites.size());
  std::transform(std::execution::seq,
    sprites.begin(),
    sprites.end(),
    spritesUniforms.begin(),
    [](const RenderableSprite& sprite)
    {
      SpriteUniforms spriteUniforms
      {
        .transform = sprite.transform,
        .tint4x8 = sprite.tint
      };
      return spriteUniforms;
    });

  // geometric expansion so we don't spam buffers
  if (_resources->spritesUniformsBuffer.Size() < spritesUniforms.size() * sizeof(SpriteUniforms))
  {
    _resources->spritesUniformsBuffer = Fwog::TypedBuffer<SpriteUniforms>(spritesUniforms.size() * 2, Fwog::BufferStorageFlag::DYNAMIC_STORAGE);
  }

  _resources->spritesUniformsBuffer.SubData(std::span(spritesUniforms), 0);

  Fwog::BeginSwapchainRendering({ .viewport = {.drawRect = {.offset{}, .extent{1280, 720} } } });
  Fwog::Cmd::BindGraphicsPipeline(_resources->spritePipeline);
  Fwog::Cmd::BindUniformBuffer(0, _resources->frameUniformsBuffer, 0, _resources->frameUniformsBuffer.Size());
  Fwog::Cmd::BindStorageBuffer(0, _resources->spritesUniformsBuffer, 0, _resources->spritesUniformsBuffer.Size());

  std::size_t firstInstance = 0;
  for (std::size_t i = 0; i < sprites.size(); i++)
  {
    const auto& sprite = sprites[i];

    // trigger draw if last sprite or if next sprite has a different texture
    if (i == sprites.size() - 1 || sprite.texture != sprites[i + 1].texture)
    {
      Fwog::Cmd::BindSampledImage(0, *sprite.texture, Fwog::Sampler(Fwog::SamplerState{}));
      Fwog::Cmd::Draw(4, static_cast<uint32_t>(i + 1 - firstInstance), 0, static_cast<uint32_t>(firstInstance));

      firstInstance = i + 1;
    }
  }
  Fwog::EndRendering();
}

void Renderer::DrawLines(std::span<const ecs::DebugLine> lines)
{
  if (lines.empty())
  {
    return;
  }
    
  // this buffer doesn't need to be created every frame
  auto vertexBuffer = Fwog::Buffer(lines);

  Fwog::BeginSwapchainRendering({ .viewport = {.drawRect = {.offset{}, .extent{1280, 720}}},
                                  .clearColorOnLoad = false });
  Fwog::Cmd::BindGraphicsPipeline(_resources->linesPipeline);
  Fwog::Cmd::BindUniformBuffer(0, _resources->frameUniformsBuffer, 0, _resources->frameUniformsBuffer.Size());
  Fwog::Cmd::BindVertexBuffer(0, vertexBuffer, 0, sizeof(ecs::DebugLine) / 2);
  Fwog::Cmd::Draw(static_cast<uint32_t>(lines.size() * 2), 1, 0, 0);
  Fwog::EndRendering();
}

void Renderer::DrawBoxes(std::span<const ecs::DebugBox> boxes)
{
  if (boxes.empty())
  {
    return;
  }

  std::vector<PrimitiveUniforms> primitives;
  primitives.reserve(boxes.size());
  for (const auto& box : boxes)
  {
    glm::mat3x2 model = glm::scale(glm::rotate(glm::translate(glm::mat3(1), box.translation), box.rotation), box.scale);
    primitives.push_back(PrimitiveUniforms{
      .transform = model,
      .color = box.color
    });
  }

  auto instanceBuffer = Fwog::Buffer(std::span(primitives));

  Fwog::BeginSwapchainRendering({ .viewport = {.drawRect = {.offset{}, .extent{1280, 720}}},
                                  .clearColorOnLoad = false });
  Fwog::Cmd::BindGraphicsPipeline(_resources->primitivePipeline);
  Fwog::Cmd::BindUniformBuffer(0, _resources->frameUniformsBuffer, 0, _resources->frameUniformsBuffer.Size());
  Fwog::Cmd::BindStorageBuffer(0, instanceBuffer, 0, instanceBuffer.Size());
  Fwog::Cmd::BindVertexBuffer(0, _resources->boxVertexBuffer, 0, sizeof(glm::vec2));
  Fwog::Cmd::Draw(5, static_cast<uint32_t>(boxes.size()), 0, 0);
  Fwog::EndRendering();
}

void Renderer::DrawCircles(std::span<const ecs::DebugCircle> circles)
{
  if (circles.empty())
  {
    return;
  }

  std::vector<PrimitiveUniforms> primitives;
  primitives.reserve(circles.size());
  for (const auto& circle : circles)
  {
    glm::mat3x2 model = glm::scale(glm::translate(glm::mat3(1), circle.translation), glm::vec2(circle.radius));
    primitives.push_back(PrimitiveUniforms{
      .transform = model,
      .color = circle.color
      });
  }

  auto instanceBuffer = Fwog::Buffer(std::span(primitives));

  Fwog::BeginSwapchainRendering({ .viewport = {.drawRect = {.offset{}, .extent{1280, 720}}},
                                  .clearColorOnLoad = false });
  Fwog::Cmd::BindGraphicsPipeline(_resources->primitivePipeline);
  Fwog::Cmd::BindUniformBuffer(0, _resources->frameUniformsBuffer, 0, _resources->frameUniformsBuffer.Size());
  Fwog::Cmd::BindStorageBuffer(0, instanceBuffer, 0, instanceBuffer.Size());
  Fwog::Cmd::BindVertexBuffer(0, _resources->circleVertexBuffer, 0, sizeof(glm::vec2));
  Fwog::Cmd::Draw(CIRCLE_SEGMENTS + 1, static_cast<uint32_t>(circles.size()), 0, 0);
  Fwog::EndRendering();
}