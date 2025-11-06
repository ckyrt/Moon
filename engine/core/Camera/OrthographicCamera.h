#pragma once
#include "Camera.h"

namespace Moon {

class OrthographicCamera : public Camera {
public:
    OrthographicCamera(float w=10, float h=10, float near=0.1f, float far=1000.0f);
    ~OrthographicCamera() override = default;
    void SetWidth(float w) { m_width = w; m_projDirty = true; }
    void SetHeight(float h) { m_height = h; m_projDirty = true; }
    void SetNearPlane(float n) { m_near = n; m_projDirty = true; }
    void SetFarPlane(float f) { m_far = f; m_projDirty = true; }
    void SetNearFar(float n, float f) { m_near = n; m_far = f; m_projDirty = true; }
    float GetWidth() const { return m_width; }
    float GetHeight() const { return m_height; }
    float GetNearPlane() const { return m_near; }
    float GetFarPlane() const { return m_far; }
    Matrix4x4 GetProjectionMatrix() const override;
private:
    float m_width, m_height, m_near, m_far;
};

}