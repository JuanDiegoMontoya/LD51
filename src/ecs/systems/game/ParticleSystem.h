#pragma once
#include "ecs/systems/System.h"
#include "ecs/events/AddParticles.h"
#include "Input.h"
#include <Fwog/Buffer.h>
#include <Fwog/Pipeline.h>
#include <memory>

class Renderer;

namespace ecs
{
  class ParticleSystem : public System
  {
  public:
    ParticleSystem(Scene* scene, EventBus* eventBus, Renderer* renderer);

    void Reset(bool hard, uint32_t maxParticles);

    void Update(double dt) override;

    void Draw() override;

    std::uint32_t GetNumParticles();

    std::uint32_t MAX_PARTICLES;
    float magnetism;
    float friction;
    float accelerationConstant;
    float accelerationMinDistance;

  private:
    Renderer* _renderer;

    // List(s) containing per-particle attributes
    std::unique_ptr<Fwog::Buffer> _particles;

    // When a particle dies, its index gets pushed to this atomic stack
    std::unique_ptr<Fwog::Buffer> _tombstones;

    // Buffer containing a count & indices of particles to render
    std::unique_ptr<Fwog::Buffer> _renderIndices;

    std::unique_ptr<Fwog::Buffer> _uniforms;

    Fwog::ComputePipeline _particleUpdate;
    Fwog::ComputePipeline _particleAdd;

    float cursorX = 0;
    float cursorY = 0;

    void HandleParticleAdd(AddParticles& e);
    void HandleMousePosition(input::MousePositionEvent& e);
  };
}