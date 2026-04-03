#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 worldPos;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform constants {
    mat4 render_matrix;
    mat4 model_matrix;
    vec4 materialParams; // x: roughness, y: metallic, z: emission
} PushConstants;

// MRT Êä³ö
layout(location = 0) out vec4 outPosition; // RGB: Pos, A: Emission
layout(location = 1) out vec4 outNormal;   // RGB: Normal, A: Metallic
layout(location = 2) out vec4 outAlbedo;   // RGB: Albedo, A: Roughness

void main() {
    vec3 albedo = texture(texSampler, fragUV).rgb * fragColor;
    
    // Ð´Èë GBuffer
    outPosition = vec4(worldPos, PushConstants.materialParams.z);
    outNormal   = vec4(normalize(fragNormal), PushConstants.materialParams.y);
    outAlbedo   = vec4(albedo, PushConstants.materialParams.x);
}