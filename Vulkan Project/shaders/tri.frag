#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler;

struct PointLight {
	vec3 position;
	float intensity;
};

layout(std140, binding = 10) uniform Lights {
	PointLight pointLights[16];
	uint pointLightCount;
} lights;

void main() {
	vec3 finalColor = vec3(0);
	
	for(uint i = 0; i < lights.pointLightCount; i++) {
		PointLight light = lights.pointLights[i];

		vec3 L = normalize(fragPosition - light.position);

		float distance = distance(fragPosition, light.position);
		float attenuation = 1.f / (distance * distance);

		float diff = max(dot(fragNormal, L), 0.0);

		finalColor += vec3(diff * attenuation);
	}

	const float gamma = 2.2;
	finalColor = pow(finalColor, vec3(1.0 / gamma));

	outColor = vec4(finalColor, 1.0);
}