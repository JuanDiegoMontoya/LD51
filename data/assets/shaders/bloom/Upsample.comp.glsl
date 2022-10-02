#version 460 core

layout(binding = 0) uniform sampler2D s_source;
layout(binding = 1) uniform sampler2D s_targetRead;
layout(binding = 0) uniform writeonly image2D i_targetWrite;

layout(binding = 0, std140) uniform UniformBuffer
{
  ivec2 sourceDim;
  ivec2 targetDim;
  float width;
  float strength;
  float sourceLod;
  float targetLod;
}uniforms;

layout(local_size_x = 16, local_size_y = 16) in;
void main()
{
  ivec2 gid = ivec2(gl_GlobalInvocationID.xy);

  if (any(greaterThanEqual(gid, uniforms.targetDim)))
    return;

  vec2 texel = 1.0 / uniforms.sourceDim;

  // center of written pixel
  vec2 uv = (vec2(gid) + 0.5) / uniforms.targetDim;

  vec4 rgba = texelFetch(s_targetRead, gid, int(uniforms.targetLod));
  //vec4 rgba = textureLod(s_targetRead, uv, 0);

  vec4 blurSum = vec4(0);
  blurSum += textureLod(s_source, uv + vec2(-1, -1) * texel * uniforms.width, uniforms.sourceLod) * 1.0 / 16.0;
  blurSum += textureLod(s_source, uv + vec2(0, -1)  * texel * uniforms.width, uniforms.sourceLod) * 2.0 / 16.0;
  blurSum += textureLod(s_source, uv + vec2(1, -1)  * texel * uniforms.width, uniforms.sourceLod) * 1.0 / 16.0;
  blurSum += textureLod(s_source, uv + vec2(-1, 0)  * texel * uniforms.width, uniforms.sourceLod) * 2.0 / 16.0;
  blurSum += textureLod(s_source, uv + vec2(0, 0)   * texel * uniforms.width, uniforms.sourceLod) * 4.0 / 16.0;
  blurSum += textureLod(s_source, uv + vec2(1, 0)   * texel * uniforms.width, uniforms.sourceLod) * 2.0 / 16.0;
  blurSum += textureLod(s_source, uv + vec2(-1, 1)  * texel * uniforms.width, uniforms.sourceLod) * 1.0 / 16.0;
  blurSum += textureLod(s_source, uv + vec2(0, 1)   * texel * uniforms.width, uniforms.sourceLod) * 2.0 / 16.0;
  blurSum += textureLod(s_source, uv + vec2(1, 1)   * texel * uniforms.width, uniforms.sourceLod) * 1.0 / 16.0;
  rgba += blurSum * uniforms.strength;

  imageStore(i_targetWrite, gid, rgba);
}