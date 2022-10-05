#version 460 core

layout(location = 0) in vec2 v_uv;

layout(binding = 0) uniform usampler2D s_target_r;
layout(binding = 1) uniform usampler2D s_target_g;
layout(binding = 2) uniform usampler2D s_target_b;

layout(location = 0) out vec4 o_color;

void main()
{
  o_color = vec4(
    min(float(texture(s_target_r, v_uv).r) / 256.0, 64000),
    min(float(texture(s_target_g, v_uv).r) / 256.0, 64000),
    min(float(texture(s_target_b, v_uv).r) / 256.0, 64000),
    1.0
  );
}