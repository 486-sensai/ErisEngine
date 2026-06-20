#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 worldPos; 
layout(location = 4) in vec4 fragPosLightSpace;

struct GPUPointLight {
    vec4 position;
    vec4 color;
};

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
    mat4 sunlightProj;
    vec4 viewPos;   // 相机世界坐标
    vec4 fogColor;
    vec4 ambientColor;
    vec4 sunlightDir;
    vec4 sunlightColor;
    GPUPointLight pointLights[8];
    int lightCount;
} scene;

layout(set = 0, binding = 1) uniform sampler2D shadowMap;
layout(set = 1, binding = 0) uniform sampler2D texSampler;
layout(set = 2, binding = 0) uniform samplerCube skyboxMap;
layout(set = 2, binding = 1) uniform sampler2D brdfLUT;

layout(location = 0) out vec4 outFragColor;

// 接收材质参数
layout(push_constant) uniform constants {
    mat4 render_matrix;
    mat4 model_matrix;
    vec4 materialParams; // x: 粗糙度(Roughness), y: 金属度(Metallic), z: 发光(Emission), w: 占位
} PushConstants;

const float PI = 3.14159265359;

// --- PBR 核心函数 ---
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// 柔和软阴影 (PCF)
float calculateShadow(vec4 shadowPos, vec3 N, vec3 L) {
    vec3 projCoords = shadowPos.xyz / shadowPos.w;
    vec2 uv = projCoords.xy * 0.5 + 0.5;
    if (projCoords.z > 1.0 || uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
    
    float currentDepth = projCoords.z;
    // 动态 Bias 消除网格纹
    float bias = max(0.005 * (1.0 - dot(N, L)), 0.001);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, uv + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    return shadow / 9.0;
}

void main() {
    // 1. 基础属性准备
    vec4 texColor = texture(texSampler, fragUV);
    vec3 albedo = pow(texColor.rgb * fragColor, vec3(2.2)); // 转换到线性空间
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(scene.viewPos.xyz - worldPos);

    // 提取 PushConstant 里的材质属性
    float roughness = clamp(PushConstants.materialParams.x, 0.05, 1.0);
    float metallic  = clamp(PushConstants.materialParams.y, 0.0, 1.0);
    float emission  = PushConstants.materialParams.z;

    // F0 是基础反射率 (非金属为 0.04，金属为本身颜色)
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0); // 出射光总能量
    float shadow = calculateShadow(fragPosLightSpace, N, normalize(scene.sunlightDir.xyz));  

    // --- 2. 太阳光 PBR 计算 ---
    vec3 L_sun = normalize(scene.sunlightDir.xyz);
    vec3 H_sun = normalize(V + L_sun);
    vec3 radiance_sun = scene.sunlightColor.rgb * scene.sunlightColor.a * 5.0; // 太阳比较亮

    float NDF_sun = DistributionGGX(N, H_sun, roughness);       
    float G_sun   = GeometrySmith(N, V, L_sun, roughness); 
    vec3 F_sun    = fresnelSchlick(max(dot(H_sun, V), 0.0), F0);       

    vec3 kS_sun = F_sun;
    vec3 kD_sun = vec3(1.0) - kS_sun;
    kD_sun *= 1.0 - metallic;	  

    vec3 numerator_sun    = NDF_sun * G_sun * F_sun;
    float denominator_sun = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L_sun), 0.0) + 0.0001;
    vec3 specular_sun     = numerator_sun / denominator_sun;  

    float NdotL_sun = max(dot(N, L_sun), 0.0);                
    Lo += (1.0 - shadow) * (kD_sun * albedo / PI + specular_sun) * radiance_sun * NdotL_sun;

    // --- 3. 点光源 PBR 计算 ---
    for(int i = 0; i < scene.lightCount; i++) {
        vec3 lightVec = scene.pointLights[i].position.xyz - worldPos;
        vec3 L = normalize(lightVec);
        vec3 H = normalize(V + L);
        
        float distance = length(lightVec);
        float attenuation = 1.0 / (distance * distance); // 物理正确的平方反比衰减
        vec3 radiance = scene.pointLights[i].color.rgb * scene.pointLights[i].color.a * attenuation;

        float NDF = DistributionGGX(N, H, roughness);       
        float G   = GeometrySmith(N, V, L, roughness); 
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;  

        float NdotL = max(dot(N, L), 0.0);                
        Lo += (1.0 - shadow) * (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // --- 4. 环境光与自发光 ---
    // IBL diffuse
    vec3 N_ibl = N;
    vec3 V_ibl = V;
    vec3 F0_ibl =mix(vec3(0.04), albedo, metallic);
    vec3 F_ibl = fresnelSchlick(max(dot(N_ibl, V_ibl), 0.0), F0_ibl);
    vec3 kD_ibl = (vec3(1.0) - F_ibl) * (1.0 - metallic);

    vec3 irradiance = textureLod(skyboxMap, N_ibl, 8.0).rgb;
    vec3 diffuseIBL = irradiance * albedo;
    
    // IBL specular
    vec3 R_ibl = reflect(-V_ibl, N_ibl);
    vec3 prefilteredColor = textureLod(skyboxMap, R_ibl, 0.0).rgb;
    vec2 envBRDF = texture(brdfLUT, vec2(max(dot(N_ibl, V_ibl), 0.0), roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F_ibl * envBRDF.x + envBRDF.y);

    vec3 ambient = kD_ibl * diffuseIBL + specularIBL;
    // vec3 ambient = vec3(0.03) * albedo * scene.ambientColor.rgb * scene.ambientColor.a;
    vec3 emissive = albedo * emission * 5.0; // 自发光

    vec3 color = ambient + Lo + emissive;

    // --- 5. HDR 色调映射与 Gamma 校正 ---
    // PBR 计算都在线性空间，必须映射回 sRGB 才能显示正确的颜色
    color = color / (color + vec3(1.0)); // Reinhard Tone Mapping
    color = pow(color, vec3(1.0/2.2));   // Gamma Correction
    
    outFragColor = vec4(color, 1.0);
}