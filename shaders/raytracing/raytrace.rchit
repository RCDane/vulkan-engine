/*
 * Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2019-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "../common/raycommon.glsl"
#include "../common/wavefront.glsl"

hitAttributeEXT vec2 attribs;
 
// clang-format off
layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(buffer_reference, std430) buffer Vertices {Vertex v[]; }; // Positions of an object
layout(buffer_reference, scalar) buffer Indices {int i[]; }; // Triangle indices
// layout(buffer_reference, scalar) buffer Materials {WaveFrontMaterial m[]; }; // Array of all materials on an object
// layout(buffer_reference, scalar) buffer MatIndices {int i[]; }; // Material ID for each triangle
layout(set = 0, binding = eTlas) uniform accelerationStructureEXT topLevelAS;
layout(set = 1, binding = eObjDescs, scalar) buffer ObjDesc_ { ObjDesc i[]; } objDesc;
layout(set = 1, binding = eTextures) uniform sampler2D textureSamplers[];



layout(set = 1, binding = 3, scalar) buffer MaterialData_ {GLTFMaterialData data[];} materialData;


layout(push_constant) uniform _PushConstantRay { PushConstantRay pcRay; };
// clang-format on
vec3 getColorFromInstanceIndex(int index) {
    // Normalize the index to a range [0, 1]
    float normalizedIndex = float(index % 256) / 255.0;

    // Generate a color based on the normalized index
    vec3 color = vec3(
        sin(normalizedIndex * 3.14159 * 2.0),
        cos(normalizedIndex * 3.14159 * 2.0),
        sin(normalizedIndex * 3.14159)
    );

    // Ensure the color is in the range [0, 1]
    color = clamp(color, 0.0, 1.0);

    return color;
}

void main()
{
   

  // Object data
  ObjDesc    objResource = objDesc.i[gl_InstanceCustomIndexEXT];
  Indices    indices     = Indices(objResource.indexAddress);
  Vertices   vertices    = Vertices(objResource.vertexAddress);
  int idx = gl_PrimitiveID*3;
  int ind0 = indices.i[idx];
  int ind1 = indices.i[idx+1];
  int ind2 = indices.i[idx+2];
  
  // Vertex of the triangle
  Vertex v0 = vertices.v[ind0];
  Vertex v1 = vertices.v[ind1];
  Vertex v2 = vertices.v[ind2];



  const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

  // Computing the coordinates of the hit position
  const vec3 pos      = v0.position * barycentrics.x + v1.position * barycentrics.y + v2.position * barycentrics.z;
  const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));  // Transforming the position to world space

  // Computing the normal at hit position
  const vec3 nrm      = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
  const vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));  // Transforming the normal to world space
  
  
  // Vector toward the light
  vec3  L;
  float lightIntensity = pcRay.lightIntensity;
  float lightDistance  = 100000.0;
  // Point light
  if(pcRay.lightType == 0)
  {
    vec3 lDir      = pcRay.lightPosition - worldPos;
    lightDistance  = length(lDir);
    lightIntensity = pcRay.lightIntensity / (lightDistance * lightDistance);
    L              = normalize(lDir);
  } 
  else  // Directional light
  {
    L = -normalize(pcRay.lightPosition);
  }

  // Material of the object
  
  GLTFMaterialData mat    =materialData.data[gl_InstanceCustomIndexEXT];

  vec2 uv = v0.uv  * barycentrics.x + v1.uv * barycentrics.y + v2.uv  * barycentrics.z;


  vec3 baseColor = texture(textureSamplers[mat.colorIdx], uv).rgb * mat.colorFactors.rgb;

  prd.hitValue = baseColor * lightIntensity * max(dot(worldNrm, L), 0.0);
  // prd.hitValue = vec3(1.0 * lightIntensity * max(dot(worldNrm, L), 0.0));
  return;

  // // Diffuse
  // vec3 diffuse = computeDiffuse(mat, L, worldNrm);
  // if(mat.textureId >= 0)
  // {
  //   uint txtId    = mat.textureId + objDesc.i[gl_InstanceCustomIndexEXT].txtOffset;
  //   vec2 texCoord = v0.uv  * barycentrics.x + v1.uv * barycentrics.y + v2.uv  * barycentrics.z;
  //   diffuse *= texture(textureSamplers[nonuniformEXT(txtId)], texCoord).xyz;
  // }

  // vec3  specular    = vec3(0);
  // float attenuation = 1;

  // // Tracing shadow ray only if the light is visible from the surface
  // if(dot(worldNrm, L) > 0)
  // {
  //   float tMin   = 0.001;
  //   float tMax   = lightDistance;
  //   vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
  //   vec3  rayDir = L;
  //   uint  flags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
  //   isShadowed   = true;
  //   traceRayEXT(topLevelAS,  // acceleration structure
  //               flags,       // rayFlags
  //               0xFF,        // cullMask
  //               0,           // sbtRecordOffset
  //               0,           // sbtRecordStride
  //               1,           // missIndex
  //               origin,      // ray origin
  //               tMin,        // ray min range
  //               rayDir,      // ray direction
  //               tMax,        // ray max range
  //               1            // payload (location = 1)
  //   );

  //   if(isShadowed)
  //   {
  //     attenuation = 0.3;
  //   }
  //   else
  //   {
  //     // Specular
  //     specular = computeSpecular(mat, gl_WorldRayDirectionEXT, L, worldNrm);
  //   }
  // }

  // prd.hitValue = vec3(lightIntensity * attenuation * (diffuse + specular));
}
