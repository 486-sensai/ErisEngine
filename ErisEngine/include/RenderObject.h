#pragma once
#include "VulkanType.h" // 需要用到 Model 指针


#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

class RenderObject {
public:
    RenderObject(Model* model, const std::string& name);

    // 默认属性
    glm::vec3 m_initialLocation{ 0.0f };
    glm::vec3 m_initialRotation{ 0.0f };
    glm::vec3 m_initialScale{ 1.0f, 1.0f, 1.0f };

    // 变换属性
    glm::vec3 m_location{ 0.0f, 0.0f, 0.0f };
    glm::vec3 m_rotation{ 0.0f, 0.0f, 0.0f }; // 欧拉角 (度)
    glm::vec3 m_scale{ 1.0f, 1.0f, 1.0f };
    glm::vec3 m_localCenter{ 0.0f,0.0f,0.0f };

    std::string m_name;
    Model* m_pModel{ nullptr };

    bool m_physicsEnabled = false;
    JPH::BodyID m_bodyID;



    // 核心函数：计算并返回该物体的模型矩阵
    glm::mat4 getTransformMatrix();

    void resetToInitialState();
    void getWorldBounds(glm::vec3& outMin, glm::vec3& outMax);

    // 可以预留一些逻辑，比如更新动画
    void update(float deltaTime);
};