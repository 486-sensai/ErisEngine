// pcss.glsl --- PCSS 软阴影通用函数
#ifndef PCSS_GLSL
#define PCSS_GLSL

const float LIGHT_SIZE = 0.03;
const int BLOCKER_SAMPLES = 12;
const int PCF_SAMPLES = 20;

float findBlocker(sampler2D shadowMap, vec2 uv, float zFrag, float searchRadius) {
    float blockerSum = 0.0;
    int blockerCount = 0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int i = 0; i < BLOCKER_SAMPLES; i++) {
        float angle = float(i) * 2.3999;
        vec2 offset = vec2(cos(angle), sin(angle)) * texelSize * searchRadius;
        float depth = texture(shadowMap, uv + offset).r;
        if (depth < zFrag - 0.001) {
            blockerSum += depth;
            blockerCount++;
        }
    }
    return blockerCount > 0 ? blockerSum / float(blockerCount) : 1.0;
}

float calculatePCSS(sampler2D shadowMap, vec4 shadowPos) {
    vec3 projCoords = shadowPos.xyz / shadowPos.w;
    vec2 uv = projCoords.xy * 0.5 + 0.5;
    float zFrag = projCoords.z;
    if (zFrag > 1.0 || uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;

    float avgBlockerDepth = findBlocker(shadowMap, uv, zFrag, 5.0);
    float penumbra = (zFrag - avgBlockerDepth) / max(avgBlockerDepth, 0.001) * LIGHT_SIZE;
    float filterRadius = clamp(penumbra, 1.0, 16.0);

    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    float shadow = 0.0;
    for (int i = 0; i < PCF_SAMPLES; i++) {
        float angle = float(i) * 2.3999;
        vec2 offset = vec2(cos(angle), sin(angle)) * texelSize * filterRadius;
        float depth = texture(shadowMap, uv + offset).r;
        shadow += zFrag - 0.001 > depth ? 1.0 : 0.0;
    }
    return shadow / float(PCF_SAMPLES);
}
#endif