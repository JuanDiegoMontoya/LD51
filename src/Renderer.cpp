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
  if (id == 131169 || id == 131185 || id == 131218 || id == 131204 || id == 131186 || id == 0)
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
  glm::uvec2 color16f;
};

struct FrameUniforms
{
  glm::mat4 viewProj;
};

struct BloomDownsampleUniforms
{
  glm::ivec2 sourceDim;
  glm::ivec2 targetDim;
  float sourceLod;
};

struct BloomUpsampleUniforms
{
  glm::ivec2 sourceDim;
  glm::ivec2 targetDim;
  float width;
  float strength;
  float sourceLod;
  float targetLod;
};

struct Renderer::Resources
{
  // resources that need to be recreated when the window resizes
  struct Frame
  {
    uint32_t width{};
    uint32_t height{};
    Fwog::Texture output_ldr;
    Fwog::Texture output_hdr;
    Fwog::Texture output_hdr_scratch;
    Fwog::Texture particle_hdr_r;
    Fwog::Texture particle_hdr_g;
    Fwog::Texture particle_hdr_b;

    float AspectRatio()
    {
      return static_cast<float>(width) / height;
    }
  };

  Frame frame;
  Fwog::GraphicsPipeline backgroundPipeline;
  Fwog::GraphicsPipeline spritePipeline;
  Fwog::ComputePipeline particlePipeline;
  Fwog::ComputePipeline tonemapPipeline;
  Fwog::GraphicsPipeline particleResolvePipeline;
  Fwog::ComputePipeline bloomDownsampleLowPass;
  Fwog::ComputePipeline bloomDownsample;
  Fwog::ComputePipeline bloomUpsample;
  Fwog::TypedBuffer<SpriteUniforms> spritesUniformsBuffer;
  Fwog::TypedBuffer<FrameUniforms> frameUniformsBuffer;
  Fwog::TypedBuffer<BloomDownsampleUniforms> bloomDownsampleUniformBuffer;
  Fwog::TypedBuffer<BloomUpsampleUniforms> bloomUpsampleUniformBuffer;

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
  glDisable(GL_DITHER);

  int iframebufferWidth{};
  int iframebufferHeight{};
  glfwGetFramebufferSize(window, &iframebufferWidth, &iframebufferHeight);
  uint32_t framebufferWidth = static_cast<uint32_t>(iframebufferWidth);
  uint32_t framebufferHeight = static_cast<uint32_t>(iframebufferHeight);

