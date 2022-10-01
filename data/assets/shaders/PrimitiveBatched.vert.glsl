#version 460 core

struct InstanceUniforms
{
  mat3x2 model;
  uint color;
  uint _padding;
};

layout(location = 0) in vec2 a_position;

layout(std140, binding = 0) uniform CameraBuffer
{
  mat4 viewProj;
}cameraUniforms;

layout(std430, binding = 0) restrict readonly buffer InstanceData
{
  InstanceUniforms instances[];
};

layout(location = 0) out vec4 v_color;

void main()
{
  InstanceUniforms instance = instances[gl_InstanceID + gl_BaseInstance];

  v_color = unpackUnorm4x8(instance.color);
  vec2 wPos = instance.model * vec3(a_position, 1.0);
  gl_Position = cameraUniforms.viewProj * vec4(wPos, 0.5, 1.0);
}