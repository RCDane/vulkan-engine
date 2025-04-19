
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