  _resources = new Resources(
    {
      .frame = {.width = framebufferWidth,
                .height = framebufferHeight,
                .output_ldr = Fwog::CreateTexture2D({framebufferWidth, framebufferHeight}, Fwog::Format::R8G8B8A8_UNORM, "output_ldr"),
                .output_hdr = Fwog::CreateTexture2DMip({framebufferWidth, framebufferHeight}, Fwog::Format::R16G16B16A16_FLOAT, 8, "output_hdr"),
                .output_hdr_scratch = Fwog::CreateTexture2DMip({framebufferWidth / 2, framebufferHeight / 2}, Fwog::Format::R16G16B16A16_FLOAT, 8, "output_hdr_scratch"),
                .particle_hdr_r = Fwog::CreateTexture2D({framebufferWidth, framebufferHeight}, Fwog::Format::R32_UINT),
                .particle_hdr_g = Fwog::CreateTexture2D({framebufferWidth, framebufferHeight}, Fwog::Format::R32_UINT),
                .particle_hdr_b = Fwog::CreateTexture2D({framebufferWidth, framebufferHeight}, Fwog::Format::R32_UINT) },
      .spritesUniformsBuffer = Fwog::TypedBuffer<SpriteUniforms>(1024, Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      .frameUniformsBuffer = Fwog::TypedBuffer<FrameUniforms>(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      .bloomDownsampleUniformBuffer = Fwog::TypedBuffer<BloomDownsampleUniforms>(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      .bloomUpsampleUniformBuffer = Fwog::TypedBuffer<BloomUpsampleUniforms>(Fwog::BufferStorageFlag::DYNAMIC_STORAGE),
      .boxVertexBuffer = Fwog::TypedBuffer<glm::vec2>(MakeBoxVertices()),
      .circleVertexBuffer = Fwog::TypedBuffer<glm::vec2>(MakeCircleVertices(CIRCLE_SEGMENTS)),
    });

  auto view = glm::mat4(1);
  auto proj = glm::ortho<float>(-1 * _resources->frame.AspectRatio(), 1 * _resources->frame.AspectRatio(), -1, 1, -1, 1);
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
    //.colorBlendState = {.attachments = std::span(&colorBlend, 1) }
  });

  auto primitive_vs = Fwog::Shader(Fwog::PipelineStage::VERTEX_SHADER, LoadFile("assets/shaders/PrimitiveBatched.vert.glsl"));
  _resources->primitivePipeline = Fwog::CompileGraphicsPipeline({
    .vertexShader = &primitive_vs,
    .fragmentShader = &simpleColor_fs,
    .inputAssemblyState = {.topology = Fwog::PrimitiveTopology::LINE_STRIP },
    .vertexInputState = { std::span(&linePosDesc, 1) },
    //.colorBlendState = { .attachments = std::span(&colorBlend, 1) }
  });

  auto particle_cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFile("assets/shaders/particles/RenderParticles.comp.glsl"));
  _resources->particlePipeline = Fwog::CompileComputePipeline({ .shader = &particle_cs });

  auto colorBlendParticle = Fwog::ColorBlendAttachmentState
  {
    .blendEnable = true,
    .srcColorBlendFactor = Fwog::BlendFactor::ONE,
    .dstColorBlendFactor = Fwog::BlendFactor::ONE,
  };
  auto particle_resolve_fs = Fwog::Shader(Fwog::PipelineStage::FRAGMENT_SHADER, LoadFile("assets/shaders/particles/ResolveParticleImage.frag.glsl"));
  _resources->particleResolvePipeline = Fwog::CompileGraphicsPipeline({
    .vertexShader = &bg_vs,
    .fragmentShader = &particle_resolve_fs,
    .colorBlendState = { .attachments = std::span(&colorBlendParticle, 1) }
  });

  auto tonemap_cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFile("assets/shaders/bloom/TonemapAndDither.comp.glsl"));
  _resources->tonemapPipeline = Fwog::CompileComputePipeline({ .shader = &tonemap_cs });
  
  auto bloom_downsampleLowPass_cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFile("assets/shaders/bloom/DownsampleLowPass.comp.glsl"));
  _resources->bloomDownsampleLowPass = Fwog::CompileComputePipeline({ .shader = &bloom_downsampleLowPass_cs });

  auto bloom_downsample_cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFile("assets/shaders/bloom/Downsample.comp.glsl"));
  _resources->bloomDownsample = Fwog::CompileComputePipeline({ .shader = &bloom_downsample_cs });

  auto bloom_upsample_cs = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFile("assets/shaders/bloom/Upsample.comp.glsl"));
  _resources->bloomUpsample = Fwog::CompileComputePipeline({ .shader = &bloom_upsample_cs });
}

Renderer::~Renderer()
{
  delete _resources;
}

