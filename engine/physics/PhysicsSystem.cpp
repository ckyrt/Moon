#include "PhysicsSystem.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <cassert>

namespace Moon {

    // ============================
    // 几个基本参数
    // ============================
    static constexpr JPH::ObjectLayer LAYER_STATIC = 0;
    static constexpr JPH::ObjectLayer LAYER_DYNAMIC = 1;

    static constexpr uint32_t NUM_OBJECT_LAYERS = 2;
    static constexpr uint32_t NUM_BROAD_PHASE = 2;

    // ============================
    // BroadPhaseLayerInterface
    // ============================
    class PhysicsSystem::BroadPhaseLayerInterfaceImpl : public JPH::BroadPhaseLayerInterface {
    public:
        BroadPhaseLayerInterfaceImpl() {
            mMap[LAYER_STATIC] = JPH::BroadPhaseLayer(0);
            mMap[LAYER_DYNAMIC] = JPH::BroadPhaseLayer(1);
        }

        uint32_t GetNumBroadPhaseLayers() const override { return NUM_BROAD_PHASE; }

        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
            assert(layer < NUM_OBJECT_LAYERS);
            return mMap[layer];
        }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
            switch ((JPH::BroadPhaseLayer::Type)layer) {
            case 0: return "STATIC";
            case 1: return "DYNAMIC";
            default: return "UNKNOWN";
            }
        }
