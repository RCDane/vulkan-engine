void MirrorShading(vec3 N, int mat_id,vec3 position ){
	if (prd.depth >= 2){
		prd.hitValue = texture(cubeMap, N).rgb;
		return;
	}
	// reflection
	float tMin   = 0.001;
	float tMax   = 10000.0;
	vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
	vec3  rayDir = reflect(gl_WorldRayDirectionEXT, N);
	uint  flags  = gl_RayFlagsNoneEXT;
	isShadowed   = true;
    prd.depth++;
	prd.rayOrigin = position;
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
				0            // payload (location = 1)
	);
	prd.hitValue = prd.hitValue;
	return;
	// refraction

}

float r0(float n1, float n2){
	float r = (n1 - n2) / (n1 + n2);
	r = r * r;
	return r;
}

float Schlick(vec3 N, vec3 I, float ior1, float ior2){
	float r0 = r0(ior1, ior2);
	return r0 + (1.0 - r0) * pow(1.0 - dot(N, I), 5.0);
}

vec3 computeLocalGlassLighting(vec3 worldPos, vec3 worldNrm, GLTFMaterialData mat)
{
    // Choose a base color for the glass surface (tint). 
    // If your material has a “colorFactors” or a texture, include that here:
    vec3 baseColor = mat.colorFactors.rgb;

    // 1. Setup the light direction & intensity
    vec3  L;
    float lightIntensity   = pcRay.lightIntensity;
    float lightDistance    = 100000.0;
    if (pcRay.lightType == 0) {
        // Point light
        vec3 lDir = pcRay.lightPosition - worldPos;
        lightDistance  = length(lDir);
        lightIntensity = pcRay.lightIntensity / (lightDistance * lightDistance);
        L = normalize(lDir);
    } else {
        // Directional
        L = -normalize(pcRay.lightPosition);
    }

    vec3 lightColor = pcRay.lightColor * lightIntensity;

    // 2. Ambient
    vec3 ambient = pcRay.ambientColor.xyz * baseColor;

    // 3. Diffuse
    float diff = max(dot(worldNrm, L), 0.0);
    vec3 diffuse = diff * lightColor * baseColor;

    // 4. Shadow test
    float tMin = 0.001;
    float tMax = lightDistance;
    vec3 origin = worldPos;
    vec3 rayDir = L;
    uint flags  = gl_RayFlagsTerminateOnFirstHitEXT 
                | gl_RayFlagsOpaqueEXT 
                | gl_RayFlagsSkipClosestHitShaderEXT;

    // Overwrite the global isShadowed
    isShadowed = true;
    traceRayEXT(topLevelAS,
                flags,
                0xFF,     // cull mask
                0, 0,
                1,        // missIndex=1 (shadow miss)
                origin,
                tMin,
                rayDir,
                tMax,
                1);       // payload index=1 => isShadowed
    if (isShadowed) {
        // either zero or partial
        diffuse  *= 0.0;
    }

    // 5. Specular (simple Blinn-Phong)
    //    If shadowed, it might be zero, or you can do the same attenuation logic.
    vec3 specular = vec3(0.0);
    if (!isShadowed) {
        vec3 V = -gl_WorldRayDirectionEXT;
        vec3 H = normalize(L + V);
        float shininess = 64.0;  // or from your roughness
        float specAngle = max(dot(worldNrm, H), 0.0);
        float spec      = pow(specAngle, shininess);
        vec3 specularBase = vec3(0.04); // or something else for glass
        specular = spec * lightColor * specularBase;
    }

    // 6. Final local color
    vec3 localColor = ambient + diffuse + specular;
    return localColor;
}



