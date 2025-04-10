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


	// View direction (from fragment to camera)
	vec3 V = -gl_WorldRayDirectionEXT;

	// Halfway vector
	vec3 H = normalize(L + V);
	// Material of the object
	
	GLTFMaterialData mat    =materialData.data[gl_InstanceCustomIndexEXT];



	vec2 uv = v0.uv  * barycentrics.x + v1.uv * barycentrics.y + v2.uv  * barycentrics.z;


	vec4 vertexColor = v0.color  * barycentrics.x + v1.color * barycentrics.y + v2.color  * barycentrics.z;

	vec3 baseColor = vertexColor.rgb* mat.colorFactors.rgb;
	if (mat.colorIdx != 1){
		baseColor = baseColor*  texture(textureSamplers[mat.colorIdx], uv).rgb ;
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




	// Combine components    



	vec3  specular    = vec3(0);
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

		if(isShadowed)
		{
			attenuation = 0.3;
		}
		else
		{
			// Specular
			// Specular component
			float specAngle = max(dot(worldNrm, H), 0.0);
			float spec = pow(specAngle, shininess);

			// Specular color (adjusted by metallic factor)
			vec3 specularColor = mix(vec3(0.04), baseColor, metallic);
			specular = spec * lightColor * specularColor;
		}
	}
	
	// Perfect specular reflection
	vec3 R = reflect(-V, worldNrm);
	vec3 specularReflection = vec3(0);
	
	if (prd.depth < 1){
		float tMin   = 0.001;
		float tMax   = 1000000000.0;
		vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
		vec3  rayDir = R;
		uint  flags  = gl_RayFlagsNoneEXT;
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
		// We should now have the color of the reflected light. The intensity of this light is based on the fresnel equation
		vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    	float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;  // Use direct lighting formula

		float NDF = DistributionGGX(worldNrm, H, k);
		float G   = GeometrySmith(worldNrm, V, L, k);
		vec3 F = Schlick(normalize(worldNrm), normalize(V), 1.0, 1.3) * F0;
		vec3 radiance = lightColor;
		vec3 kS = F;

		vec3 kD = vec3(1.0) - kS;
		
		kD *= 1.0 - metallic;

		float NdotV = max(dot(worldNrm, V), 0.0001);
		float NdotR = max(dot(worldNrm, R), 0.0001);
		float NdotL = max(dot(worldNrm, L), 0.0000);

		vec3 spec = F * G * NDF / max(4 * NdotV * NdotR, 0.001);
		specularReflection = spec;
		diffuse = kD  * lightColor * baseColor;	
		prd.depth--;

		prd.hitValue = ambient+(kD * baseColor / PI + spec) * radiance * NdotL;
		return;
	}

  	// Apply shadow
	if (isShadowed)
	{
		diffuse  *= 0.0;
		specular *= 0.0;
	}

	vec3 result = ambient + diffuse + specularReflection;

	prd.hitValue = result;
}
