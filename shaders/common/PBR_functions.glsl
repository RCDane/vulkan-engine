const float PI = 3.1415926535;
const double dPI = 3.1415926535;

const float InvPi = 0.318309886183;



// GGX Microfacet model NDF
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness; // roughness squared
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0) ;
    denom = PI * denom * denom+ 0.000001;
	
    return num / denom;
}



// Schlick's approximation of GGX geometry function
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

// Smith's method for calculating the geometry term for both shadowing and masking
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}


float r0(float n1, float n2){
	float r = (n1 - n2) / (n1 + n2);
	r = r * r;
	return r;
}

float Schlick(vec3 N, vec3 V, float ior1, float ior2){
    float r0 = r0(ior1, ior2);
    float cosTheta = max(dot(N, -V), 0.0);  
    return r0 + (1.0 - r0) * pow(1.0 - cosTheta, 5.0);
}


vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

struct PBR_result {
    vec3 f;
    vec3 color;
};



PBR_result CalculatePBRResult(
    vec3 N, 
    vec3 V, 
    vec3 L,
    vec3 albedo, 
    vec3 lightColor, 
    float lightIntensity, 
    float metallness,
    float a){


    vec3 F0 = mix(vec3(0.04), albedo, metallness);

    // if (a < 0.01) {
    //     vec3 R = reflect(-V, N);
    //     float specFactor = pow(max(dot(R, L), 0.0), 1024.0); // Approximation of delta function
    //     vec3 F = fresnelSchlick(max(dot(N, V), 0.0), F0);
        
    //     PBR_result res;
    //     res.color = lightColor * lightIntensity * specFactor * F;
    //     res.f = F; // For mirrors, BRDF is essentially just Fresnel
    //     return res;
    // }

    vec3 radiance =  lightColor * lightIntensity; //Incoming Radiance
    vec3 H = normalize(V + L); //half vector
    // cook-torrance brdf
    float NDF = DistributionGGX(N, H, a);
    float G   = GeometrySmith(N, V, L,a);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallness;

    vec3 nominator    = NDF*G * F;
    float denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.00001/* avoid divide by zero*/;
    vec3 specular     = nominator / denominator;

    // add to outgoing radiance Lo
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse_brdf  = kD * (albedo)/ PI;

    vec3 outgoing = (diffuse_brdf  + specular) * radiance * NdotL;

    PBR_result res;
    res.color = outgoing;
    res.f = diffuse_brdf  + specular; // BRDF is diffuse + specular
    return res; 
}

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(2.2));
    return vec4(linOut,srgbIn.w);
}
vec3 SRGBtoLINEAR(vec3 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(2.2));
    return linOut;
}

vec4 LINEARtoSRGB(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(1.0 / 2.2));
    return vec4(linOut, srgbIn.w);
}

vec3 LINEARtoSRGB(vec3 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(1.0 / 2.2));
    return linOut;
}
