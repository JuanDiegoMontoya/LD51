#version 460 core

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec4 a_color;

layout(std140, binding = 0) uniform CameraBuffer
{
  mat4 viewProj;
}cameraUniforms;

layout(location = 0) out vec4 v_color;

void main()
{
  v_color = a_color;

  gl_Position = cameraUniforms.viewProj * vec4(a_position, 0.5, 1.0);
}