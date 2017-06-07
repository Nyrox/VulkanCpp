#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
};

layout(binding = 0) uniform MVPBuffer {
	mat4 model;
	mat4 view;
	mat4 projection;
} mvp;

layout(push_constant) uniform PushConstants {
	mat4 view;
} p;

layout(location = 0) out vec3 outPosition;

void main() {
    gl_Position = mvp.projection * p.view * vec4(inPosition, 1.0);
	outPosition = inPosition;
}