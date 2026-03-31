#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;

layout(set = 0, binding = 0) uniform SceneData {
    mat4 view;
    mat4 proj;
} scene;

layout(location = 0) out vec4 outColor;

float computeDepth(vec3 pos) {
    vec4 clip_space_pos = scene.proj * scene.view * vec4(pos.xyz, 1.0);
    float clip_depth = clip_space_pos.z / clip_space_pos.w;
    // 关键修正：将深度稍微减小一点点（0.9999），确保它永远小于背景清除值 1.0
    // 这样网格在没有模型的地方也能显示出来，并遮挡天空盒
    return clamp(clip_depth, 0.0, 0.99999);
}

void main() {
    float t = -nearPoint.y / (farPoint.y - nearPoint.y);
    
    // 如果射线不指向 Y=0 平面（看向天空方向），直接丢弃
    if (t <= 0) discard;

    vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);

    // 1. 写入深度，解决遮挡关系
    gl_FragDepth = computeDepth(fragPos3D);

    // 2. 计算网格线
    vec2 coord = fragPos3D.xz;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float alpha = 1.0 - min(line, 1.0);

    // 3. 【满足你的要求】：定义地面基础色，不再透明，从而遮挡天空盒
    // 背景设为深灰色
    vec4 backgroundColor = vec4(0.1, 0.1, 0.1, 1.0); 
    // 网格线设为浅灰色
    vec4 lineColor = vec4(0.3, 0.3, 0.3, 1.0);

    // 绘制坐标轴
    if(abs(fragPos3D.z) < 0.1 * derivative.y) lineColor = vec4(1.0, 0.0, 0.0, 1.0); // X轴红
    if(abs(fragPos3D.x) < 0.1 * derivative.x) lineColor = vec4(0.0, 0.0, 1.0, 1.0); // Z轴蓝

    // 混合背景和线条
    vec4 finalColor = mix(backgroundColor, lineColor, alpha);

    // 4. 边缘淡出：让地平线和天空平滑过渡，不产生硬边
    float viewDist = length(fragPos3D - nearPoint);
    float fading = max(0.0, 1.0 - (viewDist / 100.0)); // 100米外淡出

    outColor = finalColor;
    outColor.a *= fading; // 整体淡出，地平线处会慢慢变透明透出天空盒
}