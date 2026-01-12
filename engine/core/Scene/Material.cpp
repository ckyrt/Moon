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
            break;
        case MaterialPreset::Concrete:
            SetPresetConcrete();
            break;
        case MaterialPreset::Fabric:
            SetPresetFabric();
            break;
        case MaterialPreset::Metal:
            SetPresetMetal();
            break;
        case MaterialPreset::Plastic:
            SetPresetPlastic();
            break;
        case MaterialPreset::Rock:
            SetPresetRock();
            break;
        case MaterialPreset::Wood:
            SetPresetWood();
            break;
        case MaterialPreset::Glass:
            SetPresetGlass();
            break;
        case MaterialPreset::PolishedMetal:
            // 暂不支持
            break;
    }
}

void Material::SetPresetConcrete()
{
    m_currentPreset = MaterialPreset::Concrete;
    m_metallic = 0.0f;
    m_roughness = 0.9f;
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
    m_metallic = 0.0f;
    m_roughness = 0.9f;
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
    m_metallic = 0.0f;
    m_roughness = 0.5f;
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
    m_metallic = 0.0f;
    m_roughness = 0.9f;
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
    m_metallic = 0.0f;
    m_roughness = 0.8f;
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 1.0f;  // 不透明
    
    SetAlbedoMap("materials/Wood049_2K-PNG/Wood049_2K-PNG_Color.png");
    SetNormalMap("materials/Wood049_2K-PNG/Wood049_2K-PNG_NormalDX.png");
    SetAOMap("");
    SetRoughnessMap("materials/Wood049_2K-PNG/Wood049_2K-PNG_Roughness.png");
    SetMetalnessMap("");
}

void Material::SetPresetGlass()
{
    m_currentPreset = MaterialPreset::Glass;
    m_metallic = 0.0f;           // 非金属
    m_roughness = 0.05f;         // 非常光滑（镜面反射）
    m_baseColor = Vector3(1.0f, 1.0f, 1.0f);
    m_opacity = 0.15f;           // 85%透明（业界标准范围：0.1-0.3）
    m_transmissionColor = Vector3(1.0f, 1.0f, 1.0f);  // 纯白色（标准玻璃）
    
    // 玻璃不使用纹理贴图，完全程序化
    SetAlbedoMap("");
    SetNormalMap("");
    SetAOMap("");
    SetRoughnessMap("");
    SetMetalnessMap("");
}

} // namespace Moon
