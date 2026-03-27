#include "ErisPhysics.h"
#include <Jolt/Physics/Body/BodyInterface.h>

#include <iostream>
#include <cstdint>

// -----------------------------------------------------------------------------
// 1. 实现：广相层接口 (BroadPhaseLayerInterface)
// -----------------------------------------------------------------------------
class ErisPhysics::BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual uint32_t GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
        return mObjectToBroadPhase[inLayer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:     return "MOVING";
        default: JPH_ASSERT(false); return "INVALID";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

// -----------------------------------------------------------------------------
// 2. 实现：物体层 vs 物体层 过滤器 (决定哪些 ObjectLayer 互相碰撞)
// -----------------------------------------------------------------------------
class ErisPhysics::ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override {
        switch (inLayer1) {
        case Layers::NON_MOVING:
            return inLayer2 == Layers::MOVING; // 静态只跟动态撞
        case Layers::MOVING:
            return true; // 动态跟所有人（静态和动态）都撞
        default:
            return false;
        }
    }
};

// -----------------------------------------------------------------------------
// 3. 实现：物体层 vs 广相层 过滤器 (用于加速碰撞检测的粗略过滤)
// -----------------------------------------------------------------------------
class ErisPhysics::ObjectVsBPLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING; // 静态层只检查与动态广相层的碰撞
        case Layers::MOVING:
            return true; // 动态层检查所有碰撞
        default:
            return false;
        }
    }
};



void ErisPhysics::init() {
    // 1. 注册内存分配器和工厂
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // 2. 创建任务系统（支持多线程）
    m_tempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
    m_jobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

    // 3. 创建过滤器实例
    m_bpInterface = new BPLayerInterfaceImpl();
    m_objLayerFilter = new ObjectLayerPairFilterImpl();
    m_objVsBPFilter = new ObjectVsBPLayerFilterImpl();

    // 4. 初始化物理系统
    const uint maxBodies = 1024;
    const uint numBodyMutexes = 0;
    const uint maxBodyPairs = 1024;
    const uint maxContactConstraints = 1024;

    m_physicsSystem.Init(maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints,
        *m_bpInterface, *m_objVsBPFilter, *m_objLayerFilter);
}

JPH::BodyID ErisPhysics::createBox(glm::vec3 position, glm::vec3 halfExtent, bool isStatic) {
    JPH::BodyInterface& body_interface = m_physicsSystem.GetBodyInterface();

    // 创建形状
    JPH::BoxShapeSettings shape_settings(JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
    JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
    JPH::ShapeRefC shape = shape_result.Get();

    // 设置创建参数
    JPH::EMotionType motionType = isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;
    JPH::ObjectLayer layer = isStatic ? Layers::NON_MOVING : Layers::MOVING;

    JPH::BodyCreationSettings settings(shape, JPH::RVec3(position.x, position.y, position.z), JPH::Quat::sIdentity(), motionType, layer);

    // 创建并加入世界
    JPH::Body* body = body_interface.CreateBody(settings);
    body_interface.AddBody(body->GetID(), JPH::EActivation::Activate);

    return body->GetID();
}

void ErisPhysics::update(float deltaTime) {
    // Jolt 推荐使用固定的时间步进 (例如 60Hz)
    float step = std::min(deltaTime, 1.0f / 30.0f);
    m_physicsSystem.Update(step, 1, m_tempAllocator, m_jobSystem);
}

void ErisPhysics::getTransform(JPH::BodyID bodyID, glm::vec3& outPos, glm::vec3& outRot) {
    JPH::RVec3 pos;
    JPH::Quat rot;
    m_physicsSystem.GetBodyInterface().GetPositionAndRotation(bodyID, pos, rot);

    outPos = glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());

    // 将四元数转为欧拉角 (GLM)
    glm::quat gRot(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
    outRot = glm::degrees(glm::eulerAngles(gRot));
}

void ErisPhysics::cleanup() {
    // 按顺序销毁
    delete m_objVsBPFilter;
    delete m_objLayerFilter;
    delete m_bpInterface;
    delete m_jobSystem;
    delete m_tempAllocator;
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}