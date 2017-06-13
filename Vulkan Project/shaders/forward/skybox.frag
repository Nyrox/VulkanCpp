#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragPosition;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler3D skybox;

void main() {
	vec3 sampleVector = normalize(fragPosition);

	vec3 skyColor = texture(skybox, sampleVector).rgb;

	outColor = vec4(skyColor, 1.0);
}