#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension  GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_scalar_block_layout : require

#include "input_structures.glsl"
#include "common/host_device.h"
#include "common/PBR_functions.glsl"
#include "common/util.glsl"
layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 vPos;
layout (location = 4) in mat3 inTBN;
layout (location = 7) in float depth;


layout (location = 0) out vec4 outAlbedo;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec3 outMaterial;
layout (location = 3) out vec4 outEmissive;



layout(set = 1, binding = 0, scalar) uniform _GLTFMaterialData {GLTFMaterialData materialData;};


layout(buffer_reference, std430) readonly buffer VertexBuffer{ 
	Vertex vertices[];
};

layout(set = 2, binding=0) uniform sampler2D[] textureSamplers;
// layout(set = 2, binding=1) uniform samplerCube cubeMap;

//push constants block
layout( push_constant,scalar ) uniform constants
{
	mat4 render_matrix;
	VertexBuffer vertexBuffer;
	int hasTangent;
    int useCPF;
    int objectID;
} PushConstants;
 #define EPSILON 0.00001

// // Taken from:http://www.thetenthplanet.de/archives/1180
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
        TBN = cotangent_frame(normal, vPos, uv);
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
        mat3 TBN = calculate_TBN(normalize(inNormal), inUV);
        vec3 textureNormal = texture(textureSamplers[materialData.normalIdx], inUV).xyz;  

        textureNormal = normalize(textureNormal * 2.0 - 1.0);
        N = normalize(TBN * textureNormal);

    }
    // double outViewZ = (-vPos.z);
    // double far = zFar;
    outNormal = vec4(ndir_to_oct_unorm(N), length(fwidth(N)), 0.0);        



    outAlbedo = vec4(inColor,1.0);
    if (materialData.colorIdx != 0){
        vec4 sampledColor = texture(textureSamplers[materialData.colorIdx], inUV);
       
        vec3 baseColor = SRGBtoLINEAR(sampledColor.rgb);

        outAlbedo = vec4(baseColor,sampledColor.a);
    }


    // Material properties
    

    vec3 emission = vec3(0);
    if (materialData.emissiveIdx != 3){
        emission = SRGBtoLINEAR(texture(textureSamplers[materialData.emissiveIdx], inUV).xyz);
    }
    // outEmissive = vec4(emission, 1.0);
    outEmissive = vec4(vPos, 1.0);
    



    // // Roughness and metallic factors
    float roughness = materialData.metal_rough_factors.y;
    float metallic = materialData.metal_rough_factors.x;

    // If a metal roughness texture is bound
    if (materialData.metalIdx != 1){
        // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_material_pbrmetallicroughness_metallicroughnesstexture
        // The GLTF spec defines that metalness is stored in the B channel
        // while the roughness is stored in the G channel of the MetallicRougnessTexture
        vec2 metalRough = texture(textureSamplers[materialData.metalIdx], inUV).gb;
        roughness *= metalRough.x;
        metallic *= metalRough.y;
    }   

    outMaterial = vec3(roughness,metallic, float(PushConstants.objectID));
     if (outAlbedo.a < 0.5){
            discard;
        }
}