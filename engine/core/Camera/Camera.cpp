#include "Camera.h"

namespace Moon {

Camera::Camera() : m_position(0,0,-10), m_target(0,0,0), m_up(0,1,0) {}

void Camera::SetPosition(const Vector3& p) { 
    m_position = p; 
    m_viewDirty = true;
}

void Camera::SetTarget(const Vector3& t) { 
    m_target = t; 
    m_viewDirty = true;
}

void Camera::SetUp(const Vector3& u) { 
    m_up = u; 
    m_viewDirty = true;
}

Vector3 Camera::GetForward() const { 
    return (m_target - m_position).Normalized(); 
}

Vector3 Camera::GetRight() const { 
    return Vector3::Cross(m_up, GetForward()).Normalized(); 
}

Matrix4x4 Camera::GetViewMatrix() const { 
    if (m_viewDirty) {
        m_cachedView = Matrix4x4::LookAtLH(m_position, m_target, m_up);
        m_viewDirty = false;
    }
    return m_cachedView;
}

Matrix4x4 Camera::GetViewProjectionMatrix() const {
    return GetViewMatrix() * GetProjectionMatrix();
}

void Camera::LookAt(const Vector3& target) {
    SetTarget(target);
}

}