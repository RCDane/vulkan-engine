#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 vPos;
layout (location = 0) out vec4 outFragColor;

void main() 
{
	// vec3 lightDirection = sceneData.sunlightDirection.xyz;

	// vec3 pointLightDirection = normalize(pointLight.position - vPos);
	// vec3 viewDir = normalize(sceneData.cameraPosition.xyz - vPos);
	// vec3 hafwayDir = normalize(pointLightDirection + viewDir);

	// float d = length(pointLight.position - vPos);
	// float attenuation = 1.0/(1.0 + 0.0027 * d + (d*d)*0.000028);

	// vec3 pointLightColor = pointLight.color.xyz * pointLight.intensity;

	// vec3 diffuse =  pointLightColor * max(dot(inNormal, pointLightDirection), 0.0f);

	// vec3 spec = pointLightColor*pow(max(dot(inNormal, hafwayDir), 0.0), materialData.metal_rough_factors.y);
	
	
	// vec3 ambient = sceneData.ambientColor.xyz;
	// vec3 specular =   spec;
	
	
	// vec3 color = inColor * texture(colorTex,inUV).xyz;

	// vec3 result = (ambient*attenuation + specular*attenuation + diffuse*attenuation) * color;


	// outFragColor = vec4(result ,1.0f);
}