void Renderer::ApplyBloom(const Fwog::Texture& target, uint32_t passes, float strength, float width, const Fwog::Texture& scratchTexture)
{
  G_ASSERT_MSG(target.Extent().width >> passes > 0 && target.Extent().height >> passes > 0, "Bloom target is too small");

  Fwog::SamplerState samplerState;
  samplerState.minFilter = Fwog::Filter::LINEAR;
  samplerState.magFilter = Fwog::Filter::LINEAR;
  samplerState.mipmapFilter = Fwog::Filter::NEAREST;
  samplerState.addressModeU = Fwog::AddressMode::MIRRORED_REPEAT;
  samplerState.addressModeV = Fwog::AddressMode::MIRRORED_REPEAT;
  samplerState.lodBias = 0;
  samplerState.minLod = -1000;
  samplerState.maxLod = 1000;
  auto sampler = Fwog::Sampler(samplerState);

  Fwog::BeginCompute("Bloom");
  Fwog::Cmd::BindUniformBuffer(0, _resources->bloomDownsampleUniformBuffer, 0, _resources->bloomDownsampleUniformBuffer.Size());
  const int local_size = 16;
  for (uint32_t i = 0; i < passes; i++)
  {
    Fwog::Extent2D sourceDim{};
    Fwog::Extent2D targetDim = target.Extent() >> (i + 1);
    float sourceLod{};

    const Fwog::Texture* sourceTex = nullptr;

    // first pass, use downsampling with low-pass filter
    if (i == 0)
    {
      // the low pass filter prevents single pixels/thin lines from being bright
      //Fwog::Cmd::BindComputePipeline(_resources->bloomDownsampleLowPass);
      Fwog::Cmd::BindComputePipeline(_resources->bloomDownsample);

      sourceLod = 0;
      sourceTex = &target;
      sourceDim = { target.Extent().width, target.Extent().height };
    }
    else
    {
      Fwog::Cmd::BindComputePipeline(_resources->bloomDownsample);

      sourceLod = static_cast<float>(i - 1);
      sourceTex = &scratchTexture;
      sourceDim = target.Extent() >> i;
    }

    Fwog::Cmd::BindSampledImage(0, *sourceTex, sampler);
    Fwog::Cmd::BindImage(0, scratchTexture, i);

    BloomDownsampleUniforms uniforms
    {
      .sourceDim = { sourceDim.width, sourceDim.height },
      .targetDim = { targetDim.width, targetDim.height },
      .sourceLod = sourceLod
    };
    _resources->bloomDownsampleUniformBuffer.SubDataTyped(uniforms);

    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT | Fwog::MemoryBarrierAccessBit::IMAGE_ACCESS_BIT);
    auto workgroups = (targetDim + local_size - 1) / local_size;
    Fwog::Cmd::Dispatch(workgroups.width, workgroups.height, 1);
  }

  Fwog::Cmd::BindComputePipeline(_resources->bloomUpsample);
  Fwog::Cmd::BindUniformBuffer(0, _resources->bloomUpsampleUniformBuffer, 0, _resources->bloomUpsampleUniformBuffer.Size());
  for (int32_t i = passes - 1; i >= 0; i--)
  {
    Fwog::Extent2D sourceDim = target.Extent() >> (i + 1);
    Fwog::Extent2D targetDim{};
    const Fwog::Texture* targetTex = nullptr;
    float realStrength = 1.0f;
    uint32_t targetLod{};

    // final pass
    if (i == 0)
    {
      realStrength = strength;
      targetLod = 0;
      targetTex = &target;
      targetDim = target.Extent();
    }
    else
    {
      targetLod = i - 1;
      targetTex = &scratchTexture;
      targetDim = target.Extent() >> i;
    }

    Fwog::Cmd::BindSampledImage(0, scratchTexture, sampler);
    Fwog::Cmd::BindSampledImage(1, *targetTex, sampler);
    Fwog::Cmd::BindImage(0, *targetTex, targetLod);

    BloomUpsampleUniforms uniforms
    {
      .sourceDim = { sourceDim.width, sourceDim.height },
      .targetDim = { targetDim.width, targetDim.height },
      .width = width,
      .strength = realStrength,
      .sourceLod = static_cast<float>(i),
      .targetLod = static_cast<float>(targetLod)
    };
    _resources->bloomUpsampleUniformBuffer.SubDataTyped(uniforms);

    auto workgroups = (targetDim + local_size - 1) / local_size;
    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT | Fwog::MemoryBarrierAccessBit::IMAGE_ACCESS_BIT);
    Fwog::Cmd::Dispatch(workgroups.width, workgroups.height, 1);
  }
  Fwog::EndCompute();
}

