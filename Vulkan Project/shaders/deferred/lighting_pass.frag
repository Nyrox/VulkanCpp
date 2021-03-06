#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D gPosition;
layout(set = 0, binding = 1) uniform sampler2D gNormal;

struct PointLight {
	vec3 position;
	float intensity;
};

layout(set = 1, binding = 0) uniform Lights {
	PointLight pointLights[16];
	uint pointLightCount;
} lights;

void main() {
	vec2 screenSpaceUv = vec2(gl_FragCoord.x / 1280, gl_FragCoord.y / 720);
	vec3 N = texture(gNormal, screenSpaceUv).rgb;
	vec3 fragPos = texture(gPosition, screenSpaceUv).rgb;

	vec3 finalColor = vec3(0);

	for(int i = 0; i < lights.pointLightCount; i++) {
		PointLight light = lights.pointLights[i];
		vec3 L = normalize(light.position - fragPos);
		float distance = distance(fragPos, light.position);
		float attenuation = 1.f / (distance * distance);
		finalColor += max(dot(N, L), 0.0) * light.intensity * attenuation;
	}

	outColor = vec4(finalColor, 1.0);
}