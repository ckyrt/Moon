#include "Transform.h"
#include "SceneNode.h"
#include <cmath>

namespace Moon {

    // =============================
    // 构造
    // =============================
    Transform::Transform(SceneNode* owner)
        : m_owner(owner)
        , m_localPosition(0, 0, 0)
        , m_localRotation(Quaternion::Identity())
        , m_localScale(1, 1, 1)
        , m_localDirty(true)
        , m_worldDirty(true)
    {
    }

    // =============================
    // Local Setters
    // =============================
    void Transform::SetLocalPosition(const Vector3& p)
    {
        m_localPosition = p;
        MarkDirty();
    }

    void Transform::SetLocalRotation(const Quaternion& q)
    {
        m_localRotation = q;
        MarkDirty();
    }

    void Transform::SetLocalScale(const Vector3& s)
    {
        m_localScale = s;
        MarkDirty();
    }

    // =============================
    // World Setters
    // =============================
    void Transform::SetWorldPosition(const Vector3& p)
    {
        SceneNode* parent = m_owner->GetParent();
        if (!parent)
        {
            m_localPosition = p;
        }
        else
        {
            Matrix4x4 parentInv = parent->GetTransform()->GetWorldMatrix().Inverse();
            m_localPosition = parentInv.MultiplyPoint(p);
        }
        MarkDirty();
    }

    void Transform::SetWorldRotation(const Quaternion& q)
    {
        SceneNode* parent = m_owner->GetParent();
        if (!parent)
        {
            m_localRotation = q;
        }
        else
        {
            Quaternion parentRot = parent->GetTransform()->GetWorldRotation();
            m_localRotation = parentRot.Inverse() * q;
        }
        MarkDirty();
    }

    void Transform::SetWorldScale(const Vector3& s)
    {
        SceneNode* parent = m_owner->GetParent();
        if (!parent)
        {
            m_localScale = s;
        }
        else
        {
            Vector3 pScale = parent->GetTransform()->GetWorldScale();
            m_localScale = Vector3(s.x / pScale.x, s.y / pScale.y, s.z / pScale.z);
        }
        MarkDirty();
    }

    // =============================
    // World Getters
    // =============================
    Vector3 Transform::GetWorldPosition()
    {
        const Matrix4x4& m = GetWorldMatrix();
        return Vector3(m.m[3][0], m.m[3][1], m.m[3][2]);
    }

    Quaternion Transform::GetWorldRotation()
    {
        return Quaternion::FromMatrix(GetWorldMatrix());
    }

    Vector3 Transform::GetWorldScale()
    {
        const Matrix4x4& m = GetWorldMatrix();
        Vector3 x(m.m[0][0], m.m[0][1], m.m[0][2]);
        Vector3 y(m.m[1][0], m.m[1][1], m.m[1][2]);
        Vector3 z(m.m[2][0], m.m[2][1], m.m[2][2]);
        return Vector3(x.Length(), y.Length(), z.Length());
    }

    // =============================
    // Direction Vectors (Unity 等效)
    // =============================
    Vector3 Transform::GetForward()
    {
        const Matrix4x4& m = GetWorldMatrix();
        return Vector3(m.m[2][0], m.m[2][1], m.m[2][2]).Normalized();
    }

    Vector3 Transform::GetRight()
    {
        const Matrix4x4& m = GetWorldMatrix();
        return Vector3(m.m[0][0], m.m[0][1], m.m[0][2]).Normalized();
    }

    Vector3 Transform::GetUp()
    {
        const Matrix4x4& m = GetWorldMatrix();
        return Vector3(m.m[1][0], m.m[1][1], m.m[1][2]).Normalized();
    }

    // =============================
    // Matrix getters
    // =============================
    const Matrix4x4& Transform::GetLocalMatrix()
    {
        if (m_localDirty)
        {
            UpdateLocalMatrix();
            m_localDirty = false;
        }
        return m_localMatrix;
    }

    const Matrix4x4& Transform::GetWorldMatrix()
    {
        if (m_localDirty)
        {
            UpdateLocalMatrix();
            m_localDirty = false;
        }
        if (m_worldDirty)
        {
            UpdateWorldMatrix();
            m_worldDirty = false;
        }
        return m_worldMatrix;
    }

    // =============================
    // Transform operations
    // =============================
    void Transform::Translate(const Vector3& v, bool worldSpace)
    {
        if (worldSpace)
        {
            m_localPosition = m_localPosition + v;
        }
        else
        {
            m_localPosition = m_localPosition + (m_localRotation * v);
        }
        MarkDirty();
    }

    void Transform::Rotate(const Vector3& eulerDeg, bool worldSpace)
    {
        Quaternion q = Quaternion::Euler(eulerDeg);

        if (worldSpace)
            m_localRotation = q * m_localRotation;
        else
            m_localRotation = m_localRotation * q;

        MarkDirty();
    }

    void Transform::LookAt(const Vector3& target, const Vector3& up)
    {
        Vector3 pos = GetWorldPosition();
        Quaternion rot = Quaternion::LookRotation((target - pos).Normalized(), up);
        SetWorldRotation(rot);
    }

    void Transform::SetLocalRotation(const Vector3& eulerDeg)
    {
        m_localRotation = Quaternion::Euler(eulerDeg);
        MarkDirty();
    }

    void Transform::SetWorldRotation(const Vector3& eulerDeg)
    {
        Quaternion q = Quaternion::Euler(eulerDeg);
        SetWorldRotation(q);
    }

    Vector3 Transform::GetLocalEulerAngles()
    {
        return m_localRotation.ToEuler();
    }

    Vector3 Transform::GetWorldEulerAngles()
    {
        return GetWorldRotation().ToEuler();
    }


    // =============================
    // Internals
    // =============================
    void Transform::MarkDirty()
    {
        m_localDirty = true;
        m_worldDirty = true;

        // 递归更新子节点世界矩阵
        for (size_t i = 0; i < m_owner->GetChildCount(); ++i)
        {
            SceneNode* child = m_owner->GetChild(i);
            if (child)
                child->GetTransform()->m_worldDirty = true;
        }
    }

    void Transform::UpdateLocalMatrix()
    {
        Matrix4x4 rot = m_localRotation.ToMatrix();

        // Scale
        rot.m[0][0] *= m_localScale.x;
        rot.m[0][1] *= m_localScale.x;
        rot.m[0][2] *= m_localScale.x;

        rot.m[1][0] *= m_localScale.y;
        rot.m[1][1] *= m_localScale.y;
        rot.m[1][2] *= m_localScale.y;

        rot.m[2][0] *= m_localScale.z;
        rot.m[2][1] *= m_localScale.z;
        rot.m[2][2] *= m_localScale.z;

        // Translation
        rot.m[3][0] = m_localPosition.x;
        rot.m[3][1] = m_localPosition.y;
        rot.m[3][2] = m_localPosition.z;
        rot.m[3][3] = 1.0f;

        m_localMatrix = rot;
    }

    void Transform::UpdateWorldMatrix()
    {
        SceneNode* parent = m_owner->GetParent();

        if (parent)
            m_worldMatrix = parent->GetTransform()->GetWorldMatrix() * m_localMatrix;
        else
            m_worldMatrix = m_localMatrix;
    }

} // namespace Moon