void Renderer::DrawBackground(const Fwog::Texture& texture)
{
  G_ASSERT(texture.CreateInfo().imageType == Fwog::ImageType::TEX_2D);
  Fwog::BeginSwapchainRendering({ .viewport = {.drawRect = {.offset{}, .extent{_resources->frame.width, _resources->frame.height}}},
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

  Fwog::BeginSwapchainRendering({ .viewport = {.drawRect = {.offset{}, .extent{_resources->frame.width, _resources->frame.height} } } });
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

void Renderer::ClearHDR()
{
  auto attachment0 = Fwog::RenderAttachment{ .texture = &_resources->frame.output_hdr, .clearValue{.color{.f = 0}}, .clearOnLoad = true };
  Fwog::BeginRendering({ .name = "clear", .colorAttachments = {{attachment0}} });
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

  Fwog::BeginSwapchainRendering({ .viewport = {.drawRect = {.offset{}, .extent{_resources->frame.width, _resources->frame.height}}},
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
      .color16f = box.color16f
    });
  }

  auto instanceBuffer = Fwog::Buffer(std::span(primitives));

  //Fwog::BeginSwapchainRendering({ .viewport = {.drawRect = {.offset{}, .extent{_resources->frame.width, _resources->frame.height}}},
  //                                .clearColorOnLoad = false });

  auto attachment0 = Fwog::RenderAttachment{ .texture = &_resources->frame.output_hdr };
  Fwog::BeginRendering({ .name = "debug boxes", .colorAttachments = {{attachment0}}});
  {
    Fwog::Cmd::BindGraphicsPipeline(_resources->primitivePipeline);
    Fwog::Cmd::BindUniformBuffer(0, _resources->frameUniformsBuffer, 0, _resources->frameUniformsBuffer.Size());
    Fwog::Cmd::BindStorageBuffer(0, instanceBuffer, 0, instanceBuffer.Size());
    Fwog::Cmd::BindVertexBuffer(0, _resources->boxVertexBuffer, 0, sizeof(glm::vec2));
    Fwog::Cmd::Draw(5, static_cast<uint32_t>(boxes.size()), 0, 0);
  }
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
      .color16f = circle.color16f
      });
  }

  auto instanceBuffer = Fwog::Buffer(std::span(primitives));

  Fwog::BeginSwapchainRendering({ .viewport = {.drawRect = {.offset{}, .extent{_resources->frame.width, _resources->frame.height}}},
                                  .clearColorOnLoad = false });
  Fwog::Cmd::BindGraphicsPipeline(_resources->primitivePipeline);
  Fwog::Cmd::BindUniformBuffer(0, _resources->frameUniformsBuffer, 0, _resources->frameUniformsBuffer.Size());
  Fwog::Cmd::BindStorageBuffer(0, instanceBuffer, 0, instanceBuffer.Size());
  Fwog::Cmd::BindVertexBuffer(0, _resources->circleVertexBuffer, 0, sizeof(glm::vec2));
  Fwog::Cmd::Draw(CIRCLE_SEGMENTS + 1, static_cast<uint32_t>(circles.size()), 0, 0);
  Fwog::EndRendering();
}

