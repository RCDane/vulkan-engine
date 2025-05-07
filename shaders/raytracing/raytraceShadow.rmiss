
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT bool isShadowed;
// layout(location = 2) rayPayloadInEXT bool isShadowed2;

void main()
{
  isShadowed = false; 
}
