#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 localPosition;

out gl_PerVertex {
    vec4 gl_Position;
};

layout(push_constant) uniform PushConstants {
	mat4 view;
	mat4 projection;
} vp;


void main() {
    gl_Position = vp.projection * vp.view * vec4(inPosition, 1.0);
	localPosition = inPosition;
}