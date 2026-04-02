#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 fragColor;    // 来自 MTL 的 Kd 颜色
layout(location = 1) in vec2 fragUV;       // 纹理坐标
layout(location = 2) in vec3 fragNormal;   // 法线
layout(location = 3) in vec3 viewPos;
layout(location = 4) in vec4 fragPosLightSpace;

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
    GPUPointLight pointLights[8];
    int lightCount;
} scene;

layout(set = 0, binding = 1) uniform sampler2D shadowMap;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outFragColor;

float calculateShadow(vec4 shadowPos) {
    // 1. 执行透视除法
    vec3 projCoords = shadowPos.xyz / shadowPos.w;
    // 2. 变换到 [0,1] 范围
    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = projCoords.y * 0.5 + 0.5;
    
    // 如果坐标在阴影贴图之外，不产生阴影
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0; 
    }
    // 3. 取得阴影贴图中的最小深度值
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    
    // 4. 当前像素到太阳的实际深度
    float currentDepth = projCoords.z;

    // 5. 阴影偏移 (Bias) 防止阴影粉刺 (Shadow Acne)
    // 根据表面倾斜度动态调整 bias 是引擎级的做法
    float bias = 0.005;
    //float bias = max(0.05 * (1.0 - dot(normalize(fragNormal), normalize(scene.sunlightDir.xyz))), 0.005);
    
    // 简单硬阴影判断
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    return shadow;
}

void main() {
    // 1. 采样贴图
    // 如果没有贴图，我们绑定的 1x1 白图会返回 (1, 1, 1, 1)
    vec4 texColor = texture(texSampler, fragUV);
    vec3 N = normalize(fragNormal);
    
    // 1. 环境光
    vec3 ambient = scene.ambientColor.rgb * scene.ambientColor.a;
    
    float shadow = calculateShadow(fragPosLightSpace);  
    // 2. 平行光 (太阳)
    vec3 L_sun = normalize(scene.sunlightDir.xyz);
    
    // 3. 计算漫反射 (Diffuse)
    float diff_sun = max(dot(N, L_sun), 0.0);
    vec3 diffuse_sun = (1.0 - shadow) * diff_sun * scene.sunlightColor.rgb * scene.sunlightColor.a;
    
    vec3 totalPointLight = vec3(0.0);
    for(int i = 0; i < scene.lightCount; i++) {
        vec3 lightVec = scene.pointLights[i].position.xyz - viewPos;
        float dist = length(lightVec);
        vec3 L = normalize(lightVec);
        
        // 衰减计算 (距离平方反比)
        float range = scene.pointLights[i].position.w;
        float attenuation = max(1.0 - (dist / range), 0.0);
        
        float diff = max(dot(N, L), 0.0);
        totalPointLight += (1.0 - shadow) * diff * scene.pointLights[i].color.rgb * scene.pointLights[i].color.a * attenuation;
    }

    float backlight = max(dot(N, -L_sun), 0.0) * 0.1;

    // 4. 最终颜色融合
    vec3 lighting = ambient + diffuse_sun + totalPointLight + (backlight * fragColor);
    vec3 finalRGB = texColor.rgb * fragColor * lighting;
    
    outFragColor = vec4(finalRGB, 1.0);
}