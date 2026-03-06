#pragma once
#include "Component.h"
#include "../Camera/Camera.h"
#include <string>

namespace Moon {

// 前向声明
class Texture;

/**
 * @brief 材质预设枚举 - 别墅级材质系统（25种）
 * 基于 6 种 PBR 贴图 + 参数变体实现
 */
enum class MaterialPreset {
    None,                // 无预设
    
    // === 混凝土系列（Concrete044D 贴图）===
    Concrete,            // 原始混凝土（结构、地基）
    ConcreteFloor,       // 水泥地坪（车库、地下室）
    ConcretePolished,    // 抛光混凝土（现代室内地面）
    
    // === 岩石/砖石系列（Rock030 贴图）===
    Rock,                // 岩石
    Brick,               // 红砖墙
    Stone,               // 石材外立面
    Plaster,             // 室内石膏墙
    TileCeramic,         // 瓷砖（厨卫）
    
    // === 木材系列（Wood049 贴图）===
    Wood,                // 原木
    WoodFloor,           // 木地板
    WoodPolished,        // 抛光木（高档家具）
    WoodPainted,         // 油漆木（柜子、门框）
    
    // === 金属系列（Metal061B 贴图）===
    Metal,               // 通用金属
    Steel,               // 不锈钢（厨卫）
    Aluminum,            // 铝合金（窗框）
    Chrome,              // 镀铬（五金件）
    Copper,              // 铜/黄铜（装饰）
    
    // === 玻璃系列（程序化）===
    Glass,               // 透明玻璃
    GlassFrosted,        // 磨砂玻璃
    GlassTinted,         // 有色玻璃
    
    // === 软装系列（Fabric061 贴图）===
    Fabric,              // 布料
    Leather,             // 皮革（沙发）
    Carpet,              // 地毯
    
    // === 塑料/橡胶系列（Plastic018A + Fabric061 贴图）===
    Plastic,             // 塑料
    Rubber,              // 橡胶（轮胎、密封条）
};

/**
 * @brief 纹理映射模式
 * UV:       使用顶点UV坐标，适用于普通模型（GLTF/FBX等）
 * Triplanar: 使用世界空间三平面投影，适用于CSG/程序化几何
 */
enum class MappingMode {
    UV,         // 使用UV坐标 + TBN法线（普通模型）
    Triplanar   // 使用世界空间triplanar（CSG模型）
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
    
    /**
     * @brief 设置不透明度
     * @param opacity 不透明度 [0.0 = 完全透明, 1.0 = 完全不透明]
     */
    void SetOpacity(float opacity);
    
    /**
     * @brief 获取不透明度
     */
    float GetOpacity() const { return m_opacity; }
    
    /**
     * @brief 设置透射颜色（用于玻璃等透明材质）
     * @param color RGB 颜色 [0.0 - 1.0]
     */
    void SetTransmissionColor(const Vector3& color);
    
    /**
     * @brief 获取透射颜色
     */
    const Vector3& GetTransmissionColor() const { return m_transmissionColor; }

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
    
    // === 纹理映射模式 ===
    
    /**
     * @brief 设置纹理映射模式
     * @param mode UV = 普通模型，Triplanar = CSG模型
     */
    void SetMappingMode(MappingMode mode) { m_mappingMode = mode; }
    
    /**
     * @brief 获取纹理映射模式
     */
    MappingMode GetMappingMode() const { return m_mappingMode; }
    
    /**
     * @brief 设置Triplanar平铺密度
     * @param tiling 平铺密度（0.5 = 每2米重复，1.0 = 每米重复）
     */
    void SetTriplanarTiling(float tiling) { m_triplanarTiling = tiling; }
    float GetTriplanarTiling() const { return m_triplanarTiling; }
    
    /**
     * @brief 设置Triplanar混合锐度
     * @param blend 混合锐度（越高过渡越硬，默认4.0）
     */
    void SetTriplanarBlend(float blend) { m_triplanarBlend = blend; }
    float GetTriplanarBlend() const { return m_triplanarBlend; }

private:
    float m_metallic;
    float m_roughness;
    Vector3 m_baseColor;
    float m_opacity;                // 不透明度 [0.0 = 完全透明, 1.0 = 完全不透明]
    Vector3 m_transmissionColor;    // 透射颜色（用于玻璃）
    
    std::string m_albedoMap;
    std::string m_normalMap;
    std::string m_aoMap;
    std::string m_roughnessMap;
    std::string m_metalnessMap;
    
    // 当前材质预设
    MaterialPreset m_currentPreset;  ///< 当前使用的材质预设
    
    // 纹理映射模式
    MappingMode m_mappingMode = MappingMode::UV;  ///< 默认使用UV映射
    float m_triplanarTiling = 0.5f;               ///< Triplanar平铺密度
    float m_triplanarBlend = 4.0f;                ///< Triplanar混合锐度
    
    // 材质预设内部实现（25种）
    void SetPresetConcrete();
    void SetPresetConcreteFloor();
    void SetPresetConcretePolished();
    
    void SetPresetRock();
    void SetPresetBrick();
    void SetPresetStone();
    void SetPresetPlaster();
    void SetPresetTileCeramic();
    
    void SetPresetWood();
    void SetPresetWoodFloor();
    void SetPresetWoodPolished();
    void SetPresetWoodPainted();
    
    void SetPresetMetal();
    void SetPresetSteel();
    void SetPresetAluminum();
    void SetPresetChrome();
    void SetPresetCopper();
    
    void SetPresetGlass();
    void SetPresetGlassFrosted();
    void SetPresetGlassTinted();
    
    void SetPresetFabric();
    void SetPresetLeather();
    void SetPresetCarpet();
    
    void SetPresetPlastic();
    void SetPresetRubber();
};

} // namespace Moon
