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

void Material::SetARMMap(const std::string& texturePath)
{
    m_armMap = texturePath;
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
    // Unity 标准做法：这些值作为贴图的乘数/调色器
    m_metallic = 1.0f;   // 乘数 = 1.0 表示完全使用贴图值
    m_roughness = 1.0f;  // 乘数 = 1.0 表示完全使用贴图值
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);  // 白色 = 不改变贴图颜色
    
    // 设置贴图路径 (使用 rock_terrain 作为混凝土/地形)
    SetAlbedoMap("materials/rock_terrain/rocky_terrain_02_diff_4k.jpg");
    SetNormalMap("materials/rock_terrain/rocky_terrain_02_nor_dx_4k.jpg");
    SetARMMap("materials/rock_terrain/rocky_terrain_02_arm_4k.jpg");  // ARM 贴图
}

void Material::SetPresetRubber()
{
    m_metallic = 1.0f;   
    m_roughness = 1.0f;  
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    
    // 设置贴图路径
    SetAlbedoMap("materials/rubberized_track/rubberized_track_diff_1k.jpg");
    SetNormalMap("materials/rubberized_track/rubberized_track_nor_dx_1k.jpg");
    SetARMMap("materials/rubberized_track/rubberized_track_arm_1k.jpg");  // ARM 贴图
}

void Material::SetPresetBrick()
{
    m_metallic = 1.0f;   
    m_roughness = 1.0f;  
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    
    // 设置贴图路径
    SetAlbedoMap("materials/red_brick/red_brick_diff_1k.jpg");
    SetNormalMap("materials/red_brick/red_brick_nor_dx_1k.jpg");
    SetARMMap("materials/red_brick/red_brick_arm_1k.jpg");  // ARM 贴图
}

void Material::SetPresetWood()
{
    // 木头是非金属材质
    m_metallic = 0.0f;   // 贴图存在时会被贴图覆盖，这是fallback值
    m_roughness = 1.0f;  // 作为乘数，1.0表示不修改贴图值
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);  // 白色，不调整贴图颜色
    
    // 设置贴图路径
    SetAlbedoMap("materials/wood_floor/wood_floor_diff_1k.jpg");
    SetNormalMap("materials/wood_floor/wood_floor_nor_dx_1k.jpg");
    SetARMMap("materials/wood_floor/wood_floor_arm_1k.jpg");  // ARM 贴图
}

void Material::SetPresetPlaster()
{
    m_metallic = 1.0f;   
    m_roughness = 1.0f;  
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    
    // 设置贴图路径
    SetAlbedoMap("materials/painted_plaster_wall/painted_plaster_wall_diff_1k.jpg");
    SetNormalMap("materials/painted_plaster_wall/painted_plaster_wall_nor_dx_1k.jpg");
    SetARMMap("materials/painted_plaster_wall/painted_plaster_wall_arm_1k.jpg");  // ARM 贴图
}

void Material::SetPresetIron()
{
    m_metallic = 1.0f;   
    m_roughness = 1.0f;  
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    
    // 设置贴图路径
    SetAlbedoMap("materials/rusty_metal/rusty_metal_04_diff_1k.jpg");
    SetNormalMap("materials/rusty_metal/rusty_metal_04_nor_dx_1k.jpg");
    SetARMMap("materials/rusty_metal/rusty_metal_04_arm_1k.jpg");  // ARM 贴图
}

void Material::SetPresetPolishedMetal()
{
    m_metallic = 1.0f;   // 完全金属
    m_roughness = 0.05f; // 非常光滑
    m_baseColor = Vector3(0.95f, 0.95f, 0.95f);  // 银白色抛光金属
}

} // namespace Moon
