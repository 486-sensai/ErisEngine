#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) out vec3 nearPoint;
layout(location = 1) out vec3 farPoint;

// --- Set 0: 全局 UBO (必须严格对齐 C++ GPUSceneData) ---
struct GPUPointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;          // 64
    mat4 proj;          // 64
    mat4 sunlightProj;  // 64
    vec4 viewPos;       // 16
    vec4 fogColor;      // 16
    vec4 ambientColor;  // 16
    vec4 sunlightDir;   // 16
    vec4 sunlightColor; // 16
    GPUPointLight pointLights[8]; // 256
    int lightCount;     // 4
} scene;

// 全屏填充四边形 (NDC 坐标)
vec3 gridPlane[6] = vec3[](
    vec3(1, 1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
    vec3(-1, -1, 0), vec3(1, 1, 0), vec3(1, -1, 0)
);

vec3 UnprojectPoint(float x, float y, float z, mat4 view, mat4 proj) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(proj);
    vec4 unprojectedPoint =  viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    vec3 p = gridPlane[gl_VertexIndex];
    
    // 计算当前像素对应的世界空间近平面点和远平面点
    nearPoint = UnprojectPoint(p.x, p.y, 0.0, scene.view, scene.proj);
    farPoint = UnprojectPoint(p.x, p.y, 1.0, scene.view, scene.proj);
    
    gl_Position = vec4(p, 1.0); 
}