#version 460 core

layout(location = 0) uniform sampler2DArray s_texture;

layout(location = 0) in Varyings
{
  vec2 texCoord;
  flat uint spriteIndex;
  flat uint tint4x8;
}fs_in;

layout(location = 0) out vec4 o_color;

void main()
{
  vec4 color = textureLod(s_texture, vec3(fs_in.texCoord, fs_in.spriteIndex), 0.0);
  color *= unpackUnorm4x8(fs_in.tint4x8);
  o_color = color;
}