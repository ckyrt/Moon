#pragma once
#include "Component.h"
#include "../Camera/Camera.h"
#include <string>

namespace Moon {

// 前向声明
class Texture;

/**
 * @brief 材质预设枚举
 */
enum class MaterialPreset {
    None,           // 无预设
    Concrete,       // 混凝土
    Fabric,         // 布料/橡胶
    Metal,          // 金属
    Plastic,        // 塑料/石膏
    Rock,           // 岩石/砖块
    Wood,           // 木材
    Glass,          // 玻璃 (程序化，无贴图)
    PolishedMetal   // 抛光金属
};

/**
 * @brief Material 组件 - 定义对象的材质属性
 * 
 * 包含 PBR（Physically Based Rendering）材质参数，用于控制物体的渲染外观。
 * 支持金属度/粗糙度工作流（Metallic/Roughness Workflow）。
 */
class Material : public Component {
public:
    /**
     * @brief 构造函数
     * @param owner 拥有此组件的场景节点
     */
    explicit Material(SceneNode* owner);
    
    ~Material() override = default;

    // === PBR 材质参数 ===
    
    /**
     * @brief 设置金属度
     * @param metallic 金属度 [0.0 = 非金属（绝缘体）, 1.0 = 金属]
     */
    void SetMetallic(float metallic);
    
    /**
     * @brief 获取金属度
     */
    float GetMetallic() const { return m_metallic; }
    
    /**
     * @brief 设置粗糙度
     * @param roughness 粗糙度 [0.0 = 完全光滑（镜面）, 1.0 = 完全粗糙（漫反射）]
     */
    void SetRoughness(float roughness);
    
    /**
     * @brief 获取粗糙度
     */
    float GetRoughness() const { return m_roughness; }
    
    /**
     * @brief 设置基础颜色（反照率）
     * @param color RGB 颜色 [0.0 - 1.0]
     */
    void SetBaseColor(const Vector3& color);
    
    /**
     * @brief 获取基础颜色
     */
    const Vector3& GetBaseColor() const { return m_baseColor; }

    // === 纹理贴图（标准 PBR 工作流）===
    
    /**
     * @brief 设置 Albedo（基础颜色）贴图
     * @param texturePath 贴图文件路径
     */
    void SetAlbedoMap(const std::string& texturePath);
    
    /**
     * @brief 设置法线贴图
     * @param texturePath 贴图文件路径
     */
    void SetNormalMap(const std::string& texturePath);
    
    void SetAOMap(const std::string& texturePath);
    void SetRoughnessMap(const std::string& texturePath);
    void SetMetalnessMap(const std::string& texturePath);
    
    const std::string& GetAlbedoMap() const { return m_albedoMap; }
    const std::string& GetNormalMap() const { return m_normalMap; }
    const std::string& GetAOMap() const { return m_aoMap; }
    const std::string& GetRoughnessMap() const { return m_roughnessMap; }
    const std::string& GetMetalnessMap() const { return m_metalnessMap; }
    
    /**
     * @brief 检查是否有 Albedo 贴图
     */
    bool HasAlbedoMap() const { return !m_albedoMap.empty(); }
    
    bool HasNormalMap() const { return !m_normalMap.empty(); }
    bool HasAOMap() const { return !m_aoMap.empty(); }
    bool HasRoughnessMap() const { return !m_roughnessMap.empty(); }
    bool HasMetalnessMap() const { return !m_metalnessMap.empty(); }

    // === 预设材质 ===
    
    /**
     * @brief 通过枚举设置材质预设
     * @param preset 材质预设枚举
     */
    void SetMaterialPreset(MaterialPreset preset);
    
    /**
     * @brief 获取当前材质预设
     */
    MaterialPreset GetMaterialPreset() const { return m_currentPreset; }

private:
    float m_metallic;
    float m_roughness;
    Vector3 m_baseColor;
    
    std::string m_albedoMap;
    std::string m_normalMap;
    std::string m_aoMap;
    std::string m_roughnessMap;
    std::string m_metalnessMap;
    
    // 当前材质预设
    MaterialPreset m_currentPreset;  ///< 当前使用的材质预设
    
    // 材质预设内部实现
    void SetPresetConcrete();
    void SetPresetFabric();
    void SetPresetMetal();
    void SetPresetPlastic();
    void SetPresetRock();
    void SetPresetWood();
};

} // namespace Moon
