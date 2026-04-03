#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

struct GPUPointLight {
    vec4 position; // xyz: 坐标, w: 半径
    vec4 color;    // rgb: 颜色, w: 强度
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
    float padding[3];    
} scene;

layout(set = 0, binding = 1) uniform sampler2D shadowMap;
layout(set = 1, binding = 0) uniform sampler2D gPos;    
layout(set = 1, binding = 1) uniform sampler2D gNormal; 
layout(set = 1, binding = 2) uniform sampler2D gAlbedo; 

const float PI = 3.14159265359;

// --- PBR 核心公式 ---
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return num / (PI * denom * denom);
}
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// 太阳阴影采样 (只针对太阳)
float getSunShadow(vec3 worldPos) {
    vec4 shadowPos = scene.sunlightProj * vec4(worldPos, 1.0);
    vec3 projCoords = shadowPos.xyz / shadowPos.w;
    vec2 uv = projCoords.xy * 0.5 + 0.5;
    if (uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1) return 0.0;
    float closestDepth = texture(shadowMap, uv).r;
    return (projCoords.z - 0.005 > closestDepth) ? 1.0 : 0.0;
}

void main() {
    vec4 posE = texture(gPos, inUV);
    vec4 normM = texture(gNormal, inUV);
    vec4 albR = texture(gAlbedo, inUV);

    if (length(normM.xyz) < 0.5) {
        outFragColor = vec4(scene.fogColor.rgb, 1.0);
        return;
    }

    vec3 worldPos = posE.xyz;
    vec3 N = normalize(normM.xyz);
    vec3 V = normalize(scene.viewPos.xyz - worldPos);
    vec3 albedo = albR.rgb;
    float roughness = max(albR.a, 0.05);
    float metallic = normM.a;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    // 1. 【太阳光】 (受阴影影响)
    float shadow = getSunShadow(worldPos);
    vec3 L_sun = normalize(scene.sunlightDir.xyz);
    vec3 H_sun = normalize(V + L_sun);
    float NDF_s = DistributionGGX(N, H_sun, roughness);
    float G_s = GeometrySmith(N, V, L_sun, roughness);
    vec3 F_s = fresnelSchlick(max(dot(H_sun, V), 0.0), F0);
    vec3 spec_s = (NDF_s * G_s * F_s) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L_sun), 0.0) + 0.0001);
    vec3 kD_s = (vec3(1.0) - F_s) * (1.0 - metallic);
    Lo += (1.0 - shadow) * (kD_s * albedo / PI + spec_s) * scene.sunlightColor.rgb * scene.sunlightColor.a * max(dot(N, L_sun), 0.0);

    // 2. 【点光源循环】 (【关键修正】：不受上面的 shadow 变量影响)
    for(int i = 0; i < scene.lightCount; i++) {
        vec3 L = normalize(scene.pointLights[i].position.xyz - worldPos);
        vec3 H = normalize(V + L);
        float d = length(scene.pointLights[i].position.xyz - worldPos);
        float range = scene.pointLights[i].position.w;
        float intensity = scene.pointLights[i].color.w;

        float attenuation = intensity / (d * d + 1.0);
        attenuation *= clamp(1.0 - pow(d / range, 4.0), 0.0, 1.0);

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 spec = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

        Lo += (kD * albedo / PI + spec) * scene.pointLights[i].color.rgb * attenuation * max(dot(N, L), 0.0);
    }

    // 3. 【环境色 & 自发光】
    vec3 ambient = scene.ambientColor.rgb * scene.ambientColor.a * albedo;
    vec3 result = ambient + Lo + (albedo * posE.a * 5.0);

    // HDR & Gamma
    result = result / (result + vec3(1.0));
    outFragColor = vec4(pow(result, vec3(1.0/2.2)), 1.0);
}