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
layout(set = 1, binding = 5, scalar) buffer LightSources { LightSource lights[]; };
layout(set = 1, binding = 0) uniform _GlobalUniforms { GlobalUniforms uni; };



layout(set = 1, binding = 3, scalar) buffer MaterialData_ {GLTFMaterialData data[];} materialData;
layout(set = 1, binding=4) uniform samplerCube cubeMap;

layout(push_constant) uniform _PushConstantRay { PushConstantRay pcRay; };

struct LightSample {
  vec3 color;
  vec3 direction;
  float intensity;
  float distance;
  vec3 attenuation;
  float pdf;
};


// Sample sphere 
vec3 sampleSphere(vec3 center, float radius, inout uint seed)
{
  vec3 position = vec3(0.0);
  float u = rnd(seed);
  float v = rnd(seed);
  float theta = 2.0 * PI * u;
  float phi = acos(1.0 - 2.0 * v);
  position.x = radius * sin(phi) * cos(theta) + center.x;
  position.y = radius * sin(phi) * sin(theta) + center.y;
  position.z = radius * cos(phi) + center.z;
  return position;  
}

const float INF_DISTANCE = 1e20;

LightSample sampleLightsPdf(vec3 hitPoint, inout uint seed) {
    // 1) Pick one light uniformly
    uint i = randRange(seed, 0, uni.raytracingSettings.lightCount - 1);
    LightSource Ls = lights[i];
    float selectPdf = 1.0 / float(uni.raytracingSettings.lightCount);  // P(l)

    LightSample ls;
    ls.pdf = selectPdf;         // pdf of the light source
    ls.color = Ls.color;           // base color of the light
    ls.intensity = Ls.intensity;   // scalar intensity

    if (Ls.type == 0) {

        vec3 samplePos = sampleSphere(Ls.position, Ls.radius, seed);

        vec3 toLight   = samplePos - hitPoint;
        float dist     = length(toLight);
        vec3 dir       = normalize(toLight);

        float area     = 4.0 * PI * Ls.radius * Ls.radius;
        float areaPdf  = 1.0 / area;

        ls.pdf = ls.pdf * areaPdf;

        ls.intensity = ls.intensity / (area*PI); // scale intensity by area pdf
        ls.attenuation =  vec3(1)/(dist * dist);

        ls.direction = dir;
        ls.distance  = dist;
    } else {
        // directional (sun) light with finite sunAngle for soft shadows

        // 1) base direction (w) is the normalized light direction
        vec3 w = normalize(Ls.direction);

        // 2) branchless ONB (Duff et al. 2017)
        float s = (w.z >= 0.0 ? 1.0 : -1.0);
        float a = -1.0 / (s + w.z);
        float b = w.x * w.y * a;
        vec3 u  = vec3(1.0 + s * w.x * w.x * a, s * b, -s * w.x);
        vec3 v  = vec3(b, s + w.y * w.y * a, -w.y);

        // 3) sample a direction within the cone of half-angle sunAngle
        float cosMax = cos(Ls.sunAngle);
        float u1 = rnd(seed);
        float u2 = rnd(seed);
        float z  = mix(cosMax, 1.0, u1);
        float r  = sqrt(max(0.0, 1.0 - z*z));
        float phi = 2.0 * PI * u2;
        vec2 d    = vec2(cos(phi)*r, sin(phi)*r);

        // 4) build sample vector in local ONB space
        vec3 localDir = vec3(d.x, d.y, z);

        // 5) rotate into world space
        vec3 sampleDir = normalize(u * localDir.x + v * localDir.y + w * localDir.z);

        // 6) set output
        ls.direction   = sampleDir;
        ls.distance    = INF_DISTANCE;
        ls.attenuation = vec3(1.0);

        // 7) account for cone PDF
        float coneSolidAngle = 2.0 * PI * (1.0 - cosMax);
        float conePdf        = 1.0 / coneSolidAngle;
        ls.pdf = selectPdf * conePdf;
    }

    return ls;
}


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


	// Computing the normal at hit position
	const vec3 nrm      = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
	vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));  // Transforming the normal to world space
	
	
	uint i = randRange(prd.seed, 0, 3);
	LightSample l_Sample = sampleLightsPdf(worldPos, prd.seed);

	vec3 L = l_Sample.direction; // Light direction (from fragment to light)
	

	

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

	vec3 tmpColor = vec3(1.0);
	
    vec3 F0 = mix(vec3(0.04), baseColor.xyz, metallic);
	isShadowed   = true; 
	if (dot(worldNrm,L) > 0){

		float tMin   = 0.01;
		float tMax   = l_Sample.distance;
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
	}
	
	
	// 1) compute direct PBR_result
	PBR_result res = CalculatePBRResult(worldNrm, V, L,
										baseColor, l_Sample.color,
										l_Sample.intensity, F0,
										metallic, roughness);

	// 2) pick unshadowed radiance
	vec3 directRadiance = isShadowed ? vec3(0.0) : res.color;

	// 3) clamp the light falloff
	const float minD = 0.01;


	// 4) build your throughput update **without dividing by pdf**
	//    since you’re cosine‑hemisphere sampling:
	
  	float cosNL    = max(dot(worldNrm, L), 0.0);


	vec3  bsdf = res.f;

	float tHit = gl_HitTEXT;
	const float maxAtt = 100.0;
	float dist = max(tHit, minD);
	float invSq = 1.0 / (dist * dist);
	invSq = min(invSq, maxAtt);

	// 5) update attenuation
	prd.attenuation *= bsdf * cosNL;

	// 6) write your radiance into the payload
	prd.hitValue = directRadiance;
}