void GlassShading(vec3 N, GLTFMaterialData mat, vec3 position)
{
    // 1. Early‐out recursion limit
    if (prd.depth > 6) {
        prd.hitValue = vec3(0.0);
        return;
    }

    // 2. Determine if we are “inside” the glass
    bool currentlyInside = (dot(N, gl_WorldRayDirectionEXT) > 0.0);
    if (currentlyInside) {
        N = -N;  // Flip normal so it opposes the incoming direction
    }

    // 3. Indices of refraction (IoR)
    float ior1 = currentlyInside ? mat.iridescenceIoR : 1.0;  // inside -> air
    float ior2 = currentlyInside ? 1.0 : mat.iridescenceIoR;  // air -> inside
    float ratio = ior1 / ior2;

    // 4. Reflection direction
    vec3 V  = normalize(gl_WorldRayDirectionEXT);
    vec3 R  = reflect(V, N);

    // 5. Compute reflection color by tracing a reflection ray
    vec3 reflectionColor = vec3(0.0);
    {
        prd.depth++;
        prd.rayOrigin = position;
        traceRayEXT(topLevelAS,
                    gl_RayFlagsNoneEXT,
                    0xFF,   // cullMask
                    0, 0, 0,  // SBT offsets, missIndex=0 for environment
                    prd.rayOrigin,
                    0.001,    // tMin
                    R,
                    10000.0,  // tMax
                    0);       // payload index=0 (prd)
        reflectionColor = prd.hitValue;
        prd.depth--;
    }

    // 6. Attempt refraction
    vec3 T  = refract(V, N, ratio);
    bool isTIR = all(equal(T, vec3(0.0)));

    // 7. If total internal reflection, return reflection + local shading
    //    because no light is transmitted outside.
    if (isTIR) {
        // Optionally add local shading here if you want the glass surface
        // itself to still get lighting. (See step 8 for local shading.)
        vec3 localLighting = (currentlyInside)
                             ? vec3(0.0)              // if we’re inside, maybe skip
                             : computeLocalGlassLighting(position, N, mat);
        prd.hitValue = reflectionColor + localLighting;
        return;
    }

    // 8. Compute local shading from the direct light
    //    (ambient/diffuse/specular + shadow test),
    //    if we are “outside” the glass. Typically, if you are inside,
    //    you might skip local shading. This is an artistic choice.
    vec3 localColor = currentlyInside 
                      ? vec3(0.0)
                      : computeLocalGlassLighting(position, N, mat); 
    // “computeLocalGlassLighting” is a helper shown below.

    // 9. Fresnel factor for reflection vs. refraction blend
    float reflectionProb = Schlick(N, -V, ior1, ior2);

    // 10. Trace refraction ray to get “behind the glass” color
    vec3 refractionColor = vec3(0.0);
    {
        prd.depth++;
        prd.rayOrigin = position;
        traceRayEXT(topLevelAS,
                    gl_RayFlagsNoneEXT,
                    0xFF,
                    0, 0, 0,  // environment miss
                    prd.rayOrigin,
                    0.001,
                    T,
                    10000.0,
                    0);
        refractionColor = prd.hitValue;
        prd.depth--;
    }

    // 11. If we’re inside, we mostly see refraction. If outside, we do a Fresnel blend.
    vec3 glassBlend = currentlyInside
                      ? refractionColor
                      : mix(refractionColor, reflectionColor, reflectionProb);

    // 12. Add local shading as “surface color.” You can weigh it by (1 - reflectionProb)
    //     if you prefer to treat it as part of the transmitted portion:
    vec3 finalColor = glassBlend + localColor * (1.0 - reflectionProb);

    prd.hitValue = finalColor;
}


// void GlassShading(vec3 N, GLTFMaterialData mat,vec3 position ){
// 	if (prd.depth > 22){
// 		prd.hitValue = vec3(0.0);
// 		return;
// 	}

// 	// Calculate total internal reflection
// 	vec3 dir = normalize(gl_WorldRayDirectionEXT);

// 	float ior1 = currentlyInside ? mat.iridescenceIoR : 1.0;
// 	float ior2 = currentlyInside ? 1.0 : mat.iridescenceIoR;
// 	float ratio = ior1 / ior2;
// 	rayDir = refract(dir, N, ratio);
// 	bool isTIR = all(equal(rayDir, vec3(0.0)));



