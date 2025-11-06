#pragma once
#include "Camera.h"

namespace Moon {

class PerspectiveCamera : public Camera {
public:
    PerspectiveCamera(float fov=60, float aspect=16.0f/9.0f, float near=0.1f, float far=1000.0f);
    ~PerspectiveCamera() override = default;
    void SetFOV(float f) { m_fov = f; m_projDirty = true; }
    void SetAspectRatio(float a) { m_aspect = a; m_projDirty = true; }
    void SetNearPlane(float n) { m_near = n; m_projDirty = true; }
    void SetFarPlane(float f) { m_far = f; m_projDirty = true; }
    void SetNearFar(float n, float f) { m_near = n; m_far = f; m_projDirty = true; }
    float GetFOV() const { return m_fov; }
    float GetAspectRatio() const { return m_aspect; }
    float GetNearPlane() const { return m_near; }
    float GetFarPlane() const { return m_far; }
    Matrix4x4 GetProjectionMatrix() const override;
private:
    float m_fov, m_aspect, m_near, m_far;
    float Deg2Rad(float d) const;
};

}