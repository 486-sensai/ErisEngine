#pragma once


#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>  
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

typedef uint32_t uint;

// 定义简单的碰撞层
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

// 2. 定义广相层 (Broadphase Layers)
// 只有大类划分，用于粗略剔除
namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr uint32_t NUM_LAYERS = 2;
};

class ErisPhysics {
public:
	void init();
    void update(float deltaTime);
    void cleanup();

    // 辅助函数：在物理世界创建一个盒子刚体
    JPH::BodyID createBox(glm::vec3 position, glm::vec3 halfExtent, bool isStatic);
    JPH::BodyID createInfinitePlane();

    void destroyBody(JPH::BodyID bodyID);

    // 获取物体的实时变换
    void getTransform(JPH::BodyID bodyID, glm::vec3& outPos, glm::vec3& outRot);
    void setTransform(JPH::BodyID bodyID, glm::vec3 position, glm::vec3 rotation);

    void resetBodyState(JPH::BodyID bodyID, glm::vec3 position, glm::vec3 rotation,glm::vec3 scale);
    void resetVelocity(JPH::BodyID bodyID);

    JPH::BodyInterface& getBodyInterface() { return m_physicsSystem.GetBodyInterface(); }

private:
    JPH::PhysicsSystem m_physicsSystem;
    JPH::TempAllocatorImpl* m_tempAllocator;
    JPH::JobSystemThreadPool* m_jobSystem;


    // Jolt 必须的过滤回调类（实现见 cpp）
    class BPLayerInterfaceImpl;
    class ObjectLayerPairFilterImpl;
    class ObjectVsBPLayerFilterImpl;


    BPLayerInterfaceImpl* m_bpInterface = nullptr;
    ObjectLayerPairFilterImpl* m_objLayerFilter = nullptr;
    ObjectVsBPLayerFilterImpl* m_objVsBPFilter = nullptr;


};
