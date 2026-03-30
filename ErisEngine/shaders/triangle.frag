#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 fragColor;    // 来自 MTL 的 Kd 颜色
layout(location = 1) in vec2 fragUV;       // 纹理坐标
layout(location = 2) in vec3 fragNormal;   // 法线

layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(location = 0) out vec4 outFragColor;

void main() {
    // 1. 采样贴图
    // 如果没有贴图，我们绑定的 1x1 白图会返回 (1, 1, 1, 1)
    vec4 texColor = texture(texSampler, fragUV);
    
    // 2. 准备光照向量
    vec3 L = normalize(vec3(0.5, 1.0, 0.5)); // 主光源方向（斜上方）
    vec3 N = normalize(fragNormal);
    
    // 3. 计算漫反射 (Diffuse)
    float diffuse = max(dot(N, L), 0.0);
    
    // 4. 计算环境光 (Ambient) 
    // 稍微调高一点（0.3），保证物体在背光处和多视口下依然清晰
    float ambient = 0.3; 
    
    // 5. 计算补光/背光 (Backlight) - 增强体积感
    float backlight = max(dot(N, -L), 0.0) * 0.15;

    // 6. 核心颜色公式（乘法链）
    // 最终颜色 = (环境光 + 漫反射 + 补光) * 材质底色 * 贴图颜色
    vec3 finalRGB = (ambient + diffuse + backlight) * fragColor * texColor.rgb;
    
    // 7. 【关键修正】强制 Alpha 为 1.0
    // 当 Viewport 拖出主窗口时，Windows 窗口管理器对 Alpha 极其敏感。
    // 如果 Alpha 不是 1.0，图像会与桌面透明混合，导致看起来变暗、发虚。
    outFragColor = vec4(finalRGB, 1.0);
}