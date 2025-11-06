#include "OrthographicCamera.h"

namespace Moon {

OrthographicCamera::OrthographicCamera(float w, float h, float near, float far)
    : m_width(w), m_height(h), m_near(near), m_far(far) {
    m_projDirty = true;
}

Matrix4x4 OrthographicCamera::GetProjectionMatrix() const {
    if (m_projDirty) {
        m_cachedProj = Matrix4x4::OrthoLH(m_width, m_height, m_near, m_far);
        m_projDirty = false;
    }
    return m_cachedProj;
}

}