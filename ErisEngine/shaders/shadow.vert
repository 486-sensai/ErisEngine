#version 450
#extension GL_KHR_vulkan_glsl : enable

// 顶点输入属性（只需位置）
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;   // 虽然不用，但为了匹配顶点布局建议写上
layout(location = 2) in vec3 inColor;    // 同上
layout(location = 3) in vec2 inUV;       // 同上

// --- Set 0: 全局环境数据 (必须严格保持一致以防内存错位) ---
struct GPUPointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;         
    mat4 proj;         
    mat4 sunlightProj; 
    vec4 viewPos;      
    vec4 fogColor;     
    vec4 ambientColor; 
    vec4 sunlightDir;  
    vec4 sunlightColor;
    GPUPointLight pointLights[8]; // 256字节
    int lightCount;     // 4字节
} scene;

// --- Push Constants ---
layout(push_constant) uniform constants {
    mat4 render_matrix; // 实际上是物体的 MVP
    mat4 model_matrix;  // 物体的世界变换矩阵
} PushConstants;

void main() {
    // 阴影渲染的核心逻辑：
    // 使用太阳的投影矩阵 (sunlightProj) 乘以 物体的世界矩阵 (model_matrix)
    // 将物体投影到太阳的视角下
    gl_Position = scene.sunlightProj * PushConstants.model_matrix * vec4(inPos, 1.0);
}