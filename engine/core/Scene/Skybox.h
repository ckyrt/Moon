#pragma once
#include "Component.h"
#include "../Math/Math.h"
#include <string>

namespace Moon {

/**
 * @brief Skybox 组件 - 天空盒渲染
 * 
 * 可以附加到场景中的任何 GameObject 上，通常是一个专用的 "Skybox" GameObject
 * 类似 Unity 的 Skybox 组件，但使用 Component 架构
 * 
 * 使用方式：
 * ```cpp
 * SceneNode* skyboxNode = scene->CreateNode("Skybox");
 * Skybox* skybox = skyboxNode->AddComponent<Skybox>();
 * skybox->LoadEnvironmentMap("assets/textures/environment.hdr");
 * ```
 */
class Skybox : public Component {
public:
    /**
     * @brief 天空盒类型
     */
    enum class Type {
        None,                    ///< 无天空盒（纯色背景）
        EquirectangularHDR,     ///< HDR 环境贴图（equirectangular 格式）
        Cubemap,                ///< 立方体贴图
        Procedural              ///< 程序化生成（未来：大气散射）
    };

    /**
     * @brief 构造函数
     * @param owner 拥有此组件的场景节点
     */
    explicit Skybox(SceneNode* owner);
    
    ~Skybox() override = default;

    // === 天空盒配置 ===
    
    /**
     * @brief 设置天空盒类型
     */
    void SetType(Type type) { m_type = type; }
    
    /**
     * @brief 获取天空盒类型
     */
    Type GetType() const { return m_type; }
    
    /**
     * @brief 加载环境贴图
     * @param filepath HDR 环境贴图路径（.hdr 格式）
     * @return 成功返回 true
     */
    bool LoadEnvironmentMap(const std::string& filepath);
    
    /**
     * @brief 获取环境贴图路径
     */
    const std::string& GetEnvironmentMapPath() const { return m_environmentMapPath; }
    
    /**
     * @brief 设置天空盒亮度
     * @param intensity 亮度倍数 [0.0 - ∞]
     */
    void SetIntensity(float intensity);
    
    /**
     * @brief 获取天空盒亮度
     */
    float GetIntensity() const { return m_intensity; }
    
    /**
     * @brief 设置天空盒旋转（Y轴）
     * @param rotation 旋转角度（度）
     */
    void SetRotation(float rotation) { m_rotation = rotation; }
    
    /**
     * @brief 获取天空盒旋转
     */
    float GetRotation() const { return m_rotation; }
    
    /**
     * @brief 设置天空盒色调
     * @param tint RGB 色调 [0.0 - 1.0]，默认 (1, 1, 1) 无色调
     */
    void SetTint(const Vector3& tint);
    
    /**
     * @brief 获取天空盒色调
     */
    const Vector3& GetTint() const { return m_tint; }
    
    /**
     * @brief 设置是否启用 IBL（基于图像的照明）
     */
    void SetEnableIBL(bool enable) { m_enableIBL = enable; }
    
    /**
     * @brief 获取是否启用 IBL
     */
    bool IsIBLEnabled() const { return m_enableIBL; }

    // === 渲染相关 ===
    
    /**
     * @brief 是否需要重新加载资源
     * 
     * 渲染器可以查询此标志以决定是否需要重新加载 GPU 资源
     */
    bool NeedsReload() const { return m_needsReload; }
    
    /**
     * @brief 清除重新加载标志
     * 
     * 渲染器加载完资源后应调用此方法
     */
    void ClearReloadFlag() { m_needsReload = false; }

private:
    Type m_type;                        ///< 天空盒类型
    std::string m_environmentMapPath;   ///< 环境贴图路径
    float m_intensity;                  ///< 天空盒亮度
    float m_rotation;                   ///< Y轴旋转（度）
    Vector3 m_tint;                     ///< 色调
    bool m_enableIBL;                   ///< 是否启用 IBL
    bool m_needsReload;                 ///< 是否需要重新加载资源
};

} // namespace Moon
