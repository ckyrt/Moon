#include "RigidBody.h"
#include "PhysicsSystem.h"
#include "../core/Scene/SceneNode.h"
#include "../core/Scene/Transform.h"
#include "../core/Logging/Logger.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>

namespace Moon {

    RigidBody::RigidBody(SceneNode* owner)
        : Component(owner)
        , m_physicsSystem(nullptr)
        , m_bodyID(JPH::BodyID::cInvalidBodyID)
        , m_shapeType(PhysicsShapeType::Box)
        , m_size(1.0f, 1.0f, 1.0f)
        , m_mass(1.0f)
        , m_syncToTransform(true)
    {
    }

    RigidBody::~RigidBody()
    {
        DestroyBody();
    }

    void RigidBody::CreateBody(
        PhysicsSystem* physicsSystem,
        PhysicsShapeType shapeType,
        const Vector3& size,
        float mass
    )
    {
        if (!physicsSystem) {
            MOON_LOG_ERROR("RigidBody", "PhysicsSystem is null!");
            return;
        }

        // 如果已存在，先销毁
        DestroyBody();

        m_physicsSystem = physicsSystem;
        m_shapeType = shapeType;
        m_size = size;
        m_mass = mass;

        // 获取 Transform
        Transform* transform = m_owner->GetTransform();
        if (!transform) {
            MOON_LOG_ERROR("RigidBody", "Owner has no Transform!");
            return;
        }

        // 根据形状类型创建物理体
        switch (shapeType) {
        case PhysicsShapeType::Box:
            m_bodyID = m_physicsSystem->CreateRigidBody_Box(*transform, size, mass);
            break;
        case PhysicsShapeType::Sphere:
            m_bodyID = m_physicsSystem->CreateRigidBody_Sphere(*transform, size.x, mass);
            break;
        case PhysicsShapeType::Capsule:
            m_bodyID = m_physicsSystem->CreateRigidBody_Capsule(*transform, size.x, size.y, mass);
            break;
        case PhysicsShapeType::Cylinder:
            m_bodyID = m_physicsSystem->CreateRigidBody_Cylinder(*transform, size.x, size.y, mass);
            break;
        default:
            MOON_LOG_ERROR("RigidBody", "Unknown shape type!");
            return;
        }

        if (m_bodyID.IsInvalid()) {
            MOON_LOG_ERROR("RigidBody", "Failed to create physics body!");
        } else {
            MOON_LOG_INFO("RigidBody", ("Created physics body for: " + m_owner->GetName()).c_str());
        }
    }

    void RigidBody::DestroyBody()
    {
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            m_physicsSystem->RemoveBody(m_bodyID);
            m_bodyID = JPH::BodyID();
        }
    }

    void RigidBody::AddForce(const Vector3& force)
    {
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            m_physicsSystem->AddForce(m_bodyID, force);
        }
    }

    void RigidBody::AddForceAtPosition(const Vector3& force, const Vector3& position)
    {
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            m_physicsSystem->AddForceAtPosition(m_bodyID, force, position);
        }
    }

    void RigidBody::AddImpulse(const Vector3& impulse)
    {
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            m_physicsSystem->AddImpulse(m_bodyID, impulse);
        }
    }

    void RigidBody::SetLinearVelocity(const Vector3& velocity)
    {
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            m_physicsSystem->SetLinearVelocity(m_bodyID, velocity);
        }
    }

    Vector3 RigidBody::GetLinearVelocity() const
    {
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            return m_physicsSystem->GetLinearVelocity(m_bodyID);
        }
        return Vector3(0, 0, 0);
    }

    void RigidBody::SetAngularVelocity(const Vector3& angularVelocity)
    {
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            m_physicsSystem->SetAngularVelocity(m_bodyID, angularVelocity);
        }
    }

    Vector3 RigidBody::GetAngularVelocity() const
    {
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            return m_physicsSystem->GetAngularVelocity(m_bodyID);
        }
        return Vector3(0, 0, 0);
    }

    Vector3 RigidBody::GetPosition() const
    {
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            return m_physicsSystem->GetPosition(m_bodyID);
        }
        return m_owner && m_owner->GetTransform() ? m_owner->GetTransform()->GetWorldPosition() : Vector3(0, 0, 0);
    }

    Quaternion RigidBody::GetRotation() const
    {
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            return m_physicsSystem->GetRotation(m_bodyID);
        }
        return m_owner && m_owner->GetTransform() ? m_owner->GetTransform()->GetWorldRotation() : Quaternion::Identity();
    }

    void RigidBody::SetPositionRotation(const Vector3& position, const Quaternion& rotation)
    {
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            m_physicsSystem->SetPositionRotation(m_bodyID, position, rotation);
        }

        Transform* transform = m_owner ? m_owner->GetTransform() : nullptr;
        if (transform) {
            transform->SetWorldPosition(position);
            transform->SetWorldRotation(rotation);
        }
    }

    void RigidBody::SyncFromPhysics()
    {
        if (m_syncToTransform && m_mass > 0.0f && !m_bodyID.IsInvalid()) {
            Transform* transform = m_owner->GetTransform();
            if (transform && m_physicsSystem) {
                m_physicsSystem->UpdateTransformFromPhysics(*transform, m_bodyID);
            }
        }
    }

    void RigidBody::OnEnable()
    {
        // 组件启用时激活物理体
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            m_physicsSystem->ActivateBody(m_bodyID);
        }
    }

    void RigidBody::OnDisable()
    {
        // 组件禁用时停用物理体
        if (m_physicsSystem && !m_bodyID.IsInvalid()) {
            m_physicsSystem->DeactivateBody(m_bodyID);
        }
    }

    void RigidBody::Update(float deltaTime)
    {
        (void)deltaTime;
    }

} // namespace Moon
