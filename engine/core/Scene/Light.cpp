#include "Light.h"
#include "SceneNode.h"
#include "Transform.h"
#include <algorithm>
#include <cmath>

namespace Moon {

Light::Light(SceneNode* owner)
    : Component(owner)
    , m_type(Type::Directional)
    , m_color(1.0f, 1.0f, 1.0f)  // 白色
    , m_intensity(1.0f)
    , m_range(10.0f)
    , m_attenuationConstant(1.0f)
    , m_attenuationLinear(0.09f)
    , m_attenuationQuadratic(0.032f)
    , m_spotInnerConeAngle(12.5f)
    , m_spotOuterConeAngle(17.5f)
    , m_castShadows(false)
{
}

void Light::SetColor(const Vector3& color)
{
    m_color = color;
    
    // 限制颜色分量范围 [0.0 - 1.0]
    m_color.x = std::max(0.0f, std::min(1.0f, m_color.x));
    m_color.y = std::max(0.0f, std::min(1.0f, m_color.y));
    m_color.z = std::max(0.0f, std::min(1.0f, m_color.z));
}

void Light::SetIntensity(float intensity)
{
    // 强度必须为正数
    m_intensity = std::max(0.0f, intensity);
}

Vector3 Light::GetDirection() const
{
    if (!m_owner) {
        return Vector3(0.0f, -1.0f, 0.0f);  // 默认向下
    }
    
    // 获取 Transform 的世界矩阵
    const Matrix4x4& worldMatrix = m_owner->GetTransform()->GetWorldMatrix();
    
    // 提取 forward 向量（Z 轴方向）
    // 在左手坐标系中，forward = (m[2][0], m[2][1], m[2][2])
    Vector3 forward(worldMatrix.m[2][0], worldMatrix.m[2][1], worldMatrix.m[2][2]);
    
    // 归一化
    float length = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (length > 0.0001f) {
        forward.x /= length;
        forward.y /= length;
        forward.z /= length;
    }
    
    return forward;
}

void Light::SetRange(float range)
{
    m_range = std::max(0.1f, range);  // 最小范围 0.1
}

void Light::SetAttenuation(float constant, float linear, float quadratic)
{
    m_attenuationConstant = std::max(0.0f, constant);
    m_attenuationLinear = std::max(0.0f, linear);
    m_attenuationQuadratic = std::max(0.0f, quadratic);
}

void Light::GetAttenuation(float& constant, float& linear, float& quadratic) const
{
    constant = m_attenuationConstant;
    linear = m_attenuationLinear;
    quadratic = m_attenuationQuadratic;
}

void Light::SetSpotAngles(float innerConeAngle, float outerConeAngle)
{
    // 限制角度范围 [0 - 90]
    m_spotInnerConeAngle = std::max(0.0f, std::min(90.0f, innerConeAngle));
    m_spotOuterConeAngle = std::max(0.0f, std::min(90.0f, outerConeAngle));
    
    // 确保外锥角 >= 内锥角
    if (m_spotOuterConeAngle < m_spotInnerConeAngle) {
        m_spotOuterConeAngle = m_spotInnerConeAngle;
    }
}

void Light::GetSpotAngles(float& innerConeAngle, float& outerConeAngle) const
{
    innerConeAngle = m_spotInnerConeAngle;
    outerConeAngle = m_spotOuterConeAngle;
}

} // namespace Moon