#endif

    private:
        JPH::BroadPhaseLayer mMap[NUM_OBJECT_LAYERS];
    };

    // ============================
    // Object Vs BroadPhase
    // ============================
    class PhysicsSystem::ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override {
            return true;
        }
    };

    // ============================
    // ObjectLayerPairFilter
    // ============================
    class PhysicsSystem::ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
    public:
        bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override {
            return true;
        }
    };

    // ======================================================
    // 构造 / 析构
    // ======================================================
    PhysicsSystem::PhysicsSystem()
        : m_JobSystem(nullptr)
        , m_TempAllocator(nullptr)
        , m_BodyInterface(nullptr)
        , m_BroadPhaseLayerInterface(nullptr)
        , m_ObjectVsBroadPhaseLayerFilter(nullptr)
        , m_ObjectLayerPairFilter(nullptr)
    {
    }

    PhysicsSystem::~PhysicsSystem() { Shutdown(); }

    // ======================================================
    // Init
    // ======================================================
    void PhysicsSystem::Init() {
        JPH::RegisterDefaultAllocator();

        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        m_TempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
        m_JobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 4);

        // Create layer filter implementations as heap objects
        m_BroadPhaseLayerInterface = new BroadPhaseLayerInterfaceImpl();
        m_ObjectVsBroadPhaseLayerFilter = new ObjectVsBroadPhaseLayerFilterImpl();
        m_ObjectLayerPairFilter = new ObjectLayerPairFilterImpl();

        m_Physics.Init(
            8192,        // max bodies
            0,           // num body mutexes (0 = default)
            2048,        // max body pairs
            2048,        // max contact constraints
            *m_BroadPhaseLayerInterface,
            *m_ObjectVsBroadPhaseLayerFilter,
            *m_ObjectLayerPairFilter
        );

        m_Physics.SetGravity(JPH::Vec3(0, -9.8f, 0));
        m_BodyInterface = &m_Physics.GetBodyInterface();
    }

    // ======================================================
    // Shutdown
    // ======================================================
    void PhysicsSystem::Shutdown() {
        if (JPH::Factory::sInstance)
        {
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }

        delete m_JobSystem;
        m_JobSystem = nullptr;

        delete m_TempAllocator;
        m_TempAllocator = nullptr;

        delete m_BroadPhaseLayerInterface;
        m_BroadPhaseLayerInterface = nullptr;

        delete m_ObjectVsBroadPhaseLayerFilter;
        m_ObjectVsBroadPhaseLayerFilter = nullptr;

        delete m_ObjectLayerPairFilter;
        m_ObjectLayerPairFilter = nullptr;

        m_BodyInterface = nullptr;
    }

    // ======================================================
    // Step
    // ======================================================
    void PhysicsSystem::Step(float dt) {
        m_Physics.Update(dt, 1, m_TempAllocator, m_JobSystem);
    }

    // ======================================================
    // 工具：将 Transform 转换成 JPH 值
    // ======================================================
    static inline JPH::RVec3 ToJoltPos(Transform& tr) {
        Vector3 p = tr.GetWorldPosition();
        return JPH::RVec3(p.x, p.y, p.z);
    }

    static inline JPH::Quat ToJoltRot(Transform& tr) {
        Quaternion q = tr.GetWorldRotation();
        return JPH::Quat(q.x, q.y, q.z, q.w);
    }

    // ======================================================
    // Box
    // ======================================================
    JPH::BodyID PhysicsSystem::CreateRigidBody_Box(
        const Transform& tr,
        const Vector3& halfExtent,
        float mass
    ) {
        JPH::BoxShapeSettings shapeSettings(JPH::Vec3(halfExtent.x, halfExtent.y, halfExtent.z));
        auto shape = shapeSettings.Create().Get();

        Transform& transform = const_cast<Transform&>(tr);
        JPH::BodyCreationSettings settings(
            shape,
            ToJoltPos(transform),
            ToJoltRot(transform),
            mass > 0 ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
            mass > 0 ? LAYER_DYNAMIC : LAYER_STATIC
        );

        if (mass > 0) {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
            settings.mMassPropertiesOverride.mMass = mass;
        }

        return m_BodyInterface->CreateAndAddBody(settings, JPH::EActivation::Activate);
    }

    // ======================================================
    // Sphere
    // ======================================================
    JPH::BodyID PhysicsSystem::CreateRigidBody_Sphere(
        const Transform& tr,
        float radius,
        float mass
    ) {
        JPH::SphereShapeSettings shapeSettings(radius);
        auto shape = shapeSettings.Create().Get();

        Transform& transform = const_cast<Transform&>(tr);
        JPH::BodyCreationSettings settings(
            shape,
            ToJoltPos(transform),
            ToJoltRot(transform),
            mass > 0 ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
            mass > 0 ? LAYER_DYNAMIC : LAYER_STATIC
        );

        if (mass > 0) {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
            settings.mMassPropertiesOverride.mMass = mass;
        }

        return m_BodyInterface->CreateAndAddBody(settings, JPH::EActivation::Activate);
    }

    // ======================================================
    // Remove
    // ======================================================
    void PhysicsSystem::RemoveBody(JPH::BodyID id) {
        m_BodyInterface->RemoveBody(id);
        m_BodyInterface->DestroyBody(id);
    }

    // ======================================================
    // Capsule
    // ======================================================
    JPH::BodyID PhysicsSystem::CreateRigidBody_Capsule(
        const Transform& tr,
        float radius,
        float halfHeight,
        float mass
    ) {
        JPH::CapsuleShapeSettings shapeSettings(halfHeight, radius);
        auto shape = shapeSettings.Create().Get();

        Transform& transform = const_cast<Transform&>(tr);
        JPH::BodyCreationSettings settings(
            shape,
            ToJoltPos(transform),
            ToJoltRot(transform),
            mass > 0 ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
            mass > 0 ? LAYER_DYNAMIC : LAYER_STATIC
        );

        if (mass > 0) {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
            settings.mMassPropertiesOverride.mMass = mass;
        }

        return m_BodyInterface->CreateAndAddBody(settings, JPH::EActivation::Activate);
    }

    // ======================================================
    // Cylinder
    // ======================================================
    JPH::BodyID PhysicsSystem::CreateRigidBody_Cylinder(
        const Transform& tr,
        float radius,
        float halfHeight,
        float mass
    ) {
        JPH::CylinderShapeSettings shapeSettings(halfHeight, radius);
        auto shape = shapeSettings.Create().Get();

        Transform& transform = const_cast<Transform&>(tr);
        JPH::BodyCreationSettings settings(
            shape,
            ToJoltPos(transform),
            ToJoltRot(transform),
            mass > 0 ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
            mass > 0 ? LAYER_DYNAMIC : LAYER_STATIC
        );

        if (mass > 0) {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
            settings.mMassPropertiesOverride.mMass = mass;
        }

        return m_BodyInterface->CreateAndAddBody(settings, JPH::EActivation::Activate);
    }

    // ======================================================
    // 激活/停用
    // ======================================================
    void PhysicsSystem::ActivateBody(JPH::BodyID id) {
        m_BodyInterface->ActivateBody(id);
    }

    void PhysicsSystem::DeactivateBody(JPH::BodyID id) {
        m_BodyInterface->DeactivateBody(id);
    }

    // ======================================================
    // 施加力和冲量
    // ======================================================
    void PhysicsSystem::AddForce(JPH::BodyID id, const Vector3& force) {
        m_BodyInterface->AddForce(id, JPH::Vec3(force.x, force.y, force.z));
    }

    void PhysicsSystem::AddImpulse(JPH::BodyID id, const Vector3& impulse) {
        m_BodyInterface->AddImpulse(id, JPH::Vec3(impulse.x, impulse.y, impulse.z));
    }

    // ======================================================
    // 速度控制
    // ======================================================
    void PhysicsSystem::SetLinearVelocity(JPH::BodyID id, const Vector3& velocity) {
        m_BodyInterface->SetLinearVelocity(id, JPH::Vec3(velocity.x, velocity.y, velocity.z));
    }

    Vector3 PhysicsSystem::GetLinearVelocity(JPH::BodyID id) const {
        JPH::Vec3 vel = m_BodyInterface->GetLinearVelocity(id);
        return Vector3(vel.GetX(), vel.GetY(), vel.GetZ());
    }

    void PhysicsSystem::SetAngularVelocity(JPH::BodyID id, const Vector3& angularVelocity) {
        m_BodyInterface->SetAngularVelocity(id, JPH::Vec3(angularVelocity.x, angularVelocity.y, angularVelocity.z));
    }

    Vector3 PhysicsSystem::GetAngularVelocity(JPH::BodyID id) const {
        JPH::Vec3 vel = m_BodyInterface->GetAngularVelocity(id);
        return Vector3(vel.GetX(), vel.GetY(), vel.GetZ());
    }

    // ======================================================
    // 更新 Jolt → Transform
    // ======================================================
    void PhysicsSystem::UpdateTransformFromPhysics(Transform& tr, JPH::BodyID id) {
        JPH::RVec3 pos = m_BodyInterface->GetPosition(id);
        JPH::Quat rot = m_BodyInterface->GetRotation(id);

        tr.SetWorldPosition({ pos.GetX(), pos.GetY(), pos.GetZ() });
        tr.SetWorldRotation({ rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW() });
    }

} // namespace Moon
