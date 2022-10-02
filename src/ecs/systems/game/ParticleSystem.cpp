#include "ParticleSystem.h"
#include "Renderer.h"
#include "utils/LoadFile.h"
#include <Fwog/Shader.h>
#include <Fwog/Rendering.h>
#include <glm/packing.hpp>
#include <vector>

namespace ecs
{
  namespace
  {
    struct Uniforms
    {
      float dt;
      float magnetism;
      glm::vec2 cursorPosition;
    };
  }

  ParticleSystem::ParticleSystem(Scene* scene, EventBus* eventBus, Renderer* renderer)
    : System(scene, eventBus), _renderer(renderer)
  {
    // TODO: temp
    std::vector<Particle> particles;
    for (int i = 0; i < 10000; i++)
    {
      glm::vec4 em = { 1.0f, 1.0f, 1.0f, 1.0f };
      Particle particle
      {
        .position = { 0, 0 },
        .emissive = { glm::packHalf2x16({ em.r, em.g }), glm::packHalf2x16({ em.b, em.a }) },
        .velocity = glm::packHalf2x16({ 0, 0 }),
        .flags = 0
      };
      particles.push_back(particle);
    }

    _particles = std::make_unique<Fwog::Buffer>(std::span(particles), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);
    // +1 for int
    _tombstones = std::make_unique<Fwog::Buffer>(sizeof(uint32_t) * (MAX_PARTICLES + 1), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);
    _renderIndices = std::make_unique<Fwog::Buffer>(sizeof(uint32_t) + (MAX_PARTICLES + 1), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);
    _uniforms = std::make_unique<Fwog::Buffer>(sizeof(Uniforms), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);

    constexpr int32_t zero = 0;
    _tombstones->SubData(zero, 0);
    _renderIndices->SubData(zero, 0);

    auto update = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFile("assets/shaders/particles/UpdateParticles.comp.glsl"));
    auto add = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFile("assets/shaders/particles/AddParticles.comp.glsl"));
    
    _particleUpdate = Fwog::CompileComputePipeline({ .shader = &update });
    _particleAdd = Fwog::CompileComputePipeline({ .shader = &add });
  }

  void ParticleSystem::Update(double dt)
  {
    Fwog::BeginCompute("Update particles");
    {
      Fwog::Cmd::BindComputePipeline(_particleUpdate);
      Fwog::Cmd::BindStorageBuffer(0, *_particles, 0, _particles->Size());
      Fwog::Cmd::BindStorageBuffer(1, *_tombstones, 0, _tombstones->Size());
      Fwog::Cmd::BindStorageBuffer(2, *_renderIndices, 0, _renderIndices->Size());
      Fwog::Cmd::BindUniformBuffer(0, *_uniforms, 0, _uniforms->Size());
      constexpr int32_t zero = 0;
      _renderIndices->ClearSubData(0, sizeof(int32_t), Fwog::Format::R32_SINT, Fwog::UploadFormat::R, Fwog::UploadType::SINT, &zero);

      Uniforms uniforms
      {
        .dt = static_cast<float>(dt),
        .magnetism = 1.0f,
        .cursorPosition = { 2, 2 }
      };
      _uniforms->SubData(uniforms, 0);

      uint32_t workgroups = (MAX_PARTICLES + 127) / 128;
      Fwog::Cmd::Dispatch(workgroups, 1, 1);
    }
    Fwog::EndCompute();
  }

  void ParticleSystem::Draw()
  {
    _renderer->DrawParticles(*_particles, *_renderIndices);
  }

  void ParticleSystem::HandleParticleAdd(AddParticles& e)
  {
    (void)e;
  }
}