#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class ECamera
{
public:
    glm::vec3 m_position{ 0.0f, 0.0f, 3.0f }; // 相机初始位置
    glm::vec3 m_front{ 0.0f, 0.0f, -1.0f };   // 朝向
    glm::vec3 m_up{ 0.0f, 1.0f, 0.0f };      // 向上矢量

    float m_yaw{ -90.0f }; // 左右旋转
    float m_pitch{ 0.0f }; // 上下旋转
    float m_fov{ 45.0f };

    glm::mat4 getViewMatrix() {
        return glm::lookAt(m_position, m_position + m_front, m_up);
    }

    glm::mat4 getProjectionMatrix(float aspect) {
        // Vulkan Y轴向下，GLM 默认向上，所以我们要对齐
        glm::mat4 proj = glm::perspective(glm::radians(m_fov), aspect, 0.1f, 100.0f);
        proj[1][1] *= -1;
        return proj;
    }
    void processKeyboard(glm::vec3 direction, float deltaTime) {
        float velocity = 2.5f * deltaTime;
        m_position += direction * velocity;
    }

    void processMouse(float xoffset, float yoffset) {
        float sensitivity = 0.1f;
        m_yaw += xoffset * sensitivity;
        m_pitch -= yoffset * sensitivity;

        // 限制俯仰角
        if (m_pitch > 89.0f) m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;

        updateCameraVectors();
    }

private:
    void updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        front.y = sin(glm::radians(m_pitch));
        front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        m_front = glm::normalize(front);
    }
};

