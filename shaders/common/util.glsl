




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

uint tea(uint val0, uint val1)
{
  uint v0 = val0;
  uint v1 = val1;
  uint s0 = 0;

  for(uint n = 0; n < 16; n++)
  {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  return v0;
}

// Generate a random unsigned int in [0, 2^24) given the previous RNG state
// using the Numerical Recipes linear congruential generator
uint lcg(inout uint prev)
{
  uint LCG_A = 1664525u;
  uint LCG_C = 1013904223u;
  prev       = (LCG_A * prev + LCG_C);
  return prev & 0x00FFFFFF;
}

// Generate a random float in [0, 1) given the previous RNG state
float rnd(inout uint prev)
{
  return (float(lcg(prev)) / float(0x01000000));
}

uint pcg_hash (uint x)
{
    x = x * 747796405u + 2891336453u;          // LCG step
    uint word = ((x >> ((x >> 28u) + 4u)) ^ x) * 277803737u;
    return (word >> 22u) ^ word;
}

// sample an *inclusive* integer range [lo, hi]
// mutates 'seed' so the next call is decorrelated
int randRange (inout uint seed, int lo, int hi)
{
    seed = pcg_hash(seed);
    uint span = uint(hi - lo + 1);            // assumes hi ≥ lo
    return lo + int(seed % span);
}


//-------------------------------------------------------------------------------------------------
// Sampling
//-------------------------------------------------------------------------------------------------

// Randomly samples from a cosine-weighted hemisphere oriented in the `z` direction.
// From Ray Tracing Gems section 16.6.1, "Cosine-Weighted Hemisphere Oriented to the Z-Axis"
vec3 samplingHemisphere(inout uint seed, in vec3 x, in vec3 y, in vec3 z)
{
#define M_PI 3.14159265

  float r1 = rnd(seed);
  float r2 = rnd(seed);
  float sq = sqrt(r1);

  vec3 direction = vec3(cos(2 * M_PI * r2) * sq, sin(2 * M_PI * r2) * sq, sqrt(1. - r1));
  direction      = direction.x * x + direction.y * y + direction.z * z;

  return normalize(direction);
}

// Return the tangent and binormal from the incoming normal
void createCoordinateSystem(in vec3 N, out vec3 Nt, out vec3 Nb)
{
  if(abs(N.x) > abs(N.y))
    Nt = vec3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
  else
    Nt = vec3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
  Nb = cross(N, Nt);
}


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

const int MAX_LIGHTS = 1000;
const float INF_DISTANCE = 1e10;

LightSample ProcessLight(vec3 hitPoint, inout uint seed, LightSource Ls){
    LightSample ls;
    ls.color = Ls.color;           // base color of the light
    ls.intensity = Ls.intensity;   // scalar intensity

    if (Ls.type == 0) {

        vec3 samplePos = sampleSphere(Ls.position, Ls.radius, seed);

        vec3 toLight   = samplePos - hitPoint;
        float dist     = length(toLight);
        vec3 dir       = normalize(toLight);

        float area     = 4.0 * PI * Ls.radius * Ls.radius;
        float areaPdf  = 1.0 / area;

        ls.pdf = areaPdf;

        ls.intensity = ls.intensity / (area*PI); // scale intensity by area pdf
        ls.attenuation =  vec3(1)/(dist * dist);

        ls.direction = dir;
        ls.distance  = dist;
    } else {
                vec3 w = normalize(-Ls.direction);

        // 2) branchless ONB (Duff et al. 2017)
        float s = (w.z >= 0.0 ? 1.0 : -1.0);
        float a = -1.0 / (s + w.z);
        float b = w.x * w.y * a;
        vec3 u  = vec3(1.0 + s * w.x * w.x * a, s * b, -s * w.x);
        vec3 v  = vec3(b, s + w.y * w.y * a, -w.y);

        // 3) sample a direction within the cone of half-angle sunAngle
        float cosMax = cos(Ls.sunAngle/2.0);
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
        ls.pdf = conePdf;
    }
    return ls;
}


