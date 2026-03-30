#include "RenderObject.h"

RenderObject::RenderObject(Model* model, const std::string& name)
	:m_pModel(model), m_name(name)
{

}


glm::mat4 RenderObject::getTransformMatrix() {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), m_location);

    // 按照 Z-Y-X 顺序应用旋转（UE5 惯例）
    m = glm::rotate(m, glm::radians(m_rotation.z), { 0, 0, 1 });
    m = glm::rotate(m, glm::radians(m_rotation.y), { 0, 1, 0 });
    m = glm::rotate(m, glm::radians(m_rotation.x), { 1, 0, 0 });

    m = glm::scale(m, m_scale);

    return m;
}

void RenderObject::resetToInitialState()
{
    m_location = m_initialLocation;
    m_rotation = m_initialRotation;
    m_scale = m_initialScale;
}

void RenderObject::getWorldBounds(glm::vec3& outMin, glm::vec3& outMax)
{
    if (!m_pModel) return;

    // 获取该物体的 Model 矩阵（含 Location/Rotation/Scale）
    glm::mat4 modelMat = getTransformMatrix();

    // 拿到原始模型（比如自行车）的局部 AABB
    glm::vec3 min = m_pModel->minBound;
    glm::vec3 max = m_pModel->maxBound;

    // 计算世界空间中 AABB 的 8 个顶点
    glm::vec3 corners[8] = {
        {min.x, min.y, min.z}, {min.x, min.y, max.z},
        {min.x, max.y, min.z}, {min.x, max.y, max.z},
        {max.x, min.y, min.z}, {max.x, min.y, max.z},
        {max.x, max.y, min.z}, {max.x, max.y, max.z}
    };

    glm::vec3 wMin(FLT_MAX), wMax(-FLT_MAX);
    for (int i = 0; i < 8; i++) {
        // 【关键】将每一个点乘上 Model 矩阵
        glm::vec3 worldPos = glm::vec3(modelMat * glm::vec4(corners[i], 1.0f));
        wMin = glm::min(wMin, worldPos);
        wMax = glm::max(wMax, worldPos);
    }

    outMin = wMin;
    outMax = wMax;

}

void RenderObject::update(float deltaTime) {
    // 暂时为空，以后可以放简单的自转逻辑

}
