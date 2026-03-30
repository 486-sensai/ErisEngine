#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 fragColor;    // 来自 MTL 的 Kd 颜色
layout(location = 1) in vec2 fragUV;       // 纹理坐标
layout(location = 2) in vec3 fragNormal;   // 法线
layout(location = 3) in vec3 viewPos;

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

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outFragColor;

void main() {
    // 1. 采样贴图
    // 如果没有贴图，我们绑定的 1x1 白图会返回 (1, 1, 1, 1)
    vec4 texColor = texture(texSampler, fragUV);
    vec3 N = normalize(fragNormal);
    
    // 1. 环境光
    vec3 ambient = scene.ambientColor.rgb * scene.ambientColor.a;
    
    // 2. 平行光 (太阳)
    vec3 L_sun = normalize(scene.sunlightDir.xyz);
    
    // 3. 计算漫反射 (Diffuse)
    float diff_sun = max(dot(N, L_sun), 0.0);
    vec3 diffuse_sun = diff_sun * scene.sunlightColor.rgb * scene.sunlightColor.a;
    
    vec3 totalPointLight = vec3(0.0);
    for(int i = 0; i < scene.lightCount; i++) {
        vec3 lightVec = scene.pointLights[i].position.xyz - viewPos;
        float dist = length(lightVec);
        vec3 L = normalize(lightVec);
        
        // 衰减计算 (距离平方反比)
        float range = scene.pointLights[i].position.w;
        float attenuation = max(1.0 - (dist / range), 0.0);
        
        float diff = max(dot(N, L), 0.0);
        totalPointLight += diff * scene.pointLights[i].color.rgb * scene.pointLights[i].color.a * attenuation;
    }
    // 4. 最终颜色融合
    vec3 lighting = ambient + diffuse_sun + totalPointLight;
    vec3 finalRGB = texColor.rgb * fragColor * lighting;
    
    outFragColor = vec4(finalRGB, 1.0);
}