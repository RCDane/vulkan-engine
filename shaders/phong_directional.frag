#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 vPos;
layout (location = 4) in vec4 lightFragPos;
layout (location = 0) out vec4 outFragColor;

layout (set = 3, binding = 0) uniform DirectionalLight {
	vec3 direction;
	float intensity;
	vec4 color;
	mat4 lightView;
} directionalLight;

layout (set = 3, binding = 1) uniform sampler2D shadowMap;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform to [0,1] range
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // If outside shadow map bounds, return no shadow
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 1.0;
    }

    // Retrieve closest depth value from shadow map
    float shadowDepth = texture(shadowMap, projCoords.xy).r;

    // Current depth
    float currentDepth = projCoords.z;

    // Bias to prevent shadow acne
    float bias = max(0.0005 * (1.0 - dot(normalize(inNormal), normalize(-directionalLight.direction))), 0.0005);
    // float bias = 0.0005;
    // Perform shadow test
    float shadow = currentDepth < shadowDepth - bias ? 0.0 : 1.0;

    return shadow;
}

void main()
{
    // Normalize input normal
    vec3 N = normalize(inNormal);

    // Light direction (from fragment to light)
    vec3 L = normalize(-directionalLight.direction);

    // View direction (from fragment to camera)
    vec3 V = normalize(sceneData.cameraPosition.xyz - vPos);

    // Halfway vector
    vec3 H = normalize(L + V);

    // Material properties
    vec3 baseColor = inColor * texture(colorTex, inUV).rgb * materialData.colorFactors.rgb;

    // Roughness and metallic factors
    float roughness = materialData.metal_rough_factors.y;
    float metallic = materialData.metal_rough_factors.x;

    // Shininess exponent for Blinn-Phong
    float shininess = max((1.0 - roughness) * 128.0, 1.0);

    // Light properties
    vec3 lightColor = directionalLight.color.rgb * directionalLight.intensity;

    // Ambient component
    vec3 ambient = sceneData.ambientColor.rgb * baseColor;

    // Diffuse component
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * lightColor * baseColor;

    // Specular component
    float specAngle = max(dot(N, H), 0.0);
    float spec = pow(specAngle, shininess);

    // Specular color (adjusted by metallic factor)
    vec3 specularColor = mix(vec3(0.04), baseColor, metallic);

    vec3 specular = spec * lightColor * specularColor;

    // Shadow factor
    float shadow = ShadowCalculation(lightFragPos);

    // Apply shadow
    diffuse *= shadow;
    specular *= shadow;

    // Combine components
    vec3 result = ambient + diffuse + specular;
    // if (shadow > 0.0){
    //     result = vec3(1.0);
    // }

    // Output final color with alpha from material data
    outFragColor = vec4(result, materialData.colorFactors.a);
}