#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension  GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "input_structures.glsl"
#include "common/host_device.h"
layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outVPos;
layout (location = 4) out vec4 lightFragPos;
layout (location = 5) out mat3 outTBN;

layout(set = 1, binding = 0, scalar) uniform _GLTFMaterialData {GLTFMaterialData materialData;};


layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

//push constants block
layout( push_constant ) uniform constants
{
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
	int hasTangent;
} PushConstants;

layout (set = 3, binding = 0) uniform DirectionalLight {
	vec3 direction;
	float intensity;
	vec4 color;
	mat4 lightView;
} directionalLight;

void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);

	vec4 worldPos=  PushConstants.render_matrix *position;
    lightFragPos = directionalLight.lightView * worldPos;
    // lightFragPos.y *= 1.0f;
	gl_Position = sceneData.viewproj * worldPos;
	outVPos = worldPos.xyz;
	outNormal = (PushConstants.render_matrix * vec4(v.normal, 0.f)).xyz;
	outColor = v.color.xyz * materialData.colorFactors.xyz;	
	outUV.x = v.uv.x;
	outUV.y = v.uv.y;
	if (PushConstants.hasTangent == 1) {
		vec3 T = normalize(PushConstants.render_matrix * vec4(v.tangent.xyz,0.f)).xyz;
		vec3 N = normalize(PushConstants.render_matrix * vec4(v.normal, 0.f)).xyz;
		vec3 B = normalize(cross(N, T) * v.tangent.w);
		outTBN = mat3(T, B, N);
	}

}