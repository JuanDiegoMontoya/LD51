#version 460 core

struct Particle
{
  vec2 position;
  uvec2 emissive; // packed 16-bit float RGBA
  uint velocity; // packed 16-bit float XY
  float lifetime;
};

layout(std430, binding = 0) writeonly restrict buffer ParticlesBuffer
{
  Particle list[];
}outParticles;

layout(std430, binding = 1) coherent restrict buffer TombstonesBuffer
{
  int size;
  int indices[];
}tombstones;

layout(std430, binding = 2) readonly restrict buffer ParticlesCopyBuffer
{
  Particle list[];
}inParticles;

layout(local_size_x = 512, local_size_y = 1, local_size_z = 1) in;
void main()
{
  uint index = gl_GlobalInvocationID.x;
  if (index >= inParticles.list.length())
    return;

  // undo decrement and return if nothing in freelist
  int indexIndex = atomicAdd(tombstones.size, -1) - 1;
  if (indexIndex < 0)
  {
    atomicAdd(tombstones.size, 1);
    return;
  }

  int particleIndex = tombstones.indices[indexIndex];
  outParticles.list[particleIndex] = inParticles.list[index];
}