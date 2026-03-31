#pragma once

#include <vector>
#include <memory>
#include <string>

#include "RenderObject.h" 
#include "ErisPhysics.h"

class ErisEngine;

class ErisWorld {
public:
    friend class ErisEngine;
    ErisWorld();
    ~ErisWorld();


    // 创建一个新物体并加入世界，返回其原始指针以便后续操作
    RenderObject* spawnObject(Model* model, const std::string& name);

    // 获取世界中所有物体的引用
    const std::vector<std::unique_ptr<RenderObject>>& getObjects() const { return m_objects; }

    bool rayIntersectAABB(glm::vec3 origin, glm::vec3 dir, glm::vec3 min, glm::vec3 max, float& t);

    // 每帧逻辑更新（暂时留空，后续放物理模拟）
    void update(float deltaTime);
    ErisPhysics& getPhysics() { return m_physics; }


    //-----------------------LearnOpenGL Skybox----------------------------

    void createDefaultSkybox();

private:
    // 使用 unique_ptr 管理物体，确保内存安全且在 vector 扩容时地址稳定
    std::vector<std::unique_ptr<RenderObject>> m_objects;

    ErisPhysics m_physics;

    std::vector<std::string> skyboxFaces;
};