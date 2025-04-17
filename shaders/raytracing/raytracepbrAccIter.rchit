#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "../common/host_device.h"
#include "../common/PBR_functions.glsl"
#include "../common/util.glsl"
hitAttributeEXT vec2 attribs;
 
// clang-format off
layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(buffer_reference, std430) buffer Vertices {Vertex v[]; }; // Positions of an object
layout(buffer_reference, scalar) buffer Indices {int i[]; }; // Triangle indices
layout(set = 0, binding = eTlas) uniform accelerationStructureEXT topLevelAS;
layout(set = 1, binding = eObjDescs, scalar) buffer ObjDesc_ { ObjDesc i[]; } objDesc;
layout(set = 1, binding = eTextures) uniform sampler2D textureSamplers[];



layout(set = 1, binding = 3, scalar) buffer MaterialData_ {GLTFMaterialData data[];} materialData;
layout(set = 1, binding=4) uniform samplerCube cubeMap;

layout(push_constant) uniform _PushConstantRay { PushConstantRay pcRay; };

vec3 computeMappedNormal(Vertex v0, Vertex v1, Vertex v2, vec3 barycentrics, GLTFMaterialData mat)
{
    // Interpolate UV coordinates.
    vec2 uv = v0.uv * barycentrics.x +
              v1.uv * barycentrics.y +
              v2.uv * barycentrics.z;
              
    // Interpolate the vertex normal and transform to world space.
    vec3 nrm = v0.normal * barycentrics.x +
               v1.normal * barycentrics.y +
               v2.normal * barycentrics.z;
    vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));
    
    // Compute edges and UV deltas.
    vec3 edge1 = v1.position - v0.position;
    vec3 edge2 = v2.position - v0.position;
    vec2 deltaUV1 = v1.uv - v0.uv;
    vec2 deltaUV2 = v2.uv - v0.uv;
    
    // Compute tangent and bitangent.
    float f = 1.0 / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
    vec3 tangent   = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
    vec3 bitangent = f * (-deltaUV2.x * edge1 + deltaUV1.x * edge2);
    
    // Transform the tangent frame from object to world space.
    tangent   = normalize(vec3(gl_ObjectToWorldEXT * vec4(tangent, 0.0)));
    bitangent = normalize(vec3(gl_ObjectToWorldEXT * vec4(bitangent, 0.0)));
    
    // Sample and remap the normal from the texture.
    vec3 sampledNormal = texture(textureSamplers[mat.normalIdx], uv).rgb;
    sampledNormal = sampledNormal * 2.0 - 1.0;
    
    // Construct the final mapped normal in world space.
    return normalize(tangent * sampledNormal.x +
                     bitangent * sampledNormal.y +
                     worldNrm * sampledNormal.z);
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

    vec3 fromOrigin = prd.rayOrigin - worldPos;
	float distanceFromOrigin = length(fromOrigin);
	vec3 rayAttenuation = vec3(1.0);
	if (prd.depth > 0){
		rayAttenuation = prd.attenuation * (1.0 / distanceFromOrigin * distanceFromOrigin);
	}


	// Computing the normal at hit position
	const vec3 nrm      = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
	vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));  // Transforming the normal to world space
	
	
  	vec3  L;
	float lightIntensity = pcRay.lightIntensity;
	float lightDistance  = 100000.0;
	vec3 lightColor = pcRay.lightColor;
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

	

	// View direction (from fragment to camera)
	vec3 V = -gl_WorldRayDirectionEXT;

	// Halfway vector
	vec3 H = normalize(L + V);
	// Material of the object
	
	GLTFMaterialData mat    =materialData.data[gl_InstanceCustomIndexEXT]; 

	if (mat.normalIdx != 2){
       	// Calculate normal using normal texture.
	 	worldNrm = computeMappedNormal(v0, v1, v2, barycentrics, mat);

    }

	vec2 uv = v0.uv  * barycentrics.x + v1.uv * barycentrics.y + v2.uv  * barycentrics.z;


	vec4 vertexColor = v0.color  * barycentrics.x + v1.color * barycentrics.y + v2.color  * barycentrics.z;
	vec3 emission = vec3(0.0);
	if (mat.emissiveIdx != 3){
		emission = SRGBtoLINEAR(texture(textureSamplers[mat.emissiveIdx], uv).rgb);
		
	}

	vec3 baseColor = vertexColor.rgb* mat.colorFactors.rgb;
	if (mat.colorIdx != 0){
		baseColor = baseColor*  SRGBtoLINEAR(texture(textureSamplers[mat.colorIdx], uv).rgb);
	}


	// Roughness and metallic factors
	float roughness = mat.metal_rough_factors.y;
	float metallic = mat.metal_rough_factors.x;
	
	// If a metal roughness texture is bound
	if (mat.metalIdx != 1){
		// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_material_pbrmetallicroughness_metallicroughnesstexture
		// The GLTF spec defines that metalness is stored in the B channel
		// while the roughness is stored in the G channel of the MetallicRougnessTexture
		vec2 metalRough = texture(textureSamplers[mat.metalIdx], uv).gb;
		roughness = metalRough.x;
		metallic = metalRough.y;
	}   

	// Shininess exponent for Blinn-Phong
	float shininess = max((1.0 - roughness) * 128.0, 1.0);

	vec3 tmpColor = vec3(1.0);

    vec3 F0 = mix(vec3(0.04), baseColor.xyz, metallic);
	isShadowed   = true; 
    float tMin   = 0.001;
    float tMax   = lightDistance;
    vec3  rayOrigin = worldPos;
    vec3  rayDir = L;
    uint  flags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    traceRayEXT(topLevelAS,  // acceleration structure
        flags,       // rayFlags
        0xFF,        // cullMask
        0,           // sbtRecordOffset
        0,           // sbtRecordStride
        1,           // missIndex
        rayOrigin,      // ray origin
        tMin,        // ray min range
        rayDir,      // ray direction
    	tMax,        // ray max range
        1            // payload is isShadowed
    );
	
	
	vec3 result = vec3(0);
	if (!isShadowed){
	    result += CalculatePBR(worldNrm,V,L,baseColor, lightColor, lightIntensity, F0, metallic, roughness);
	}

	prd.rayOrigin = worldPos;
	prd.normal = worldNrm;
	prd.attenuation = rayAttenuation * result;
	prd.hitValue = result;
}
