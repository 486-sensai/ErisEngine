#version 450
#extension GL_KHR_vulkan_glsl : enable

layout (location = 0) in vec3 inPos;

layout (location = 0) out vec3 fragViewDir;

layout (set = 0, binding = 0) uniform SceneData {
	mat4 view;
	mat4 proj;
} scene;

void main() {
	fragViewDir = inPos;
	// 如果发现倒过来了，在这里对 fragViewDir 动手脚：
    // fragViewDir.y *= -1.0; 

	mat4 viewNoTransform = mat4(mat3(scene.view));

	vec4 pos = scene.proj * viewNoTransform * vec4(inPos, 1.0);

	gl_Position = pos.xyww;
}