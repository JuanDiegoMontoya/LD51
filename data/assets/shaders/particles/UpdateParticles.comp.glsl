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
  float friction;
  float accelerationConstant; // 0 = use dynamic acceleration
  float accelerationMinDistance;
}uniforms;

shared int sh_freeIndex;
shared int sh_requestedFreeIndices;
shared int sh_drawIndex;
shared int sh_requestedDrawIndices;

layout(local_size_x = 512, local_size_y = 1, local_size_z = 1) in;
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
    // https://gamedev.stackexchange.com/a/109046
    vec2 velocity = unpackHalf2x16(particle.velocity) * (1.0 / (1.0 + (uniforms.dt * uniforms.friction)));
    float accelMagnitude = uniforms.magnetism / max(uniforms.accelerationMinDistance, distance(uniforms.cursorPosition, particle.position));
    if (uniforms.accelerationConstant != 0)
    {
      accelMagnitude = uniforms.accelerationConstant;
    }
    vec2 acceleration = accelMagnitude * normalize(uniforms.cursorPosition - particle.position);

    // visualize velocity
    //particle.emissive.x = packHalf2x16(abs(velocity) * .2);

    // visualize acceleration
    //particle.emissive.x = packHalf2x16(abs(acceleration) * .2);

    // visualize velocity magnitude
    if (particle.lifetime > 1)
    {
      particle.emissive.y = packHalf2x16(vec2(unpackHalf2x16(particle.emissive.y).x, length(velocity * 1.4)));
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
        const vec2 bscl = box.scale / 2;
        
        if (ppos.x > bpos.x - bscl.x && ppos.y > bpos.y - bscl.y &&
            ppos.x < bpos.x + bscl.x && ppos.y < bpos.y + bscl.y)
        {
          particle.emissive.x = packHalf2x16(vec2(2.8, .1));
          particle.emissive.y = packHalf2x16(vec2(.1, .0));
          //vec2 refldir = reflect(-normalize(velocity), vec2(normal));
          //velocity = refldir * length(velocity) * 2;

          if (particle.lifetime > 1)
          {
            particle.lifetime = 1;
            particle.position += velocity * uniforms.dt * 3.0;
            velocity = -velocity * 1.5;
            //if (index % 8 == 0) velocity *= -1;
          }
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