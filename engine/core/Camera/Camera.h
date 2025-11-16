#pragma once
#include "../Math/Math.h"

namespace Moon {

class ICamera {
public:
    virtual ~ICamera() = default;
    virtual void SetPosition(const Vector3& p) = 0;
    virtual void SetTarget(const Vector3& t) = 0;
    virtual void SetUp(const Vector3& u) = 0;
    virtual Vector3 GetPosition() const = 0;
    virtual Vector3 GetTarget() const = 0;
    virtual Vector3 GetUp() const = 0;
    virtual Vector3 GetForward() const = 0;
    virtual Vector3 GetRight() const = 0;
    virtual Matrix4x4 GetViewMatrix() const = 0;
    virtual Matrix4x4 GetProjectionMatrix() const = 0;
    virtual Matrix4x4 GetViewProjectionMatrix() const = 0;
};

class Camera : public ICamera {
public:
    Camera();
    virtual ~Camera() = default;
    void SetPosition(const Vector3& p) override;
    void SetTarget(const Vector3& t) override;
    void SetUp(const Vector3& u) override;
    Vector3 GetPosition() const override { return m_position; }
    Vector3 GetTarget() const override { return m_target; }
    Vector3 GetUp() const override { return m_up; }
    Vector3 GetForward() const override;
    Vector3 GetRight() const override;
    Matrix4x4 GetViewMatrix() const override;
    virtual Matrix4x4 GetProjectionMatrix() const override = 0;
    Matrix4x4 GetViewProjectionMatrix() const override;
    
    // Helper method for easier camera positioning
    void LookAt(const Vector3& target);
    
protected:
    Vector3 m_position, m_target, m_up;
    mutable bool m_viewDirty = true;
    mutable bool m_projDirty = true;
    mutable Matrix4x4 m_cachedView;
    mutable Matrix4x4 m_cachedProj;
};

}