#include "ParticleSystem.h"
#include "Renderer.h"
#include "utils/LoadFile.h"
#include <Fwog/Shader.h>
#include <Fwog/Rendering.h>
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
    _particles = std::make_unique<Fwog::Buffer>(sizeof(Particle) * MAX_PARTICLES, Fwog::BufferStorageFlag::NONE);
    // +1 for int
    std::vector<int> tombstones;
    tombstones.reserve(MAX_PARTICLES + 1);
    tombstones.push_back(MAX_PARTICLES);
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
      tombstones.push_back(i);
    }

    _tombstones = std::make_unique<Fwog::Buffer>(std::span(tombstones), Fwog::BufferStorageFlag::NONE);
    _renderIndices = std::make_unique<Fwog::Buffer>(sizeof(uint32_t) * (MAX_PARTICLES + 1), Fwog::BufferStorageFlag::NONE);
    _uniforms = std::make_unique<Fwog::Buffer>(sizeof(Uniforms), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);

    constexpr int32_t zero = 0;
    _particles->ClearSubData(0, _particles->Size(), Fwog::Format::R32_SINT, Fwog::UploadFormat::R, Fwog::UploadType::SINT, &zero);
    _renderIndices->ClearSubData(0, sizeof(int32_t), Fwog::Format::R32_SINT, Fwog::UploadFormat::R, Fwog::UploadType::SINT, &zero);

    auto update = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFile("assets/shaders/particles/UpdateParticles.comp.glsl"));
    auto add = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFile("assets/shaders/particles/AddParticles.comp.glsl"));
    
    _particleUpdate = Fwog::CompileComputePipeline({ .shader = &update });
    _particleAdd = Fwog::CompileComputePipeline({ .shader = &add });

    _eventBus->Subscribe(this, &ParticleSystem::HandleParticleAdd);
    _eventBus->Subscribe(this, &ParticleSystem::HandleMousePosition);
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
        .cursorPosition = { cursorX, cursorY }
      };
      _uniforms->SubData(uniforms, 0);

      uint32_t workgroups = (MAX_PARTICLES + 127) / 128;
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::SHADER_STORAGE_BIT | Fwog::MemoryBarrierAccessBit::UNIFORM_BUFFER_BIT);
      Fwog::Cmd::Dispatch(workgroups, 1, 1);
    }
    Fwog::EndCompute();
  }

  void ParticleSystem::Draw()
  {
    _renderer->DrawParticles(*_particles, *_renderIndices, MAX_PARTICLES);
  }

  void ParticleSystem::HandleParticleAdd(AddParticles& e)
  {
    Fwog::BeginCompute("Copy particles");
    {
      auto tempBuffer = Fwog::TypedBuffer<Particle>(e.particles);
      Fwog::Cmd::BindComputePipeline(_particleAdd);
      Fwog::Cmd::BindStorageBuffer(0, *_particles, 0, _particles->Size());
      Fwog::Cmd::BindStorageBuffer(1, *_tombstones, 0, _tombstones->Size());
      Fwog::Cmd::BindStorageBuffer(2, tempBuffer, 0, tempBuffer.Size());

      uint32_t workgroups = static_cast<uint32_t>((e.particles.size() + 63) / 64);
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::SHADER_STORAGE_BIT);
      Fwog::Cmd::Dispatch(workgroups, 1, 1);
    }
    Fwog::EndCompute();
  }

  void ParticleSystem::HandleMousePosition(input::MousePositionEvent& e)
  {
    cursorX = float(e.cursorPosX / e.windowX * 2.0 - 1.0);
    cursorY = float(e.cursorPosY / e.windowY * 2.0 - 1.0);
  }
}