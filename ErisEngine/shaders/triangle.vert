#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 viewPos;

struct GPUPointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform SceneData {
    vec4 fogColor;
    vec4 ambientColor;
    vec4 sunlightDir;
    vec4 sunlightColor;
    GPUPointLight pointLights[8];
    int lightCount;
} scene;

layout(push_constant) uniform constants {
    mat4 render_matrix;
    mat4 model_matrix;
} PushConstants;

void main() {
    vec4 worldPos = PushConstants.model_matrix * vec4(inPos, 1.0);
    gl_Position = PushConstants.render_matrix * vec4(inPos, 1.0);

    fragColor = inColor;
    fragUV = inUV;
    fragNormal = mat3(PushConstants.model_matrix) * inNormal;
    viewPos = worldPos.xyz;
}