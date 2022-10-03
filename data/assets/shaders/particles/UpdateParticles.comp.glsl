#version 460 core

struct Particle
{
  vec2 position;
  uvec2 emissive; // packed 16-bit float RGBA
  uint velocity; // packed 16-bit float XY
  float lifetime;
};

struct Box
{
  vec2 position;
  vec2 scale;
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

layout(std430, binding = 3) readonly restrict buffer WallBuffer
{
  Box list[];
}walls;

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
    float accelMagnitude = uniforms.magnetism / max(2.0, distance(uniforms.cursorPosition, particle.position));
    accelMagnitude = 1.0;
    vec2 acceleration = accelMagnitude * normalize(uniforms.cursorPosition - particle.position);

    // visualize velocity
    //particle.emissive.x = packHalf2x16(abs(velocity) * .2);

    // visualize acceleration
    //particle.emissive.x = packHalf2x16(abs(acceleration) * .2);

    // visualize velocity magnitude
    if (particle.lifetime > 1)
    {
      particle.emissive.y = packHalf2x16(vec2(length(velocity * 0.4), unpackHalf2x16(particle.emissive.y).y));
    }

    // visualize acceleration magnitude
    //particle.emissive.y = packHalf2x16(vec2(length(acceleration), 1.0));

    if (particle.lifetime > 0.0)
    {
      velocity += acceleration * uniforms.dt;

      // test the particle against each wall
      for (int i = 0; i < walls.list.length(); i++)
      {
        Box box = walls.list[i];
        const vec2 ppos = particle.position;
        const vec2 bpos = box.position;
        const vec2 bscl = box.scale / 1.5;
        if (ppos.x > bpos.x - bscl.x && ppos.y > bpos.y - bscl.y &&
            ppos.x < bpos.x + bscl.x && ppos.y < bpos.y + bscl.y)
        {
          if (particle.lifetime > 1)
          {
            particle.lifetime = 1;
          }
          particle.emissive.x = packHalf2x16(vec2(2.8, .1));
          particle.emissive.y = packHalf2x16(vec2(.1, .0));
          vec2 refldir = reflect(normalize(velocity), vec2(0, 1));
          velocity = refldir * length(velocity);
          particle.position += velocity * uniforms.dt * 2;
          break;
        }
      }

      particle.position += velocity * uniforms.dt;
      particle.velocity = packHalf2x16(velocity);
      particle.lifetime -= uniforms.dt;

      if (particle.lifetime <= 0.0) // particle just died
      {
        needFreeIndex = true;
        atomicAdd(sh_requestedFreeIndices, 1);
      }
      else
      {
         // particle is alive, so we will render it (add its index to drawIndices)
         needDrawIndex = true;
         atomicAdd(sh_requestedDrawIndices, 1);
      }
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