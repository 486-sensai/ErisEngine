#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNormal;

layout(push_constant) uniform constants {
    mat4 render_matrix;
} PushConstants;

void main() {
    gl_Position = PushConstants.render_matrix * vec4(inPos, 1.0);
    fragColor = inColor;
    fragUV = inUV;
    // 这里的法线变换在拍扁模型后可以直接用 mat3 提取旋转
    fragNormal = mat3(PushConstants.render_matrix) * inNormal;
}