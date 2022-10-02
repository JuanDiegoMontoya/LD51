#version 460 core

struct Particle
{
  vec2 position;
  uvec2 emissive; // packed 16-bit float RGBA
  uint velocity; // packed 16-bit float XY
  uint flags;
};

layout(std430, binding = 0) restrict buffer ParticlesBuffer
{
  Particle list[];
}particles;

layout(std430, binding = 1) coherent restrict buffer TombstonesBuffer
{
  coherent int size;
  int indices[];
}tombstones;

layout(std430, binding = 2) coherent restrict buffer RenderIndicesBuffer
{
  coherent int size;
  int indices[];
}renderIndices;

layout(std140, binding = 0) uniform Uniforms
{
  float dt;
  float magnetism;
  vec2 cursorPosition;
}uniforms;

shared int sh_freeIndex;
shared int sh_requestedFreeIndices;
shared int sh_drawIndex;
shared int sh_requestedDrawIndices;

layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;
void main()
{
  // strategy: record how many elements we actually need, request it at once, then write to buffers

  if (gl_LocalInvocationIndex == 0)
  {
    sh_requestedFreeIndices = 0;
    sh_requestedDrawIndices = 0;
  }

  barrier();
  memoryBarrierShared();

  int index = int(gl_GlobalInvocationID.x);
  bool needFreeIndex = false;
  bool needDrawIndex = false;
  if (index < particles.list.length())
  {
    Particle particle = particles.list[index];
    vec2 velocity = unpackHalf2x16(particle.velocity) * .9995;
    float accelMagnitude = uniforms.magnetism / max(1.0, distance(uniforms.cursorPosition, particle.position));
    vec2 acceleration = accelMagnitude * normalize(uniforms.cursorPosition - particle.position);

    if (particle.flags > 0)
    {
      velocity += acceleration * uniforms.dt;
      particle.position += velocity * uniforms.dt;
      particle.velocity = packHalf2x16(velocity);

      // if (life <= 0.0) // particle just died
      // {
      //   needFreeIndex = true;
      //   atomicAdd(sh_requestedFreeIndices, 1);
      // }
      // else
      // {
         // particle is alive, so we will render it (add its index to drawIndices)
         needDrawIndex = true;
         atomicAdd(sh_requestedDrawIndices, 1);
      // }
    }

    particles.list[index] = particle;
  }

  barrier();
  memoryBarrierShared();

  if (gl_LocalInvocationIndex == 0)
  {
    sh_freeIndex = atomicAdd(tombstones.size, sh_requestedFreeIndices);
    sh_drawIndex = atomicAdd(renderIndices.size, sh_requestedDrawIndices);
  }

  barrier();
  memoryBarrierShared();

  if (needFreeIndex)
  {
    int freeIndex = atomicAdd(sh_freeIndex, 1);
    tombstones.indices[freeIndex] = index;
  }

  if (needDrawIndex)
  {
    uint drawIndex = atomicAdd(sh_drawIndex, 1);
    renderIndices.indices[drawIndex] = index;
  }
}