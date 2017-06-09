#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragPosition;


out gl_PerVertex {
    vec4 gl_Position;
};

layout(set = 0, binding = 0) uniform MVPBuffer {
	mat4 model;
	mat4 view;
	mat4 projection;
} mvp;


void main() {
	fragPosition = inPosition;

    mat4 rotView = mat4(mat3(mvp.view)); // remove translation from the view matrix
    vec4 clipPos = mvp.projection * rotView * vec4(inPosition, 1.0);

    gl_Position = clipPos.xyww;
}