// 	bool currentlyInside = dot(N, gl_WorldRayDirectionEXT) > 0.0;
// 	N = currentlyInside ? -N : N;

// 	vec3 reflectColor = vec3(0.001);
// 	float tMin   = 0.00001;
// 	float tMax   = 10000.0;
// 	vec3  origin = position;
// 	vec3 rayDir = reflect(normalize(gl_WorldRayDirectionEXT), N);
// 	uint  flags  = gl_RayFlagsNoneEXT;

// 	currentlyInside
// 		// reflection
		
// 		prd.depth++;
// 		prd.rayOrigin = origin;
// 		traceRayEXT(topLevelAS,  // acceleration structure
// 					flags,       // rayFlags
// 					0xFF,        // cullMask
// 					0,           // sbtRecordOffset
// 					0,           // sbtRecordStride
// 					0,           // missIndex
// 					prd.rayOrigin,      // ray origin
// 					tMin,        // ray min range
// 					rayDir,      // ray direction
// 					tMax,        // ray max range
// 					0            // payload (location = 1)
// 		);
// 		reflectColor = prd.hitValue;
	
// 	if (isTIR){
// 		prd.hitValue = reflectColor;
// 		return;
// 	}
	
// 	// refraction
// 	tMin   = 0.00001;
// 	tMax   = 10000.0;
// 	origin = position;

// 	vec3 dir = normalize(gl_WorldRayDirectionEXT);

// 	float ior1 = currentlyInside ? mat.iridescenceIoR : 1.0;
// 	float ior2 = currentlyInside ? 1.0 : mat.iridescenceIoR;
// 	float ratio = ior1 / ior2;
// 	float reflectionProb = Schlick(N, -dir, ior1, ior2);
// 	rayDir = refract(dir, N, ratio);
// 	bool isTIR = all(equal(rayDir, vec3(0.0)));
	
// 	flags  = gl_RayFlagsNoneEXT;
//     prd.depth++;
// 	prd.rayOrigin = origin;
// 	prd.currentIoR = currentlyInside ? 1.0 : mat.iridescenceIoR;
// 	traceRayEXT(topLevelAS,  // acceleration structure
// 				flags,       // rayFlags
// 				0xFF,        // cullMask
// 				0,           // sbtRecordOffset
// 				0,           // sbtRecordStride
// 				0,           // missIndex
// 				prd.rayOrigin,      // ray origin
// 				tMin,        // ray min range
// 				rayDir,      // ray direction
// 				tMax,        // ray max range
// 				0            // payload (location = 1)
// 	);
// 	// prd.inside = currentlyInside ? 1 : 0;
// 	vec3 refractColor = prd.hitValue;
// 	vec3 finalColor = vec3(0.0);
// 	if (currentlyInside){
// 		finalColor = refractColor;
// 	}
// 	else{
// 		finalColor = mix(refractColor,reflectColor, reflectionProb);
// 	}
// 	prd.hitValue = finalColor;
// 	return;
// }
vec3 getIridescenceColor(float NdotV, float thickness) {
    // Approximate thin film interference
    float phase = 6.28318530718 * (thickness * 2.0);
    vec3 wavelengths = vec3(650.0, 510.0, 475.0); // RGB wavelengths in nanometers
    vec3 phi = 2.0 * 3.14159 * thickness * (1.0 - NdotV) / wavelengths;
    return 0.5 + 0.5 * cos(phi);
}
vec3 computeLocalBubbleLighting(vec3 worldPos, vec3 worldNrm, GLTFMaterialData mat)
{
    // We can use a base color or “tint” for the bubble film:
    vec3 baseColor = vec3(0.8); // or a bubble tint if desired

    // Light setup
    vec3 L;
    float lightIntensity = pcRay.lightIntensity;
    float lightDistance  = 1e6; // large default
    if (pcRay.lightType == 0) {
        // Point light
        vec3 lDir = pcRay.lightPosition - worldPos;
        lightDistance  = length(lDir);
        lightIntensity = pcRay.lightIntensity / (lightDistance * lightDistance);
        L = normalize(lDir);
    } else {
        // Directional
        L = -normalize(pcRay.lightPosition);
    }

    vec3 lightColor = pcRay.lightColor * lightIntensity;

    // Ambient
    vec3 ambient = pcRay.ambientColor.xyz * baseColor;

    // Diffuse
    float diff = max(dot(worldNrm, L), 0.0);
    vec3 diffuse = diff * lightColor * baseColor;

    // Shadow ray
    float tMin   = 0.001;
    float tMax   = lightDistance;
    vec3 origin  = worldPos;
    vec3 rayDir  = L;
    uint flags   = gl_RayFlagsTerminateOnFirstHitEXT 
                 | gl_RayFlagsOpaqueEXT 
                 | gl_RayFlagsSkipClosestHitShaderEXT;

    isShadowed = true;  
    traceRayEXT(topLevelAS,  
                flags,
                0xFF,  // cullMask
                0, 0,
                1,     // missIndex = 1 (shadow)
                origin,
                tMin,
                rayDir,
                tMax,
                1);    // payload index=1 => isShadowed

    if (isShadowed) {
        diffuse = vec3(0.0);
    }

    // Simple specular (Blinn-Phong)
    vec3 specular = vec3(0.0);
    if (!isShadowed) {
        vec3 V = -gl_WorldRayDirectionEXT;
        vec3 H = normalize(L + V);
        float shininess = 64.0; // or from your roughness
        float s = max(dot(worldNrm, H), 0.0);
        float specPower = pow(s, shininess);
        // For a bubble, a small base reflectance is typical:
        vec3 specularBase = vec3(0.04);  
        specular = specPower * lightColor * specularBase;
    }

    return ambient + diffuse + specular;
}

