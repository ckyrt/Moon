#include "Material.h"
#include "SceneNode.h"
#include <algorithm>

namespace Moon {

Material::Material(SceneNode* owner)
    : Component(owner)
    , m_metallic(0.0f)
    , m_roughness(0.5f)
    , m_baseColor(1.0f, 1.0f, 1.0f)
{
}

void Material::SetMetallic(float metallic)
{
    // 限制范围 [0.0 - 1.0]
    m_metallic = std::max(0.0f, std::min(1.0f, metallic));
}

void Material::SetRoughness(float roughness)
{
    // 限制范围 [0.0 - 1.0]
    m_roughness = std::max(0.0f, std::min(1.0f, roughness));
}

void Material::SetBaseColor(const Vector3& color)
{
    m_baseColor = color;
    
    // 限制颜色分量范围 [0.0 - 1.0]
    m_baseColor.x = std::max(0.0f, std::min(1.0f, m_baseColor.x));
    m_baseColor.y = std::max(0.0f, std::min(1.0f, m_baseColor.y));
    m_baseColor.z = std::max(0.0f, std::min(1.0f, m_baseColor.z));
}

// === 预设材质 ===

void Material::SetPresetMetal(float roughness)
{
    m_metallic = 1.0f;  // 完全金属
    SetRoughness(roughness);
    m_baseColor = Vector3(0.9f, 0.9f, 0.9f);  // 银白色金属
}

void Material::SetPresetPlastic(float roughness)
{
    m_metallic = 0.0f;  // 非金属
    SetRoughness(roughness);
    m_baseColor = Vector3(0.8f, 0.8f, 0.8f);  // 浅灰色塑料
}

void Material::SetPresetConcrete()
{
    m_metallic = 0.0f;   // 非金属
    m_roughness = 0.95f; // 非常粗糙
    m_baseColor = Vector3(0.5f, 0.5f, 0.5f);  // 灰色混凝土
}

void Material::SetPresetRubber()
{
    m_metallic = 0.0f;   // 非金属
    m_roughness = 0.9f;  // 很粗糙
    m_baseColor = Vector3(0.2f, 0.2f, 0.2f);  // 深灰色橡胶
}

} // namespace Moon
