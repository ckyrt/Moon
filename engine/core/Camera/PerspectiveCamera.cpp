#include "PerspectiveCamera.h"

namespace Moon {
namespace { constexpr float PI = 3.14159265358979323846f; }

PerspectiveCamera::PerspectiveCamera(float fov, float aspect, float near, float far)
    : m_fov(fov), m_aspect(aspect), m_near(near), m_far(far) {
    m_projDirty = true;
}

Matrix4x4 PerspectiveCamera::GetProjectionMatrix() const {
    if (m_projDirty) {
        m_cachedProj = Matrix4x4::PerspectiveFovLH(Deg2Rad(m_fov), m_aspect, m_near, m_far);
        m_projDirty = false;
    }
    return m_cachedProj;
}

float PerspectiveCamera::Deg2Rad(float d) const { return d * PI / 180.0f; }

}