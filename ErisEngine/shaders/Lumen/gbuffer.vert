#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 worldPos;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 sunlightProj;
} scene;

layout(push_constant) uniform constants {
    mat4 render_matrix; // Projection * View * Model
    mat4 model_matrix;  // Model
    vec4 materialParams; 
} PushConstants;

void main() {
    vec4 wPos = PushConstants.model_matrix * vec4(inPos, 1.0);
    gl_Position = PushConstants.render_matrix * vec4(inPos, 1.0);

    fragUV = inUV;
    fragColor = inColor;
    worldPos = wPos.xyz;
    // 传导法线到世界空间
    fragNormal = mat3(PushConstants.model_matrix) * inNormal;
}