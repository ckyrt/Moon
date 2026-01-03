#include "Skybox.h"
#include "SceneNode.h"
#include "../Logging/Logger.h"
#include <algorithm>

namespace Moon {

Skybox::Skybox(SceneNode* owner)
    : Component(owner)
    , m_type(Type::None)
    , m_intensity(1.0f)
    , m_rotation(0.0f)
    , m_tint(1.0f, 1.0f, 1.0f)
    , m_enableIBL(true)
    , m_needsReload(false)
{
}

bool Skybox::LoadEnvironmentMap(const std::string& filepath)
{
    if (filepath.empty()) {
        MOON_LOG_WARN("Skybox", "Empty filepath provided");
        return false;
    }
    
    m_environmentMapPath = filepath;
    m_type = Type::EquirectangularHDR;
    m_needsReload = true;  // 标记需要重新加载
    
    MOON_LOG_INFO("Skybox", "Environment map queued for loading: {}", filepath);
    return true;
}

void Skybox::SetIntensity(float intensity)
{
    // 限制强度为正数
    m_intensity = std::max(0.0f, intensity);
}

void Skybox::SetTint(const Vector3& tint)
{
    m_tint = tint;
    
    // 限制色调范围 [0.0 - 1.0]
    m_tint.x = std::max(0.0f, std::min(1.0f, m_tint.x));
    m_tint.y = std::max(0.0f, std::min(1.0f, m_tint.y));
    m_tint.z = std::max(0.0f, std::min(1.0f, m_tint.z));
}

} // namespace Moon
