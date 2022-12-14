#include "ParticleSystem.h"
#include "Renderer.h"
#include "ecs/Scene.h"
#include "ecs/Entity.h"
#include "utils/LoadFile.h"
#include <glm/packing.hpp>
#include <glm/glm.hpp>
#include <entt/entity/registry.hpp>
#include <Fwog/Shader.h>
#include <Fwog/Rendering.h>
#include <vector>
#include <glad/gl.h>
#include <algorithm>
#include <cmath>

#include <iostream>

namespace ecs
{
  namespace
  {
    struct Uniforms
    {
      float dt;
      float magnetism;
      glm::vec2 cursorPosition;
      float friction;
      float accelerationConstant;
      float accelerationMinDistance;
    };
  }

  ParticleSystem::ParticleSystem(Scene* scene, EventBus* eventBus, Renderer* renderer)
    : System(scene, eventBus), _renderer(renderer)
  {
    Reset(true, 5'000'000);

    auto update = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFile("assets/shaders/particles/UpdateParticles.comp.glsl"));
    auto add = Fwog::Shader(Fwog::PipelineStage::COMPUTE_SHADER, LoadFile("assets/shaders/particles/AddParticles.comp.glsl"));
    
    _particleUpdate = Fwog::CompileComputePipeline({ .shader = &update });
    _particleAdd = Fwog::CompileComputePipeline({ .shader = &add });

