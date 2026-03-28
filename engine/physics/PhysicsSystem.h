#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/PhysicsStepListener.h>

#include "core/Math/Quaternion.h"
#include "core/Math/Vector3.h"
#include "core/Scene/Transform.h"

namespace Moon {

class PhysicsSystem {
public:
    PhysicsSystem();
    ~PhysicsSystem();

    void Init();
    void Shutdown();
    void Step(float dt);

    JPH::BodyID CreateRigidBody_Box(const Transform& t, const Vector3& halfExtent, float mass);
    JPH::BodyID CreateRigidBody_Sphere(const Transform& t, float radius, float mass);
    JPH::BodyID CreateRigidBody_Capsule(const Transform& t, float radius, float halfHeight, float mass);
    JPH::BodyID CreateRigidBody_Cylinder(const Transform& t, float radius, float halfHeight, float mass);
    JPH::BodyID CreateStaticHeightField(const float* samples, uint32_t sampleCount, const Vector3& offset, const Vector3& scale);

    void RemoveBody(JPH::BodyID id);
    void ActivateBody(JPH::BodyID id);
    void DeactivateBody(JPH::BodyID id);

    void AddForce(JPH::BodyID id, const Vector3& force);
    void AddForceAtPosition(JPH::BodyID id, const Vector3& force, const Vector3& position);
    void AddImpulse(JPH::BodyID id, const Vector3& impulse);
    void SetLinearVelocity(JPH::BodyID id, const Vector3& velocity);
    Vector3 GetLinearVelocity(JPH::BodyID id) const;
    void SetAngularVelocity(JPH::BodyID id, const Vector3& angularVelocity);
    Vector3 GetAngularVelocity(JPH::BodyID id) const;
    Vector3 GetPosition(JPH::BodyID id) const;
    Quaternion GetRotation(JPH::BodyID id) const;
    void SetPositionRotation(JPH::BodyID id, const Vector3& position, const Quaternion& rotation);
    JPH::Body* TryGetBody(JPH::BodyID id);
    const JPH::Body* TryGetBody(JPH::BodyID id) const;
    void AddConstraint(JPH::Constraint* constraint);
    void RemoveConstraint(JPH::Constraint* constraint);
    void AddStepListener(JPH::PhysicsStepListener* listener);
    void RemoveStepListener(JPH::PhysicsStepListener* listener);

    void UpdateTransformFromPhysics(Transform& dst, JPH::BodyID id);

    JPH::PhysicsSystem& GetJoltSystem() { return m_Physics; }
    const JPH::PhysicsSystem& GetJoltSystem() const { return m_Physics; }
    JPH::BodyInterface& GetBodyInterface() { return *m_BodyInterface; }
    const JPH::BodyInterface& GetBodyInterface() const { return *m_BodyInterface; }

private:
    JPH::PhysicsSystem m_Physics;
    JPH::JobSystemThreadPool* m_JobSystem;
    JPH::TempAllocatorImpl* m_TempAllocator;
    JPH::BodyInterface* m_BodyInterface;

    class BroadPhaseLayerInterfaceImpl;
    class ObjectVsBroadPhaseLayerFilterImpl;
    class ObjectLayerPairFilterImpl;

    BroadPhaseLayerInterfaceImpl* m_BroadPhaseLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl* m_ObjectVsBroadPhaseLayerFilter;
    ObjectLayerPairFilterImpl* m_ObjectLayerPairFilter;
};

} // namespace Moon
