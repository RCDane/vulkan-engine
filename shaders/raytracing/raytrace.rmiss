
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../common/host_device.h"
layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(set = 1, binding=4) uniform samplerCube cubeMap;

layout(push_constant) uniform _PushConstantRay
{
  PushConstantRay pcRay;
};

void main()
{
  prd.hitValue = pow(texture(cubeMap, normalize(gl_WorldRayDirectionEXT)).rgb, vec3(2.2));
  prd.done = 1;
  // prd.attenuation = vec3(1.0); // TODO: How should I handle attenuation here?
}
