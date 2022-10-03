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

struct Box2
{
  vec3 radius;
  vec3 invRadius;
  vec3 center;
};
struct Ray
{
  vec3 origin;
  vec3 dir;
};

// https://www.jcgt.org/published/0007/03/04/paper-lowres.pdf
bool IntersectBox(in Box2 box, in Ray ray, out float dist, out vec3 normal)
{
  ray.origin = ray.origin - box.center;

  vec3 sgn = -sign(ray.dir);
  vec3 d = box.radius * sgn - ray.origin;
  d *= 1.0 / ray.dir;

  #define TEST(U, VW) (d.U >= 0.0) && \
    all(lessThan(abs(ray.origin.VW + ray.dir.VW * d.U), box.radius.VW))
  bvec3 test = bvec3(TEST(x, yz), TEST(y, zx), TEST(z, xy));
  sgn = test.x ? vec3(sgn.x,0,0) : (test.y ? vec3(0,sgn.y,0) : vec3(0,0,test.z ? sgn.z:0));
  #undef TEST

  normal = sgn;
  dist = (sgn.x != 0) ? d.x : ((sgn.y != 0) ? d.y : d.z);
  return (sgn.x != 0) || (sgn.y != 0) || (sgn.z != 0);
}

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
        const vec2 bscl = box.scale / 2;
        
        if (ppos.x > bpos.x - bscl.x && ppos.y > bpos.y - bscl.y &&
            ppos.x < bpos.x + bscl.x && ppos.y < bpos.y + bscl.y)
        {
          Box2 box2;
          box2.radius = vec3(bscl, 0.01);
          box2.invRadius = 1.0 / box2.radius;
          box2.center = vec3(bpos, 0.1);
          Ray ray;
          ray.origin = vec3(particle.position, 0.1);
          ray.dir = normalize(vec3(velocity, 0.01));

          float dist;
          vec3 normal;
          IntersectBox(box2, ray, dist, normal);
          {
            if (particle.lifetime > 1)
            {
              particle.lifetime = 1;
            }
            particle.emissive.x = packHalf2x16(vec2(2.8, .1));
            particle.emissive.y = packHalf2x16(vec2(.1, .0));
            vec2 refldir = reflect(-normalize(velocity), vec2(normal));
            velocity = refldir * length(velocity) * 2;
            particle.position += velocity * uniforms.dt * 1.2;
            break;
          }
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