void Renderer::DrawParticles(const Fwog::Buffer& particles, const Fwog::Buffer& renderIndices, uint32_t maxParticles)
{
  Fwog::ClearColorValue ccv{};
  ccv.ui[0] = 0;
  ccv.ui[1] = 0;
  ccv.ui[2] = 0;
  ccv.ui[3] = 0;
  auto attachment0 = Fwog::RenderAttachment{ .texture = &_resources->frame.particle_hdr_r, .clearValue{.color{ccv}}, .clearOnLoad = true };
  auto attachment1 = Fwog::RenderAttachment{ .texture = &_resources->frame.particle_hdr_g, .clearValue{.color{ccv}}, .clearOnLoad = true };
  auto attachment2 = Fwog::RenderAttachment{ .texture = &_resources->frame.particle_hdr_b, .clearValue{.color{ccv}}, .clearOnLoad = true };
  auto attachments = { attachment0, attachment1, attachment2 };
  
  // dumb hack to prevent warnings
  glUseProgram(static_cast<GLuint>(_resources->backgroundPipeline.id));
  glDisable(GL_BLEND);
  Fwog::BeginRendering({ .colorAttachments = { attachments } });
  Fwog::EndRendering();

  Fwog::BeginCompute("Render particles");
  {
    Fwog::Cmd::BindComputePipeline(_resources->particlePipeline);
    Fwog::Cmd::BindStorageBuffer(0, particles, 0, particles.Size());
    Fwog::Cmd::BindStorageBuffer(1, renderIndices, 0, renderIndices.Size());
    Fwog::Cmd::BindImage(0, _resources->frame.particle_hdr_r, 0);
    Fwog::Cmd::BindImage(1, _resources->frame.particle_hdr_g, 0);
    Fwog::Cmd::BindImage(2, _resources->frame.particle_hdr_b, 0);

    uint32_t workgroups = (maxParticles + 63) / 64;
    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::IMAGE_ACCESS_BIT | Fwog::MemoryBarrierAccessBit::SHADER_STORAGE_BIT);
    Fwog::Cmd::Dispatch(workgroups, 1, 1);
  }
  Fwog::EndCompute();

  auto sampler = Fwog::Sampler(Fwog::SamplerState{ .minFilter = Fwog::Filter::NEAREST, .magFilter = Fwog::Filter::NEAREST });
  auto attachment = Fwog::RenderAttachment{ .texture = &_resources->frame.output_hdr };
  Fwog::BeginRendering({ .name = "Resolve particles", .colorAttachments = {{ attachment }} });
  {
    Fwog::Cmd::BindGraphicsPipeline(_resources->particleResolvePipeline);
    Fwog::Cmd::BindSampledImage(0, _resources->frame.particle_hdr_r, sampler);
    Fwog::Cmd::BindSampledImage(1, _resources->frame.particle_hdr_g, sampler);
    Fwog::Cmd::BindSampledImage(2, _resources->frame.particle_hdr_b, sampler);
    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::TEXTURE_FETCH_BIT);
    Fwog::Cmd::Draw(3, 1, 0, 0);
  }
  Fwog::EndRendering();

  ApplyBloom(_resources->frame.output_hdr, 6, 1.0f / 64.0f, 1.0f, _resources->frame.output_hdr_scratch);

  // fuggit, I'm gonna resolve the final image here too
  Fwog::BeginCompute("Tonemap");
  {
    Fwog::Cmd::BindComputePipeline(_resources->tonemapPipeline);
    Fwog::Cmd::BindSampledImage(0, _resources->frame.output_hdr, sampler);
    Fwog::Cmd::BindImage(0, _resources->frame.output_ldr, 0);

    auto workgroups = (_resources->frame.output_ldr.Extent() + 7) / 8;
    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::IMAGE_ACCESS_BIT);
    Fwog::Cmd::Dispatch(workgroups.width, workgroups.height, 1);
    Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::FRAMEBUFFER_BIT);
  }
  Fwog::EndCompute();

  Fwog::BlitTextureToSwapchain(_resources->frame.output_ldr,
                               { 0, 0, 0 },
                               { 0, 0, 0 },
                               _resources->frame.output_ldr.Extent(),
                               { _resources->frame.width, _resources->frame.height, 1 },
                               Fwog::Filter::LINEAR);
}
