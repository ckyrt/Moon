#include "Material.h"
#include "SceneNode.h"
#include <algorithm>

namespace Moon {

Material::Material(SceneNode* owner)
    : Component(owner)
    , m_metallic(0.0f)
    , m_roughness(0.5f)
    , m_baseColor(1.0f, 1.0f, 1.0f)
    , m_opacity(1.0f)
    , m_transmissionColor(1.0f, 1.0f, 1.0f)
    , m_currentPreset(MaterialPreset::None)
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

void Material::SetOpacity(float opacity)
{
    // 限制范围 [0.0 - 1.0]
    m_opacity = std::max(0.0f, std::min(1.0f, opacity));
}

void Material::SetTransmissionColor(const Vector3& color)
{
    m_transmissionColor = color;
    
    // 限制颜色分量范围 [0.0 - 1.0]
    m_transmissionColor.x = std::max(0.0f, std::min(1.0f, m_transmissionColor.x));
    m_transmissionColor.y = std::max(0.0f, std::min(1.0f, m_transmissionColor.y));
    m_transmissionColor.z = std::max(0.0f, std::min(1.0f, m_transmissionColor.z));
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

void Material::SetAOMap(const std::string& texturePath)
{
    m_aoMap = texturePath;
}

void Material::SetRoughnessMap(const std::string& texturePath)
{
    m_roughnessMap = texturePath;
}

void Material::SetMetalnessMap(const std::string& texturePath)
{
    m_metalnessMap = texturePath;
}

// === 预设材质 ===

void Material::SetMaterialPreset(MaterialPreset preset)
{
    m_currentPreset = preset;
    
    switch (preset) {
        case MaterialPreset::None:
            SetAlbedoMap("");
            SetNormalMap("");
            SetAOMap("");
            SetRoughnessMap("");
            SetMetalnessMap("");
            break;
            
        // === 混凝土系列 ===
        case MaterialPreset::Concrete:          SetPresetConcrete(); break;
        case MaterialPreset::ConcreteFloor:     SetPresetConcreteFloor(); break;
        case MaterialPreset::ConcretePolished:  SetPresetConcretePolished(); break;
        
        // === 岩石/砖石系列 ===
        case MaterialPreset::Rock:              SetPresetRock(); break;
        case MaterialPreset::Brick:             SetPresetBrick(); break;
        case MaterialPreset::Stone:             SetPresetStone(); break;
        case MaterialPreset::Plaster:           SetPresetPlaster(); break;
        case MaterialPreset::TileCeramic:       SetPresetTileCeramic(); break;
        
        // === 木材系列 ===
        case MaterialPreset::Wood:              SetPresetWood(); break;
        case MaterialPreset::WoodFloor:         SetPresetWoodFloor(); break;
        case MaterialPreset::WoodPolished:      SetPresetWoodPolished(); break;
        case MaterialPreset::WoodPainted:       SetPresetWoodPainted(); break;
        
        // === 金属系列 ===
        case MaterialPreset::Metal:             SetPresetMetal(); break;
        case MaterialPreset::Steel:             SetPresetSteel(); break;
        case MaterialPreset::Aluminum:          SetPresetAluminum(); break;
        case MaterialPreset::Chrome:            SetPresetChrome(); break;
        case MaterialPreset::Copper:            SetPresetCopper(); break;
        
        // === 玻璃系列 ===
        case MaterialPreset::Glass:             SetPresetGlass(); break;
        case MaterialPreset::GlassFrosted:      SetPresetGlassFrosted(); break;
        case MaterialPreset::GlassTinted:       SetPresetGlassTinted(); break;
        
        // === 软装系列 ===
        case MaterialPreset::Fabric:            SetPresetFabric(); break;
        case MaterialPreset::Leather:           SetPresetLeather(); break;
        case MaterialPreset::Carpet:            SetPresetCarpet(); break;
        
        // === 塑料/橡胶系列 ===
        case MaterialPreset::Plastic:           SetPresetPlastic(); break;
        case MaterialPreset::Rubber:            SetPresetRubber(); break;
    }
}

void Material::SetPresetConcrete()
{
    m_currentPreset = MaterialPreset::Concrete;
    m_metallic = 1.0f;  // 有 Metalness 贴图，让贴图控制
    m_roughness = 1.0f;  // 有 Roughness 贴图，让贴图控制
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;  // 不透明
    
    SetAlbedoMap("materials/Concrete044D_2K-PNG/Concrete044D_2K-PNG_Color.png");
    SetNormalMap("materials/Concrete044D_2K-PNG/Concrete044D_2K-PNG_NormalDX.png");
    SetAOMap("materials/Concrete044D_2K-PNG/Concrete044D_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/Concrete044D_2K-PNG/Concrete044D_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/Concrete044D_2K-PNG/Concrete044D_2K-PNG_Metalness.png");
}

void Material::SetPresetFabric()
{
    m_currentPreset = MaterialPreset::Fabric;
    m_metallic = 0.0f;  // 非金属，无 Metalness 贴图
    m_roughness = 1.0f;  // 有 Roughness 贴图，让贴图控制
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;  // 不透明
    
    SetAlbedoMap("materials/Fabric061_2K-PNG/Fabric061_2K-PNG_Color.png");
    SetNormalMap("materials/Fabric061_2K-PNG/Fabric061_2K-PNG_NormalDX.png");
    SetAOMap("materials/Fabric061_2K-PNG/Fabric061_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/Fabric061_2K-PNG/Fabric061_2K-PNG_Roughness.png");
    SetMetalnessMap("");
}

void Material::SetPresetMetal()
{
    m_currentPreset = MaterialPreset::Metal;
    m_metallic = 1.0f;
    m_roughness = 1.0f;
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;  // 不透明
    
    SetAlbedoMap("materials/Metal061B_2K-PNG/Metal061B_2K-PNG_Color.png");
    SetNormalMap("materials/Metal061B_2K-PNG/Metal061B_2K-PNG_NormalDX.png");
    SetAOMap("");
    SetRoughnessMap("materials/Metal061B_2K-PNG/Metal061B_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/Metal061B_2K-PNG/Metal061B_2K-PNG_Metalness.png");
}

void Material::SetPresetPlastic()
{
    m_currentPreset = MaterialPreset::Plastic;
    m_metallic = 0.0f;  // 非金属，无 Metalness 贴图
    m_roughness = 1.0f;  // 有 Roughness 贴图，让贴图控制
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;  // 不透明
    
    SetAlbedoMap("materials/Plastic018A_2K-PNG/Plastic018A_2K-PNG_Color.png");
    SetNormalMap("materials/Plastic018A_2K-PNG/Plastic018A_2K-PNG_NormalDX.png");
    SetAOMap("");
    SetRoughnessMap("materials/Plastic018A_2K-PNG/Plastic018A_2K-PNG_Roughness.png");
    SetMetalnessMap("");
}

void Material::SetPresetRock()
{
    m_currentPreset = MaterialPreset::Rock;
    m_metallic = 0.0f;  // 非金属，无 Metalness 贴图
    m_roughness = 1.0f;  // 有 Roughness 贴图，让贴图控制
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;  // 不透明
    
    SetAlbedoMap("materials/Rock030_2K-PNG/Rock030_2K-PNG_Color.png");
    SetNormalMap("materials/Rock030_2K-PNG/Rock030_2K-PNG_NormalDX.png");
    SetAOMap("materials/Rock030_2K-PNG/Rock030_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/Rock030_2K-PNG/Rock030_2K-PNG_Roughness.png");
    SetMetalnessMap("");
}

void Material::SetPresetWood()
{
    m_currentPreset = MaterialPreset::Wood;
    m_metallic = 0.0f;  // 非金属，无 Metalness 贴图
    m_roughness = 1.0f;  // 有 Roughness 贴图，让贴图控制
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;  // 不透明
    
    SetAlbedoMap("materials/Wood049_2K-PNG/Wood049_2K-PNG_Color.png");
    SetNormalMap("materials/Wood049_2K-PNG/Wood049_2K-PNG_NormalDX.png");
    SetAOMap("");
    SetRoughnessMap("materials/Wood049_2K-PNG/Wood049_2K-PNG_Roughness.png");
    SetMetalnessMap("");
}

// ============================================================================
// 混凝土系列（3种）
// ============================================================================

void Material::SetPresetConcreteFloor()
{
    m_currentPreset = MaterialPreset::ConcreteFloor;
    m_metallic = 0.0f;
    m_roughness = 0.85f;  // 比原始混凝土稍光滑
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/ConcreteFloor_2K-PNG/ConcreteFloor_2K-PNG_Color.png");
    SetNormalMap("materials/ConcreteFloor_2K-PNG/ConcreteFloor_2K-PNG_NormalDX.png");
    SetAOMap("materials/ConcreteFloor_2K-PNG/ConcreteFloor_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/ConcreteFloor_2K-PNG/ConcreteFloor_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/ConcreteFloor_2K-PNG/ConcreteFloor_2K-PNG_Metalness.png");
}

void Material::SetPresetConcretePolished()
{
    m_currentPreset = MaterialPreset::ConcretePolished;
    m_metallic = 0.0f;
    m_roughness = 0.3f;  // 抛光后非常光滑
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/ConcretePolished_2K-PNG/ConcretePolished_2K-PNG_Color.png");
    SetNormalMap("materials/ConcretePolished_2K-PNG/ConcretePolished_2K-PNG_NormalDX.png");
    SetAOMap("materials/ConcretePolished_2K-PNG/ConcretePolished_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/ConcretePolished_2K-PNG/ConcretePolished_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/ConcretePolished_2K-PNG/ConcretePolished_2K-PNG_Metalness.png");
}

// ============================================================================
// 岩石/砖石系列（4种，Rock已有）
// ============================================================================

void Material::SetPresetBrick()
{
    m_currentPreset = MaterialPreset::Brick;
    m_metallic = 1.0f;  // 有 Metalness 贴图，让贴图控制
    m_roughness = 1.0f;  // 有 Roughness 贴图，让贴图控制
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/Brick_2K-PNG/Brick_2K-PNG_Color.png");
    SetNormalMap("materials/Brick_2K-PNG/Brick_2K-PNG_NormalDX.png");
    SetAOMap("materials/Brick_2K-PNG/Brick_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/Brick_2K-PNG/Brick_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/Brick_2K-PNG/Brick_2K-PNG_Metalness.png");
}

void Material::SetPresetStone()
{
    m_currentPreset = MaterialPreset::Stone;
    m_metallic = 0.0f;
    m_roughness = 0.8f;
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/Stone_2K-PNG/Stone_2K-PNG_Color.png");
    SetNormalMap("materials/Stone_2K-PNG/Stone_2K-PNG_NormalDX.png");
    SetAOMap("materials/Stone_2K-PNG/Stone_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/Stone_2K-PNG/Stone_2K-PNG_Roughness.png");
    SetMetalnessMap("");
}

void Material::SetPresetPlaster()
{
    m_currentPreset = MaterialPreset::Plaster;
    m_metallic = 1.0f;  // 有 Metalness 贴图，让贴图控制
    m_roughness = 1.0f;  // 有 Roughness 贴图，让贴图控制
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/Plaster_2K-PNG/Plaster_2K-PNG_Color.png");
    SetNormalMap("materials/Plaster_2K-PNG/Plaster_2K-PNG_NormalDX.png");
    SetAOMap("materials/Plaster_2K-PNG/Plaster_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/Plaster_2K-PNG/Plaster_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/Plaster_2K-PNG/Plaster_2K-PNG_Metalness.png");
}

void Material::SetPresetTileCeramic()
{
    m_currentPreset = MaterialPreset::TileCeramic;
    m_metallic = 1.0f;  // 有 Metalness 贴图，让贴图控制
    m_roughness = 1.0f;  // 有 Roughness 贴图，让贴图控制
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/TileCeramic_2K-PNG/TileCeramic_2K-PNG_Color.png");
    SetNormalMap("materials/TileCeramic_2K-PNG/TileCeramic_2K-PNG_NormalDX.png");
    SetAOMap("materials/TileCeramic_2K-PNG/TileCeramic_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/TileCeramic_2K-PNG/TileCeramic_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/TileCeramic_2K-PNG/TileCeramic_2K-PNG_Metalness.png");
}

// ============================================================================
// 木材系列（3种，Wood已有）
// ============================================================================

void Material::SetPresetWoodFloor()
{
    m_currentPreset = MaterialPreset::WoodFloor;
    m_metallic = 1.0f;  // 有 Metalness 贴图，让贴图控制
    m_roughness = 1.0f;  // 有 Roughness 贴图，让贴图控制
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/WoodFloor_2K-PNG/WoodFloor_2K-PNG_Color.png");
    SetNormalMap("materials/WoodFloor_2K-PNG/WoodFloor_2K-PNG_NormalDX.png");
    SetAOMap("materials/WoodFloor_2K-PNG/WoodFloor_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/WoodFloor_2K-PNG/WoodFloor_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/WoodFloor_2K-PNG/WoodFloor_2K-PNG_Metalness.png");
}

void Material::SetPresetWoodPolished()
{
    m_currentPreset = MaterialPreset::WoodPolished;
    m_metallic = 0.0f;
    m_roughness = 0.3f;  // 抛光木材
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/WoodPolished_2K-PNG/WoodPolished_2K-PNG_Color.png");
    SetNormalMap("materials/WoodPolished_2K-PNG/WoodPolished_2K-PNG_NormalDX.png");
    SetAOMap("materials/WoodPolished_2K-PNG/WoodPolished_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/WoodPolished_2K-PNG/WoodPolished_2K-PNG_Roughness.png");
    SetMetalnessMap("");
}

void Material::SetPresetWoodPainted()
{
    m_currentPreset = MaterialPreset::WoodPainted;
    m_metallic = 0.0f;
    m_roughness = 0.4f;
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/WoodPainted_2K-PNG/WoodPainted_2K-PNG_Color.png");
    SetNormalMap("materials/WoodPainted_2K-PNG/WoodPainted_2K-PNG_NormalDX.png");
    SetAOMap("materials/WoodPainted_2K-PNG/WoodPainted_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/WoodPainted_2K-PNG/WoodPainted_2K-PNG_Roughness.png");
    SetMetalnessMap("");
}

// ============================================================================
// 金属系列（4种，Metal已有）
// ============================================================================

void Material::SetPresetSteel()
{
    m_currentPreset = MaterialPreset::Steel;
    m_metallic = 1.0f;
    m_roughness = 0.3f;  // 不锈钢比较光滑
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/Steel_2K-PNG/Steel_2K-PNG_Color.png");
    SetNormalMap("materials/Steel_2K-PNG/Steel_2K-PNG_NormalDX.png");
    SetAOMap("");
    SetRoughnessMap("materials/Steel_2K-PNG/Steel_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/Steel_2K-PNG/Steel_2K-PNG_Metalness.png");
}

void Material::SetPresetAluminum()
{
    m_currentPreset = MaterialPreset::Aluminum;
    m_metallic = 1.0f;
    m_roughness = 0.4f;
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/Aluminum_2K-PNG/Aluminum_2K-PNG_Color.png");
    SetNormalMap("materials/Aluminum_2K-PNG/Aluminum_2K-PNG_NormalDX.png");
    SetAOMap("");
    SetRoughnessMap("materials/Aluminum_2K-PNG/Aluminum_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/Aluminum_2K-PNG/Aluminum_2K-PNG_Metalness.png");
}

void Material::SetPresetChrome()
{
    m_currentPreset = MaterialPreset::Chrome;
    m_metallic = 1.0f;
    m_roughness = 0.1f;  // 镀铬非常光滑
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/Chrome_2K-PNG/Chrome_2K-PNG_Color.png");
    SetNormalMap("materials/Chrome_2K-PNG/Chrome_2K-PNG_NormalDX.png");
    SetAOMap("");
    SetRoughnessMap("materials/Chrome_2K-PNG/Chrome_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/Chrome_2K-PNG/Chrome_2K-PNG_Metalness.png");
}

void Material::SetPresetCopper()
{
    m_currentPreset = MaterialPreset::Copper;
    m_metallic = 1.0f;
    m_roughness = 0.5f;
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/Copper_2K-PNG/Copper_2K-PNG_Color.png");
    SetNormalMap("materials/Copper_2K-PNG/Copper_2K-PNG_NormalDX.png");
    SetAOMap("");
    SetRoughnessMap("materials/Copper_2K-PNG/Copper_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/Copper_2K-PNG/Copper_2K-PNG_Metalness.png");
}

// ============================================================================
// 玻璃系列（3种）- 程序化，无贴图
// ============================================================================

void Material::SetPresetGlass()
{
    m_currentPreset = MaterialPreset::Glass;
    m_metallic = 0.0f;
    m_roughness = 0.05f;  // 透明玻璃非常光滑
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 0.4f;  // 60%透明
    m_transmissionColor = Vector3(1.0f, 1.0f, 1.0f);
    
    SetAlbedoMap("");
    SetNormalMap("");
    SetAOMap("");
    SetRoughnessMap("");
    SetMetalnessMap("");
}

void Material::SetPresetGlassFrosted()
{
    m_currentPreset = MaterialPreset::GlassFrosted;
    m_metallic = 0.0f;
    m_roughness = 0.3f;  // 磨砂玻璃粗糙
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 0.6f;  // 40%透明
    m_transmissionColor = Vector3(1.0f, 1.0f, 1.0f);
    
    SetAlbedoMap("");
    SetNormalMap("");
    SetAOMap("");
    SetRoughnessMap("");
    SetMetalnessMap("");
}

void Material::SetPresetGlassTinted()
{
    m_currentPreset = MaterialPreset::GlassTinted;
    m_metallic = 0.0f;
    m_roughness = 0.05f;
    m_baseColor = Vector3(0.7f, 0.7f, 0.7f);  // 有色玻璃（灰色调）
    m_opacity = 0.5f;  // 50%透明
    m_transmissionColor = Vector3(0.8f, 0.8f, 0.9f);  // 蓝灰色
    
    SetAlbedoMap("");
    SetNormalMap("");
    SetAOMap("");
    SetRoughnessMap("");
    SetMetalnessMap("");
}

// ============================================================================
// 软装系列（2种，Fabric已有）
// ============================================================================

void Material::SetPresetLeather()
{
    m_currentPreset = MaterialPreset::Leather;
    m_metallic = 1.0f;  // 有 Metalness 贴图，让贴图控制
    m_roughness = 1.0f;  // 有 Roughness 贴图，让贴图控制
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/Leather_2K-PNG/Leather_2K-PNG_Color.png");
    SetNormalMap("materials/Leather_2K-PNG/Leather_2K-PNG_NormalDX.png");
    SetAOMap("materials/Leather_2K-PNG/Leather_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/Leather_2K-PNG/Leather_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/Leather_2K-PNG/Leather_2K-PNG_Metalness.png");
}

void Material::SetPresetCarpet()
{
    m_currentPreset = MaterialPreset::Carpet;
    m_metallic = 1.0f;  // 有 Metalness 贴图，让贴图控制
    m_roughness = 1.0f;  // 有 Roughness 贴图，让贴图控制
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);  // 白色让贴图完全控制颜色
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/Carpet_2K-PNG/Carpet_2K-PNG_Color.png");
    SetNormalMap("materials/Carpet_2K-PNG/Carpet_2K-PNG_NormalDX.png");
    SetAOMap("materials/Carpet_2K-PNG/Carpet_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/Carpet_2K-PNG/Carpet_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/Carpet_2K-PNG/Carpet_2K-PNG_Metalness.png");
}

// ============================================================================
// 塑料/橡胶系列（1种，Plastic已有）
// ============================================================================

void Material::SetPresetRubber()
{
    m_currentPreset = MaterialPreset::Rubber;
    m_metallic = 1.0f;  // 有 Metalness 贴图，让贴图控制
    m_roughness = 1.0f;  // 有 Roughness 贴图，让贴图控制
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;
    
    SetAlbedoMap("materials/Rubber_2K-PNG/Rubber_2K-PNG_Color.png");
    SetNormalMap("materials/Rubber_2K-PNG/Rubber_2K-PNG_NormalDX.png");
    SetAOMap("materials/Rubber_2K-PNG/Rubber_2K-PNG_AmbientOcclusion.png");
    SetRoughnessMap("materials/Rubber_2K-PNG/Rubber_2K-PNG_Roughness.png");
    SetMetalnessMap("materials/Rubber_2K-PNG/Rubber_2K-PNG_Metalness.png");
}

} // namespace Moon
