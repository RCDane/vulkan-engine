#extension GL_EXT_scalar_block_layout : require


layout(set = 0, binding = 0) uniform  SceneData{   

	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	vec4 cameraPosition;
} sceneData;





layout(set = 2, binding = 0) uniform PointLight {
	vec3 position;
	float intensity;
	vec4 color;
} pointLight;