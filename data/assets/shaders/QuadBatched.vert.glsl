#version 460 core

struct ObjectUniforms
{
  mat3x2 transform;
  uint tint4x8;
  uint _padding;
};

layout(std140, binding = 0) uniform CameraBuffer
{
  mat4 viewProj;
}cameraUniforms;

layout(std430, binding = 0) readonly restrict buffer UniformBuffer
{
  ObjectUniforms objectUniforms[];
};

layout(location = 0) out vec2 v_uv;
layout(location = 1) out flat uint v_tint4x8;

// vertices in [0, 1]
vec2 CreateQuad(in uint vertexID) // triangle fan
{
  uint b = 1 << vertexID;
  return vec2((0x9 & b) != 0, (0x3 & b) != 0);
}

void main()
{
  v_uv = CreateQuad(gl_VertexID);
  vec2 aPos = v_uv - 0.5;

  ObjectUniforms object = objectUniforms[gl_InstanceID + gl_BaseInstance];
  v_tint4x8 = object.tint4x8;
  mat3x2 transform3x2 = object.transform;
  vec2 wPos = transform3x2 * vec3(aPos, 1.0);

  gl_Position = cameraUniforms.viewProj * vec4(wPos, 0.5, 1.0);
}