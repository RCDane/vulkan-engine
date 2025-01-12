#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension  GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_scalar_block_layout : require

#include "input_structures.glsl"
#include "common/host_device.h"
layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 vPos;
layout (location = 4) in vec4 lightFragPos;
layout (location = 5) in mat3 inTBN;

layout (location = 0) out vec4 outFragColor;

layout (set = 3, binding = 0) uniform DirectionalLight {
	vec3 direction;
	float intensity;
	vec4 color;
	mat4 lightView;
} directionalLight;

layout (set = 3, binding = 1) uniform sampler2D shadowMap;

layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

layout(set = 4, binding=0) uniform sampler2D[] textureSamplers;

//push constants block
layout( push_constant,scalar ) uniform constants
{
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
	int hasTangent;
    int useCPF;
} PushConstants;
 #define EPSILON 0.00001
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal)
{
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform to [0,1] range
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    float z = 0.5 * projCoords.z + 0.5;
    // If outside shadow map bounds, return no shadow
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z < 0.0 || projCoords.z > 1.0)
    {
        return 1.0;
    }

    // Current depth
    float currentDepth = projCoords.z;

    // Bias to prevent shadow acne
    // float bias = max(0.05 * (1.0 - dot(normalize(normal), normalize(-directionalLight.direction))), 0.05);
    float bias = 0.005;


    float shadow = 0.0;
    if (PushConstants.useCPF == 1){
        // Percentage-closer filtering
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
        
        for (int y = -1 ; y <= 1 ; y++) {
            for (int x = -1 ; x <= 1 ; x++) {
                vec2 Offsets = vec2(x * texelSize.x, y * texelSize.y);
                vec2 UVC = vec2(projCoords.xy + Offsets);
                shadow += texture(shadowMap, UVC.xy).r -bias > currentDepth ? 0.0 : 1.0;
            }
        }
        shadow /= 9.0;
    }
    else {
        // Simple shadows
        float textureDepth = texture(shadowMap, projCoords.xy).r;
        shadow = textureDepth -bias > currentDepth ? 0.0 : 1.0;
    }


    return shadow;
}
// Taken from:http://www.thetenthplanet.de/archives/1180
mat3 inverse3x3( mat3 M ) { 
    // The original was written in HLSL, but this is GLSL, 
    // therefore 
    // - the array index selects columns, so M_t[0] is the 
    // first row of M, etc. 
    // - the mat3 constructor assembles columns, so 
    // cross( M_t[1], M_t[2] ) becomes the first column 
    // of the adjugate, etc. 
    // - for the determinant, it does not matter whether it is 
    // computed with M or with M_t; but using M_t makes it 
    // easier to follow the derivation in the text 
    
    mat3 M_t = transpose( M ); 
    float det = dot( cross( M_t[0], M_t[1] ), M_t[2] ); 
    mat3 adjugate = mat3( 
        cross( M_t[1], M_t[2] ), 
        cross( M_t[2], M_t[0] ), 
        cross( M_t[0], M_t[1] ) ); 
    return adjugate / det; 
}

mat3 cotangent_frame( vec3 N, vec3 p, vec2 uv ) 
{ 
    // get edge vectors of the pixel triangle 
    vec3 dp1 = dFdx( p ); 
    vec3 dp2 = dFdy( p ); 
    vec2 duv1 = dFdx( uv ); 
    vec2 duv2 = dFdy( uv );   
    // solve the linear system 
    vec3 dp2perp = cross( dp2, N ); 
    vec3 dp1perp = cross( N, dp1 ); 
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x; 
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;   
    // construct a scale-invariant frame 
    float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) ); 
    return mat3( T * invmax, B * invmax, N ); 
}

mat3 calculate_TBN(vec3 normal, vec2 uv){
    mat3 TBN;
    if (PushConstants.hasTangent == 0){
        TBN = cotangent_frame(normal, -vPos, uv);
    }
    else{
        TBN = inTBN;
    }
    return TBN;
}

void main()
{
    // Normalize input normal

    vec3 N = normalize(inNormal);
    if (materialData.normalIdx != 2){
       mat3 TBN = calculate_TBN(inNormal, inUV);
       vec3 textureNormal = texture(textureSamplers[materialData.normalIdx], inUV).xyz;
        
       textureNormal = textureNormal * 255./127. - 128./127.;
       N = normalize(TBN * textureNormal);
    }
    
    // Light direction (from fragment to light)
    vec3 L = normalize(-directionalLight.direction);

    // View direction (from fragment to camera)
    vec3 V = normalize(sceneData.cameraPosition.xyz - vPos);

    // Halfway vector
    vec3 H = normalize(L + V);





    // Material properties
    vec3 baseColor = inColor * texture(textureSamplers[materialData.colorIdx], inUV).rgb;



    // Roughness and metallic factors
    float roughness = materialData.metal_rough_factors.y;
    float metallic = materialData.metal_rough_factors.x;

    // If a metal roughness texture is bound
    if (materialData.metalIdx != 1){
        // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_material_pbrmetallicroughness_metallicroughnesstexture
        // The GLTF spec defines that metalness is stored in the B channel
        // while the roughness is stored in the G channel of the MetallicRougnessTexture
        vec2 metalRough = texture(textureSamplers[materialData.metalIdx], inUV).gb;
        roughness = metalRough.x;
        metallic = metalRough.y;
    }   

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
    float shadow = ShadowCalculation(lightFragPos, N);

    // Apply shadow
    diffuse *= shadow;
    specular *= shadow;

    // Combine components
    vec3 result = ambient + diffuse + specular;


    // Output final color with alpha from material data
    outFragColor = vec4(result, materialData.colorFactors.a);
}