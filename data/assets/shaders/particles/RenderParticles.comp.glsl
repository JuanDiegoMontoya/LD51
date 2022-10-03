#version 460 core

struct Particle
{
  vec2 position;
  uvec2 emissive; // packed 16-bit float RGBA
  uint velocity; // packed 16-bit float XY
  float lifetime;
};

layout(std430, binding = 0) readonly restrict buffer ParticlesBuffer
{
  Particle list[];
}particles;

layout(std430, binding = 1) readonly restrict buffer RenderIndicesBuffer
{
  int size;
  int indices[];
}renderIndices;

// due to GLSL limitations, I gotta do this
layout(binding = 0, r32ui) restrict uniform uimage2D i_target_r;
layout(binding = 1, r32ui) restrict uniform uimage2D i_target_g;
layout(binding = 2, r32ui) restrict uniform uimage2D i_target_b;

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
void main()
{
  uint index = gl_GlobalInvocationID.x;
  if (index >= renderIndices.size)
    return;

  int indexIndex = renderIndices.indices[index];
  Particle particle = particles.list[indexIndex];

  ivec2 targetDim = imageSize(i_target_r);
  // [-1, 1) -> [0, imageDim)
  ivec2 uv = ivec2(((particle.position + 1.0) / 2.0) * vec2(targetDim));
  if (any(greaterThanEqual(uv, targetDim)) || any(lessThan(uv, ivec2(0))))
    return;
  
  vec4 color = vec4(unpackHalf2x16(particle.emissive.x), unpackHalf2x16(particle.emissive.y));
  if (particle.lifetime < 1.0) color *= particle.lifetime;
  uvec4 colorQuantized = uvec4(color * 256.0 + 0.5);
  imageAtomicAdd(i_target_r, uv, colorQuantized.r);
  imageAtomicAdd(i_target_g, uv, colorQuantized.g);
  imageAtomicAdd(i_target_b, uv, colorQuantized.b);
}