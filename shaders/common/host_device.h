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


#ifndef COMMON_HOST_DEVICE
#define COMMON_HOST_DEVICE

#ifdef __cplusplus
#include <glm/glm.hpp>
// GLSL Type
using vec2  = glm::vec2;
using ivec2 = glm::ivec2;
using uvec2 = glm::uvec2;
using vec3  = glm::vec3;
using vec4  = glm::vec4;
using mat4  = glm::mat4;
using uint  = unsigned int;
using u64 = uint64_t;
#endif

// clang-format off
#ifdef __cplusplus // Descriptor binding helper for C++ and GLSL
 #define START_BINDING(a) enum a {
 #define END_BINDING() }
#else
 #define START_BINDING(a)  const uint
 #define END_BINDING() 
#endif

START_BINDING(SceneBindings)
	eGlobals  = 0,  // Global uniform containing camera matrices
	eObjDescs = 1,  // Access to the object descriptions
	eTextures = 2   // Access to textures
END_BINDING();

START_BINDING(RtxBindings)
	eTlas     = 0,  // Top-level acceleration structure
	eOutImage = 1   // Ray tracer output image
END_BINDING();
// clang-format on


// Information of a obj model when referenced in a shader
struct ObjDesc
{
	uint64_t vertexAddress;         // Address of the Vertex buffer
	uint64_t indexAddress;          // Address of the index buffer
	uint64_t materialAddress;       // Address of the material buffer
	uint64_t materialIndexAddress;  // Address of the triangle material index buffer
	uint indexOffset;
	int padding;
};

// Uniform buffer set at each frame
struct GlobalUniforms
{
	mat4 viewProj;     // Camera view * projection
	mat4 viewInverse;  // Camera inverse view matrix
	mat4 projInverse;  // Camera inverse projection matrix
	ivec2 viewPort;
};


struct GPUSceneData {
    mat4 view;
    mat4 proj;
    mat4 viewproj;
    vec4 ambientColor;
    vec4 sunlightDirection; // w for sun power
    vec4 sunlightColor;
	  vec4 cameraPosition;
};
// Push constant structure for the raster
struct PushConstantRaster
{
	mat4  modelMatrix;  // matrix of the instance
	vec3  lightPosition;
	uint  objIndex;
	float lightIntensity;
	int   lightType;
};


// Push constant structure for the ray tracer
struct PushConstantRay
{
	vec4  clearColor;
	vec3  lightPosition;
	float lightIntensity;
	vec3 lightColor;
	int   lightType;
	vec4 ambientColor;
};


struct Vertex {
	vec3 position;
	vec3 normal;
  	vec2 uv;
	vec4 color;
	vec4 tangent;
}; 





#endif
