#version 450
#extension GL_KHR_vulkan_glsl : enable
#extension GL_GOOGLE_include_directive : enable
#include "../shadows/pcss.glsl"

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

// GBuffer 绑定
layout(set = 0, binding = 1) uniform sampler2D shadowMap;
layout(set = 1, binding = 0) uniform sampler2D gPos;    
layout(set = 1, binding = 1) uniform sampler2D gNormal; 
layout(set = 1, binding = 2) uniform sampler2D gAlbedo; 
// 新增：天空盒，用于 IBL
layout(set = 1, binding = 3) uniform samplerCube skyboxMap; 
layout(set = 1, binding = 4) uniform sampler2D brdfLUT;
layout(set = 1, binding = 5) uniform samplerCube irradianceMap;
layout(set = 1, binding = 6) uniform samplerCube prefilteredMap;
layout(set = 1, binding = 7) uniform sampler2D aoTexture;

layout(set = 2, binding = 0, rgba16f) uniform image2D hdrImage;

const float PI = 3.14159265359;

// --- [SSR 屏幕空间反射] ---
// 基于深度的射线步进，用于生成高光反射
vec3 CalculateSSR(vec3 P, vec3 N, vec3 V, float roughness, out float hitMask) {
    hitMask = 0.0;
    // 如果太粗糙，就不做SSR，直接用天空盒，节省性能
    if (roughness > 0.6) return vec3(0.0);

    vec3 R = normalize(reflect(-V, N));
    vec3 rayPos = P + N * 0.05; // 沿法线偏移一点，防止自交
    float stepSize = 0.15;      // 步长
    const int maxSteps = 30;    // 最大步进次数

    for (int i = 0; i < maxSteps; i++) {
        rayPos += R * stepSize;

        // 将世界坐标投影到屏幕空间
        vec4 clipPos = scene.proj * scene.view * vec4(rayPos, 1.0);
        vec3 ndc = clipPos.xyz / clipPos.w;
        vec2 uv = ndc.xy * 0.5 + 0.5;

        // 越界退出
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) break;

        // 采样深度/位置
        vec4 samplePos = texture(gPos, uv);
        if (samplePos.w == 0.0) continue; // 背景没有几何体

        // 检查射线是否穿透了几何体表面
        float dist = distance(rayPos, samplePos.xyz);
        if (dist < stepSize * 1.5) { // 命中阈值
            hitMask = 1.0;
            // 边缘淡出，避免屏幕边缘反射突然截断
            vec2 dUV = smoothstep(0.0, 0.1, uv) * smoothstep(1.0, 0.9, uv);
            hitMask *= dUV.x * dUV.y;
            // 返回命中的基础色 + 假光照（或者自发光）
            return texture(gAlbedo, uv).rgb * (samplePos.a * 5.0 + 0.5); 
        }
    }
    return vec3(0.0);
}

