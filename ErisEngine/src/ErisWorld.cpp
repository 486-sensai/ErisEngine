#include "ErisWorld.h"

ErisWorld::ErisWorld()
{
    m_physics.init();
}

ErisWorld::~ErisWorld()
{
    for (auto& obj : m_objects) {
        if (obj->m_physicsEnabled) {
            m_physics.destroyBody(obj->m_bodyID);
        }
    }
    
    createDefaultSkybox();

    m_physics.cleanup();
}

RenderObject* ErisWorld::spawnObject(Model* model, const std::string& name) {
    auto newObject = std::make_unique<RenderObject>(model, name);
    RenderObject* ptr = newObject.get();

    ptr->m_initialLocation = ptr->m_location;
    ptr->m_initialRotation = ptr->m_rotation;
    ptr->m_initialScale = ptr->m_scale;

    m_objects.push_back(std::move(newObject));
    return ptr;
}

bool ErisWorld::rayIntersectAABB(glm::vec3 origin, glm::vec3 dir, glm::vec3 min, glm::vec3 max, float& t) {
    float tmin = (min.x - origin.x) / dir.x;
    float tmax = (max.x - origin.x) / dir.x;
    if (tmin > tmax) std::swap(tmin, tmax);

    float tymin = (min.y - origin.y) / dir.y;
    float tymax = (max.y - origin.y) / dir.y;
    if (tymin > tymax) std::swap(tymin, tymax);

    if ((tmin > tymax) || (tymin > tmax)) return false;
    if (tymin > tmin) tmin = tymin;
    if (tymax < tmax) tmax = tymax;

    float tzmin = (min.z - origin.z) / dir.z;
    float tzmax = (max.z - origin.z) / dir.z;
    if (tzmin > tzmax) std::swap(tzmin, tzmax);

    if ((tmin > tzmax) || (tzmin > tmax)) return false;

    t = tmin;
    return (t >= 0);
}

void ErisWorld::update(float deltaTime) {
     // 1. 物理引擎向前步进
    m_physics.update(deltaTime);

    // 2. 将物理计算后的结果写回给 RenderObject
    for (auto& obj : m_objects) {
        if (obj->m_physicsEnabled) {
            glm::vec3 newPos, newRot;
            m_physics.getTransform(obj->m_bodyID, newPos, newRot);

            glm::mat4 rotationMat = glm::mat4(1.0f);
            rotationMat = glm::rotate(rotationMat, glm::radians(newRot.z), { 0,0,1 });
            rotationMat = glm::rotate(rotationMat, glm::radians(newRot.y), { 0,1,0 });
            rotationMat = glm::rotate(rotationMat, glm::radians(newRot.x), { 1,0,0 });
            
            glm::vec3 rotatedOffset = glm::vec3(rotationMat * glm::vec4(obj->m_localCenter, 1.0f));

            obj->m_location = newPos-rotatedOffset;
            obj->m_rotation = newRot;
        }
    }
}

void ErisWorld::createDefaultSkybox()
{
    // 使用的是 LearnOpenGL Skybox 所使用的skybox贴图

    skyboxFaces = {
        "assets/skybox/right.jpg",
        "assets/skybox/left.jpg",
        "assets/skybox/top.jpg",
        "assets/skybox/bottom.jpg",
        "assets/skybox/front.jpg",
        "assets/skybox/back.jpg"
    };
    
}