    _eventBus->Subscribe(this, &ParticleSystem::HandleParticleAdd);
    _eventBus->Subscribe(this, &ParticleSystem::HandleMousePosition);
  }

  void ParticleSystem::Reset(bool hard, uint32_t maxParticles)
  {
    MAX_PARTICLES = maxParticles;
    // reset to default
    if (hard)
    {
      magnetism = 1.0f;
      friction = 0.15f;
      accelerationConstant = 0.0f;
      accelerationMinDistance = 1.0f;
    }

    _particles = std::make_unique<Fwog::Buffer>(sizeof(Particle) * MAX_PARTICLES, Fwog::BufferStorageFlag::NONE);
    // +1 for int
    std::vector<int> tombstones;
    tombstones.reserve(MAX_PARTICLES + 1);
    tombstones.push_back(MAX_PARTICLES);
    for (uint32_t i = 0; i < MAX_PARTICLES; i++)
    {
      tombstones.push_back(i);
    }

    _tombstones = std::make_unique<Fwog::Buffer>(std::span(tombstones), Fwog::BufferStorageFlag::NONE);
    _renderIndices = std::make_unique<Fwog::Buffer>(sizeof(uint32_t) * (MAX_PARTICLES + 1), Fwog::BufferStorageFlag::NONE);
    _uniforms = std::make_unique<Fwog::Buffer>(sizeof(Uniforms), Fwog::BufferStorageFlag::DYNAMIC_STORAGE);

    constexpr int32_t zero = 0;
    _particles->ClearSubData(0, _particles->Size(), Fwog::Format::R32_SINT, Fwog::UploadFormat::R, Fwog::UploadType::SINT, &zero);
    _renderIndices->ClearSubData(0, sizeof(int32_t), Fwog::Format::R32_SINT, Fwog::UploadFormat::R, Fwog::UploadType::SINT, &zero);
  }

  void ParticleSystem::Update(double dt)
  {
    struct Box
    {
      glm::vec2 position;
      glm::vec2 scale;
    };
    auto viewBox = _scene->Registry().view<ecs::DebugBox>();
    std::vector<Box> boxes;
    boxes.reserve(viewBox.size());
    for (auto&& [_, box] : viewBox.each()) if (box.active) boxes.push_back({ box.translation, box.scale });
    auto boxBuffer = Fwog::Buffer(std::span(boxes));

    // make boxes that are "about to spawn" flicker in some way
    auto viewBoxLife = _scene->Registry().view<ecs::DebugBox, ecs::Flicker>();
    std::vector<ecs::Entity> removeFlickerList;
    for (auto&& [entity, box, flicker] : viewBoxLife.each())
    {
      flicker.timeLeft -= dt;
      glm::vec4 emissive = {};
      emissive.r = static_cast<float>(200 * (1 + sin(flicker.timeLeft * 4.0 * 3.1415) / 2));
      emissive.g = static_cast<float>(200 * (1 + sin(flicker.timeLeft * 4.0 * 3.1415) / 2));
      emissive = glm::mix(emissive, glm::vec4(200, 0, 0, 0), float(1.0 - flicker.timeLeft / 3.0));

      if (flicker.timeLeft <= 0)
      {
        emissive.r = 200;
        emissive.g = 0;
        removeFlickerList.push_back(Entity(entity, _scene));
        box.active = true;
      }
      box.color16f.x = glm::packHalf2x16(glm::vec2(emissive));
      //box.color16f.y = glm::packHalf2x16(glm::vec2(emissive));
    }

    for (auto&& entity : removeFlickerList)
    {
      entity.RemoveComponents<ecs::Flicker>();
    }

    auto viewBoxMove = _scene->Registry().view<ecs::DebugBox, ecs::Movement>();
    for (auto&& [_, box, move] : viewBoxMove.each())
    {
      move.accum += dt;
      move.accum = std::fmod(move.accum, move.period);
      if (move.accum / (move.period / 2.0) < 1.0)
      {
        box.translation = glm::mix(move.posA, move.posB, move.accum / (move.period / 2.0));
      }
      else
      {
        box.translation = glm::mix(move.posA, move.posB, 2 - move.accum / (move.period / 2.0));
      }
    }

    Fwog::BeginCompute("Update particles");
    {
      Fwog::Cmd::BindComputePipeline(_particleUpdate);
      Fwog::Cmd::BindStorageBuffer(0, *_particles, 0, _particles->Size());
      Fwog::Cmd::BindStorageBuffer(1, *_tombstones, 0, _tombstones->Size());
      Fwog::Cmd::BindStorageBuffer(2, *_renderIndices, 0, _renderIndices->Size());
      Fwog::Cmd::BindStorageBuffer(3, boxBuffer, 0, boxBuffer.Size());
      Fwog::Cmd::BindUniformBuffer(0, *_uniforms, 0, _uniforms->Size());

      Uniforms uniforms
      {
        .dt = static_cast<float>(dt),
        .magnetism = magnetism,
        .cursorPosition = { cursorX, cursorY },
        .friction = friction,
        .accelerationConstant = accelerationConstant,
        .accelerationMinDistance = accelerationMinDistance,
      };
      _uniforms->SubData(uniforms, 0);

      uint32_t workgroups = (MAX_PARTICLES + 511) / 512;
      Fwog::Cmd::MemoryBarrier(Fwog::MemoryBarrierAccessBit::SHADER_STORAGE_BIT | Fwog::MemoryBarrierAccessBit::UNIFORM_BUFFER_BIT);
      constexpr int32_t zero = 0;
      _renderIndices->ClearSubData(0, sizeof(int32_t), Fwog::Format::R32_SINT, Fwog::UploadFormat::R, Fwog::UploadType::SINT, &zero);
      Fwog::Cmd::Dispatch(workgroups, 1, 1);
    }
    Fwog::EndCompute();
  }

  void ParticleSystem::Draw()
  {
    // draw debug primitives
    // FYI, this is a HACK as the code is ripped straight from the debug system
    // the reason it's done this way is because the game needs a simple way to draw boxes, which the debug drawing facilities provide
    auto viewLine = _scene->Registry().view<ecs::DebugLine>();
    auto viewBox = _scene->Registry().view<ecs::DebugBox>();
    auto viewCircle = _scene->Registry().view<ecs::DebugCircle>();
    std::vector<ecs::DebugLine> lines;
    std::vector<ecs::DebugBox> boxes;
    std::vector<ecs::DebugCircle> circles;
    lines.reserve(viewLine.size());
    boxes.reserve(viewBox.size());
    circles.reserve(viewCircle.size());
    
    for (auto&& [_, box] : viewBox.each())
    {
      boxes.push_back(box);
    }

    _renderer->ClearHDR();

    _renderer->DrawBoxes(boxes);

    _renderer->DrawParticles(*_particles, *_renderIndices, MAX_PARTICLES);
  }

  std::uint32_t ParticleSystem::GetNumParticles()
  {
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    int32_t size{};
    glGetNamedBufferSubData(_tombstones->Handle(), 0, sizeof(int32_t), &size);
    auto ret = int32_t(MAX_PARTICLES) - size;
    if (ret < 0)
    {
#ifndef NDEBUG
      printf("bug! value: %d\n", ret);
#endif
      ret = 0;
    }
    return ret;
  }

  void ParticleSystem::HandleParticleAdd(AddParticles& e)
  {
    Fwog::BeginCompute("Copy particles");
    {
      auto tempBuffer = Fwog::TypedBuffer<Particle>(std::span(e.particles));
      Fwog::Cmd::BindComputePipeline(_particleAdd);
      Fwog::Cmd::BindStorageBuffer(0, *_particles, 0, _particles->Size());
      Fwog::Cmd::BindStorageBuffer(1, *_tombstones, 0, _tombstones->Size());
      Fwog::Cmd::BindStorageBuffer(2, tempBuffer, 0, tempBuffer.Size());

      uint32_t workgroups = static_cast<uint32_t>((e.particles.size() + 511) / 512);
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