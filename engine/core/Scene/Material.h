#pragma once
#include "Component.h"
#include "../Camera/Camera.h"
#include <string>

namespace Moon {

// 前向声明
class Texture;

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
    
    /**
     * @brief 设置金属度贴图
     * @param texturePath 贴图文件路径
     */
    void SetMetallicMap(const std::string& texturePath);
    
    /**
     * @brief 设置粗糙度贴图
     * @param texturePath 贴图文件路径
     */
    void SetRoughnessMap(const std::string& texturePath);
    
    /**
     * @brief 设置环境光遮蔽（AO）贴图
     * @param texturePath 贴图文件路径
     */
    void SetAOMap(const std::string& texturePath);
    
    /**
     * @brief 获取 Albedo 贴图路径
     */
    const std::string& GetAlbedoMap() const { return m_albedoMap; }
    
    /**
     * @brief 获取法线贴图路径
     */
    const std::string& GetNormalMap() const { return m_normalMap; }
    
    /**
     * @brief 获取金属度贴图路径
     */
    const std::string& GetMetallicMap() const { return m_metallicMap; }
    
    /**
     * @brief 获取粗糙度贴图路径
     */
    const std::string& GetRoughnessMap() const { return m_roughnessMap; }
    
    /**
     * @brief 获取 AO 贴图路径
     */
    const std::string& GetAOMap() const { return m_aoMap; }
    
    /**
     * @brief 检查是否有 Albedo 贴图
     */
    bool HasAlbedoMap() const { return !m_albedoMap.empty(); }
    
    /**
     * @brief 检查是否有法线贴图
     */
    bool HasNormalMap() const { return !m_normalMap.empty(); }
    
    /**
     * @brief 检查是否有金属度贴图
     */
    bool HasMetallicMap() const { return !m_metallicMap.empty(); }
    
    /**
     * @brief 检查是否有粗糙度贴图
     */
    bool HasRoughnessMap() const { return !m_roughnessMap.empty(); }
    
    /**
     * @brief 检查是否有 AO 贴图
     */
    bool HasAOMap() const { return !m_aoMap.empty(); }

    // === 预设材质 ===
    
    /**
     * @brief 设置为金属材质预设
     * @param roughness 粗糙度（默认 0.2 = 抛光金属）
     */
    void SetPresetMetal(float roughness = 0.2f);
    
    /**
     * @brief 设置为塑料材质预设
     * @param roughness 粗糙度（默认 0.5 = 哑光塑料）
     */
    void SetPresetPlastic(float roughness = 0.5f);
    
    /**
     * @brief 设置为混凝土材质预设
     */
    void SetPresetConcrete();
    
    /**
     * @brief 设置为橡胶材质预设
     */
    void SetPresetRubber();
    
    /**
     * @brief 设置为砖头材质预设
     */
    void SetPresetBrick();
    
    /**
     * @brief 设置为木头材质预设
     */
    void SetPresetWood();
    
    /**
     * @brief 设置为石膏材质预设
     */
    void SetPresetPlaster();
    
    /**
     * @brief 设置为铁球材质预设（生锈的铁）
     */
    void SetPresetIron();
    
    /**
     * @brief 设置为抛光金属材质预设（如铬合金）
     */
    void SetPresetPolishedMetal();

private:
    float m_metallic;      ///< 金属度 [0.0 - 1.0]
    float m_roughness;     ///< 粗糙度 [0.0 - 1.0]
    Vector3 m_baseColor;   ///< 基础颜色（反照率）
    
    // 纹理贴图路径（标准 PBR 工作流）
    std::string m_albedoMap;    ///< Albedo/Base Color 贴图路径
    std::string m_normalMap;    ///< 法线贴图路径
    std::string m_metallicMap;  ///< 金属度贴图路径
    std::string m_roughnessMap; ///< 粗糙度贴图路径
    std::string m_aoMap;        ///< 环境光遮蔽贴图路径
};

} // namespace Moon
