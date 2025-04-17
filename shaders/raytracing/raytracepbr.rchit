#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "../common/host_device.h"
#include "../common/PBR_functions.glsl"
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
	float rayAttenuation = 1.0;
	if (prd.depth > 0){
		rayAttenuation = 1.0 / distanceFromOrigin * distanceFromOrigin;
	}


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


	// View direction (from fragment to camera)
	vec3 V = -gl_WorldRayDirectionEXT;

	// Halfway vector
	vec3 H = normalize(L + V);
	// Material of the object
	
	GLTFMaterialData mat    =materialData.data[gl_InstanceCustomIndexEXT]; 



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


	// Light properties
	vec3 lightColor = pcRay.lightColor * lightIntensity;

	vec3 tmpAmbient = vec3(0.2);

	// Ambient component
	vec3 ambient = pcRay.ambientColor.xyz * baseColor;

	// Diffuse component
	float diff = max(dot(worldNrm, L), 0.0);
	vec3 diffuse = diff * lightColor * baseColor;



	float attenuation = 1;
	isShadowed   = true; 

	// Tracing shadow ray only if the light is visible from the surface
	if(dot(worldNrm, L) > 0)
	{
		float tMin   = 0.001;
		float tMax   = lightDistance;
		vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
		vec3  rayDir = L;
		uint  flags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
		traceRayEXT(topLevelAS,  // acceleration structure
					flags,       // rayFlags
					0xFF,        // cullMask
					0,           // sbtRecordOffset
					0,           // sbtRecordStride
					1,           // missIndex
					origin,      // ray origin
					tMin,        // ray min range
					rayDir,      // ray direction
					tMax,        // ray max range
					1            // payload is isShadowed
		);
	}
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
	
	// Perfect specular reflection
	vec3 R = normalize(reflect(-V, worldNrm));
	vec3 directContribution = vec3(0);
	vec3 reflectanceContribution = vec3(0);

	if (prd.depth < 1){
		float tMin   = 0.001;
		float tMax   = 1000000000.0;
		vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
		vec3  rayDir = R;
		uint  flags  = gl_RayFlagsNoneEXT;

		// Compute Fresnel attenuation
		vec3 F = fresnelSchlick(max(dot(worldNrm, V), 0.0), F0);
		float materialAttenuation = mix(1.0 - metallic, 1.0, roughness); // Higher roughness loses more energy

		// Apply attenuation from previous bounces
		prd.attenuation *= F * materialAttenuation; 

		prd.depth++;
		prd.hitValue = vec3(0);
		traceRayEXT(topLevelAS,  // acceleration structure
					flags,       // rayFlags 
					0xFF,        // cullMask
					0,           // sbtRecordOffset
					0,           // sbtRecordStride
					0,           // missIndex
					origin,      // ray origin
					tMin,        // ray min range
					rayDir,      // ray direction
					tMax,        // ray max range
					0            // payload is prd
		);
		
		reflectanceContribution *= F0;
	}  

	

	if (!isShadowed){
	    directContribution = CalculatePBR(worldNrm,V,L,baseColor, lightColor, lightIntensity * rayAttenuation, F0, metallic, roughness);
	}

	

	// vec3 result = normalize(R);
	vec3 result = emission+  reflectanceContribution + directContribution;
	// result = result / (result + vec3(1.0));
    // vec3 result = vec3(0.0, roughness, 0.0); 
	prd.hitValue = result;
}
