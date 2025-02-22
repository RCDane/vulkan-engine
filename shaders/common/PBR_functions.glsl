const float PI = 3.1415926535897932384626433832795;





// Link: https://learnopengl.com/PBR/Theory
float DistributionGGX(vec3 N, vec3 H, float a)
{
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float nom    = a2;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom        = PI * denom * denom;
	
    return nom / denom;
}


// Link: https://learnopengl.com/PBR/Theory
float GeometrySchlickGGX(float NdotV, float k)
{
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return nom / denom;
}
  
// Link: https://learnopengl.com/PBR/Theory
float GeometrySmith(vec3 N, vec3 V, vec3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);
	
    return ggx1 * ggx2;
}


float r0(float n1, float n2){
	float r = (n1 - n2) / (n1 + n2);
	r = r * r;
	return r;
}

float Schlick(vec3 N, vec3 V, float ior1, float ior2){
    float r0 = r0(ior1, ior2);
    float cosTheta = max(dot(N, V), 0.0);  // Use the view direction instead of I
    return r0 + (1.0 - r0) * pow(1.0 - cosTheta, 5.0);
}