void BubbleShading(vec3 N, GLTFMaterialData mat, vec3 position)
{
    // Basic recursion limit
    if (prd.depth > 6) {
        prd.hitValue = vec3(0.0);
        return;
    }

    // Are we inside the bubble (dot>0) or outside?
    bool currentlyInside = (dot(N, gl_WorldRayDirectionEXT) > 0.0);

    // Flip the normal if inside
    if (currentlyInside) {
        N = -N;
    }

    // Compute reflection direction
    vec3 V = normalize(gl_WorldRayDirectionEXT);
    vec3 R = reflect(V, N);

    // Trace reflection ray
    vec3 reflectionColor = vec3(0.0);
    {
        prd.depth++;
        prd.rayOrigin = position;
        traceRayEXT(topLevelAS, 
                    gl_RayFlagsNoneEXT, 
                    0xFF, 
                    0, 0, 0,  // environment miss
                    prd.rayOrigin, 
                    0.001, 
                    R, 
                    10000.0, 
                    0);
        reflectionColor = prd.hitValue;
        prd.depth--;
    }

    // Thin film parameters (iridescence):
    // We compute a factor based on angle & thickness
    vec3 Vneg   = normalize(-gl_WorldRayDirectionEXT);
    float NdotV = abs(dot(N, Vneg));
    float thickness = mat.iridescenceThickness;
    vec3 iridescence = getIridescenceColor(NdotV, thickness);

    // Indices of refraction (bubble film: water/soap ~1.33 or so)
    // "Inside" is air, "outside" might be air as well, but let's assume
    // ior is ~1.33 for the bubble film. If truly air->air, you won't see much refraction.
    float ior1 = currentlyInside ? mat.iridescenceIoR : 1.0; 
    float ior2 = currentlyInside ? 1.0 : mat.iridescenceIoR; 
    float ratio = ior1 / ior2;

    // Fresnel factor
    float reflectionProb = Schlick(N, Vneg, ior1, ior2);
    // A hack to boost reflection near grazing angles
    reflectionProb = mix(reflectionProb, 1.0, pow(1.0 - NdotV, 5.0));

    // Attempt refraction (the bubble is thin, but let's still do it)
    vec3 T = refract(-Vneg, N, ratio);
    // If T=0, then total internal reflection
    bool isTIR = all(equal(T, vec3(0.0)));

    // If not inside, we can compute local lighting for the outside film
    // If inside, maybe skip it or do a separate shading if the bubble has thickness.
    vec3 localLight = currentlyInside 
                      ? vec3(0.0) 
                      : computeLocalBubbleLighting(position, N, mat);

    // If TIR, it's all reflection
    if (isTIR) {
        // Add local lighting if outside
        vec3 finalColor = reflectionColor * iridescence + localLight;
        prd.hitValue = finalColor;
        return;
    }

    // Otherwise, trace the refraction ray
    vec3 refractionColor;
    {
        prd.depth++;
        prd.rayOrigin = position;
        traceRayEXT(topLevelAS, 
                    gl_RayFlagsNoneEXT, 
                    0xFF, 
                    0, 0, 0,
                    prd.rayOrigin, 
                    0.001, 
                    T, 
                    10000.0, 
                    0);
        refractionColor = prd.hitValue;
        prd.depth--;
    }

    // Iridescent reflection color
    vec3 iridescentReflection = reflectionColor * iridescence;

    // If inside the bubble, we see primarily the refraction
    // If outside, we do a blend
    vec3 bubbleBlend = currentlyInside
        ? refractionColor
        : iridescentReflection * reflectionProb + refractionColor * (1.0 - reflectionProb);

    // Optionally, combine local lighting
    // Typically, you'd weigh local lighting by (1-reflectionProb) or something
    // because reflection can overshadow local diffuse.
    vec3 finalColor = bubbleBlend + localLight * (1.0 - reflectionProb);

    prd.hitValue = finalColor;
}

