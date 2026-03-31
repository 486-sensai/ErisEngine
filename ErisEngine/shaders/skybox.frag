#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (location = 0) in vec3 fragViewDir;
layout (location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform samplerCube skyboxSampler;

void main() {
	outColor = texture(skyboxSampler, fragViewDir);
}
