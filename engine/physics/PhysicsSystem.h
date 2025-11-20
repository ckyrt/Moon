#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>

#include "core/Scene/Transform.h"
#include "core/Math/Vector3.h"
#include "core/Math/Quaternion.h"

namespace Moon {

    class PhysicsSystem {
    public:
        PhysicsSystem();
        ~PhysicsSystem();

        void Init();
        void Shutdown();
        void Step(float dt);

        // === 创建刚体 ===
        JPH::BodyID CreateRigidBody_Box(const Transform& t, const Vector3& halfExtent, float mass);
        JPH::BodyID CreateRigidBody_Sphere(const Transform& t, float radius, float mass);
        JPH::BodyID CreateRigidBody_Capsule(const Transform& t, float radius, float halfHeight, float mass);
        JPH::BodyID CreateRigidBody_Cylinder(const Transform& t, float radius, float halfHeight, float mass);

        // === 刚体管理 ===
        void RemoveBody(JPH::BodyID id);
        void ActivateBody(JPH::BodyID id);
        void DeactivateBody(JPH::BodyID id);
        
        // === 物理操作 ===
        void AddForce(JPH::BodyID id, const Vector3& force);
        void AddImpulse(JPH::BodyID id, const Vector3& impulse);
        void SetLinearVelocity(JPH::BodyID id, const Vector3& velocity);
        Vector3 GetLinearVelocity(JPH::BodyID id) const;
        void SetAngularVelocity(JPH::BodyID id, const Vector3& angularVelocity);
        Vector3 GetAngularVelocity(JPH::BodyID id) const;
        
        // === 同步 ===
        void UpdateTransformFromPhysics(Transform& dst, JPH::BodyID id);

    private:
        JPH::PhysicsSystem m_Physics;
        JPH::JobSystemThreadPool* m_JobSystem;
        JPH::TempAllocatorImpl* m_TempAllocator;
        JPH::BodyInterface* m_BodyInterface;

        // Layer filter implementations (must be members, not local statics)
        class BroadPhaseLayerInterfaceImpl;
        class ObjectVsBroadPhaseLayerFilterImpl;
        class ObjectLayerPairFilterImpl;
        
        BroadPhaseLayerInterfaceImpl* m_BroadPhaseLayerInterface;
        ObjectVsBroadPhaseLayerFilterImpl* m_ObjectVsBroadPhaseLayerFilter;
        ObjectLayerPairFilterImpl* m_ObjectLayerPairFilter;
    };

} // namespace Moon
