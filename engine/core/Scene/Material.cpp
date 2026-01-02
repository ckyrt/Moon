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

// === 纹理贴图设置 ===

void Material::SetAlbedoMap(const std::string& texturePath)
{
    m_albedoMap = texturePath;
}

void Material::SetNormalMap(const std::string& texturePath)
{
    m_normalMap = texturePath;
}

void Material::SetMetallicMap(const std::string& texturePath)
{
    m_metallicMap = texturePath;
}

void Material::SetRoughnessMap(const std::string& texturePath)
{
    m_roughnessMap = texturePath;
}

void Material::SetAOMap(const std::string& texturePath)
{
    m_aoMap = texturePath;
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
    
    // 设置贴图路径 (使用 rock_terrain 作为混凝土/地形)
    SetAlbedoMap("materials/rock_terrain/rocky_terrain_02_diff_4k.jpg");
    SetNormalMap("materials/rock_terrain/rocky_terrain_02_nor_dx_4k.jpg");
}

void Material::SetPresetRubber()
{
    m_metallic = 0.0f;   // 非金属
    m_roughness = 0.9f;  // 很粗糙
    m_baseColor = Vector3(0.2f, 0.2f, 0.2f);  // 深灰色橡胶
    
    // 设置贴图路径
    SetAlbedoMap("materials/rubberized_track/rubberized_track_diff_1k.jpg");
    SetNormalMap("materials/rubberized_track/rubberized_track_nor_dx_1k.jpg");
}

void Material::SetPresetBrick()
{
    m_metallic = 0.0f;   // 非金属
    m_roughness = 0.9f;  // 粗糙表面
    m_baseColor = Vector3(0.6f, 0.3f, 0.2f);  // 红褐色砖头
    
    // 设置贴图路径
    SetAlbedoMap("materials/red_brick/red_brick_diff_1k.jpg");
    SetNormalMap("materials/red_brick/red_brick_nor_dx_1k.jpg");
}

void Material::SetPresetWood()
{
    m_metallic = 0.0f;   // 非金属
    m_roughness = 0.7f;  // 中等粗糙度
    m_baseColor = Vector3(0.6f, 0.4f, 0.2f);  // 木头色
    
    // 设置贴图路径
    SetAlbedoMap("materials/wood_floor/wood_floor_diff_1k.jpg");
    SetNormalMap("materials/wood_floor/wood_floor_nor_dx_1k.jpg");
}

void Material::SetPresetPlaster()
{
    m_metallic = 0.0f;   // 非金属
    m_roughness = 0.85f; // 较粗糙
    m_baseColor = Vector3(0.95f, 0.95f, 0.92f);  // 接近白色的石膏
    
    // 设置贴图路径
    SetAlbedoMap("materials/painted_plaster_wall/painted_plaster_wall_diff_1k.jpg");
    SetNormalMap("materials/painted_plaster_wall/painted_plaster_wall_nor_dx_1k.jpg");
}

void Material::SetPresetIron()
{
    m_metallic = 0.8f;   // 高金属度（略有氧化）
    m_roughness = 0.5f;  // 中等粗糙度（生锈效果）
    m_baseColor = Vector3(0.5f, 0.5f, 0.5f);  // 灰色铁
    
    // 设置贴图路径
    SetAlbedoMap("materials/rusty_metal/rusty_metal_04_diff_1k.jpg");
    SetNormalMap("materials/rusty_metal/rusty_metal_04_nor_dx_1k.jpg");
}

void Material::SetPresetPolishedMetal()
{
    m_metallic = 1.0f;   // 完全金属
    m_roughness = 0.05f; // 非常光滑
    m_baseColor = Vector3(0.95f, 0.95f, 0.95f);  // 银白色抛光金属
}

} // namespace Moon
