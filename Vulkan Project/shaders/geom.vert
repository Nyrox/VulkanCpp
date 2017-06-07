#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

out gl_PerVertex {
	vec4 gl_Position;
};

layout(binding = 0) uniform MVPBuffer {
	mat4 model;
	mat4 view;
	mat4 projection;
} mvp;

void main() {
	gl_Position = (mat4(mat3(mvp.view)) * vec4(inPosition, 1.0)).xyzz;

	fragPosition = (mvp.model * vec4(inPosition, 1.0)).xyz;
	fragNormal = mat3(transpose(inverse(mvp.model))) * inNormal;
	fragTexCoord = inTexCoord;
}