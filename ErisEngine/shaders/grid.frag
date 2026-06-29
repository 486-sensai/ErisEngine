#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_GOOGLE_include_directive : enable
#include "shadows/pcss.glsl"

layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;

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

layout(location = 0) out vec4 outColor;

// 计算该像素对应的 Vulkan 深度值 [0, 1]
float computeDepth(vec3 worldPos) {
    vec4 clip_space_pos = scene.proj * scene.view * vec4(worldPos, 1.0);
    float depth = clip_space_pos.z / clip_space_pos.w;
    return clamp(depth, 0.0, 0.999999);
}


void main() {
    float t = -nearPoint.y / (farPoint.y - nearPoint.y);
    if (t <= 0) discard;

    vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);

    // 写入深度
    gl_FragDepth = computeDepth(fragPos3D);

    // 计算阴影 (返回值为 0.0 到 1.0，越大代表越处于阴影中)
    vec4 shadowPos = scene.sunlightProj * vec4(fragPos3D, 1.0);
    float shadow = calculatePCSS(shadowMap, shadowPos);

    vec2 coord = fragPos3D.xz;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float lineAlpha = 1.0 - min(line, 1.0);

    vec3 groundColor = vec3(0.1); 
    vec3 lineColor = vec3(0.25);  

    if(abs(fragPos3D.z) < 0.1 * derivative.y) lineColor = vec3(1.0, 0.0, 0.0);
    if(abs(fragPos3D.x) < 0.1 * derivative.x) lineColor = vec3(0.0, 0.0, 1.0);

    vec3 finalColor = mix(groundColor, lineColor, lineAlpha);

    // 【应用阴影】：mix 函数让阴影过渡更平滑，0.4 是阴影最暗处的亮度
    finalColor = mix(finalColor, finalColor * 0.4, shadow);

    float viewDist = length(fragPos3D - scene.viewPos.xyz);
    float fading = max(0.0, 1.0 - (viewDist / 100.0)); 

    outColor = vec4(finalColor, fading);
    
}