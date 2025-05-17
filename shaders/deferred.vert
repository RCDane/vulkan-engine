#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension  GL_EXT_shader_explicit_arithmetic_types_int64 : require
#include "common/PBR_functions.glsl"

#include "input_structures.glsl"
#include "common/host_device.h"
#include "common/util.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outVPos;
layout (location = 4) out mat3 outTBN;
layout (location = 7) out float depth;
layout(set = 1, binding = 0, scalar) uniform _GLTFMaterialData {GLTFMaterialData materialData;};


layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

//push constants block
layout(push_constant, scalar) uniform constants {
    mat4 render_matrix;
    VertexBuffer vertexBuffer;
    int hasTangent;
    int useCPF;
} PushConstants;


void main() 
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);

	vec4 worldPos=  PushConstants.render_matrix *position;
	dvec4 screen_space = sceneData.viewproj * worldPos;
	depth = float(LinearizeDepth(screen_space.z));
	gl_Position = vec4(screen_space);
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