// --- [SSGI 核心算法] ---
vec3 CalculateSSGI(vec3 P, vec3 N, vec3 albedo) {
    vec3 indirectLight = vec3(0.0);
    const int SAMPLES = 8;
    float sampleRadius = 0.05; 

    for(int i = 0; i < SAMPLES; i++) {
        float angle = float(i) * (2.0 * PI / float(SAMPLES));
        vec2 offset = vec2(cos(angle), sin(angle)) * sampleRadius;
        vec2 sampleUV = inUV + offset;

        if(sampleUV.x < 0 || sampleUV.x > 1 || sampleUV.y < 0 || sampleUV.y > 1) continue;

        vec4 nPosE = texture(gPos, sampleUV);
        vec3 nNormal = texture(gNormal, sampleUV).xyz;
        vec3 nAlbedo = texture(gAlbedo, sampleUV).rgb;
        float nEmission = nPosE.a; 

        vec3 L_bounce = nPosE.xyz - P;
        float dist = length(L_bounce);
        L_bounce = normalize(L_bounce);

        float cosTheta = max(dot(N, L_bounce), 0.0);
        float cosPhi = max(dot(nNormal, -L_bounce), 0.0);
        float attenuation = 1.0 / (dist * dist + 0.1);
        
        vec3 radiance = nAlbedo * (nEmission * 5.0 + 0.2) * cosTheta * cosPhi * attenuation;
        indirectLight += radiance;
    }   
    return (indirectLight / float(SAMPLES)) * albedo;
}

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
// 为 IBL 特制的 Fresnel，考虑粗糙度，避免极粗糙物体边缘也像镜子一样反射天空
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Epic 的 EnvBRDF 拟合近似函数 (无需查表贴图直接计算 BRDF)
vec2 EnvBRDFApprox(float roughness, float NoV) {
    vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;
    return AB;
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
    vec3 R = reflect(-V, N); // 反射向量

    vec3 albedo = albR.rgb;
    float roughness = max(albR.a, 0.05);
    float metallic = normM.a;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);

    // --- 1. 直接光照 (太阳 + 点光源) ---
    vec4 shadowPos = scene.sunlightProj * vec4(worldPos, 1.0);
    float shadow = calculatePCSS(shadowMap, shadowPos);
    vec3 L_sun = normalize(scene.sunlightDir.xyz);
    vec3 H_sun = normalize(V + L_sun);
    float NDF_s = DistributionGGX(N, H_sun, roughness);
    float G_s = GeometrySmith(N, V, L_sun, roughness);  
    vec3 F_s = fresnelSchlick(max(dot(H_sun, V), 0.0), F0);
    vec3 spec_s = (NDF_s * G_s * F_s) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L_sun), 0.0) + 0.0001);
    vec3 kD_s = (vec3(1.0) - F_s) * (1.0 - metallic);
    // PBR 求光
    Lo += (1.0 - shadow) * (kD_s * albedo / PI + spec_s) * scene.sunlightColor.rgb * scene.sunlightColor.a * max(dot(N, L_sun), 0.0);

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

    // --- 2. 环境光照 (IBL) ---
    vec3 F_ibl = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kD_ibl = (1.0 - F_ibl) * (1.0 - metallic);

    // A. 漫反射 IBL (环境光照明)
    // unused: 技巧：如果没有卷积贴图，通过极高的 LOD 模糊天空盒来近似 Irradiance Map
    // vec3 irradiance = textureLod(skyboxMap, N, 8.0).rgb; 
    // using method : real IBL
    vec3 irradiance = texture(irradianceMap, N).rgb;

    vec3 diffuseIBL = irradiance * albedo;

    // B. 高光 IBL (反射天空)
    const float MAX_REF_LOD = 8.0; 
    // unreal lod prefilteredMap
    // vec3 prefilteredColor = textureLod(skyboxMap, R, roughness * MAX_REF_LOD).rgb;
    // real IBL prefilteredMap
    vec3 prefilteredColor = textureLod(prefilteredMap, R, roughness * 5.0).rgb;

    // vec2 envBRDF = EnvBRDFApprox(roughness, max(dot(N, V), 0.0));
    vec2 envBRDF = texture(brdfLUT, vec2(max(dot(N,V), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F_ibl * envBRDF.x + envBRDF.y);

    // --- 3. 屏幕空间反射 (SSR) ---
    float ssrMask;
    vec3 ssrColor = CalculateSSR(worldPos, N, V, roughness, ssrMask);
    // 混合逻辑：如果屏幕空间反射命中，优先使用SSR，否则使用天空盒的高光IBL
    vec3 finalSpecular = mix(specularIBL, ssrColor, ssrMask * (1.0 - roughness));

    // --- 4. 漫反射全局光照 (SSGI) ---
    vec3 indirectGI = CalculateSSGI(worldPos, N, albedo);

    // --- 5. 最终合成 ---
    float aoFactor = texture(aoTexture, inUV).r;
    vec3 ambient = (kD_ibl * diffuseIBL * aoFactor) + finalSpecular;
    vec3 emissive = albedo * posE.a * 5.0;
    
    vec3 result = ambient + Lo + emissive + indirectGI;
    // --- Bloom: 导出 HDR 用于 Bloom 管线 ---
    imageStore(hdrImage, ivec2(gl_FragCoord.xy), vec4(result, 1.0));

    // 曝光与 Gamma 校正
    result = vec3(1.0) - exp(-result * 1.5); // 简单的 Tone Mapping
    outFragColor = vec4(pow(result, vec3(1.0/2.2)), 1.0);
}