// void BubbleShading(vec3 N, GLTFMaterialData mat, vec3 position) {
//     // if (prd.depth >22) {
//     //     prd.hitValue = vec3(0.0);
//     //     return;
//     // }

//     bool currentlyInside = dot(N, gl_WorldRayDirectionEXT) > 0.0;
//     N = currentlyInside ? -N : N;

//     vec3 reflectColor = vec3(0.0);
//     float tMin = 0.001;
//     float tMax = 10000.0;
//     vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
//     vec3 rayDir = reflect(gl_WorldRayDirectionEXT, N);
//     uint flags = gl_RayFlagsNoneEXT;

//     if (!currentlyInside) {
//         // reflection
//         prd.depth++;
//         prd.rayOrigin = origin;
//         traceRayEXT(topLevelAS, flags, 0xFF, 0, 0, 0, 
//                     prd.rayOrigin, tMin, rayDir, tMax, 0);
//         reflectColor = prd.hitValue;
//     }

//     // Calculate view angle
//     vec3 V = normalize(-gl_WorldRayDirectionEXT);
//     float NdotV = abs(dot(N, V));

//     // Thin film parameters
//     float thickness = mat.iridescenceThickness;
//     vec3 iridescence = getIridescenceColor(NdotV, thickness);

//     // Calculate refraction
//     float ior1 = currentlyInside ? mat.iridescenceIoR : 1.0; 
//     float ior2 = currentlyInside ? 1.0 : mat.iridescenceIoR; 
//     float ratio = ior1 / ior2;

//     float reflectionProb = Schlick(N, V, ior1, ior2);
//     reflectionProb = mix(reflectionProb, 1.0, pow(1.0 - NdotV, 5.0));

//     vec3 refractDir = refract(-V, N, ratio);
//     prd.depth++;
//     prd.rayOrigin = origin;
//     traceRayEXT(topLevelAS, flags, 0xFF, 0, 0, 0,
//                 origin, tMin, refractDir, tMax, 0);

//     vec3 refractColor = prd.hitValue;

//     // Blend colors with iridescence
//     vec3 finalColor;
//     if (currentlyInside) {
//         finalColor = refractColor;
//     } else {
//         vec3 iridescentReflection = reflectColor * iridescence;
//         // Correctly combine reflection and refraction based on reflectionProb
//         finalColor = iridescentReflection * reflectionProb + refractColor * (1.0 - reflectionProb);
//     }

//     prd.hitValue = finalColor;
// }