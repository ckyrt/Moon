#pragma once
#include "Component.h"
#include "../Camera/Camera.h"

namespace Moon {

/**
 * @brief Light 组件 - 场景光源
 * 
 * 支持多种光源类型：
 * - Directional Light（方向光）：模拟太阳光，平行光线
 * - Point Light（点光源）：从一点向四周发散的光，有衰减
 * - Spot Light（聚光灯）：锥形光束，有方向和范围
 */
class Light : public Component {
public:
    /**
     * @brief 光源类型枚举
     */
    enum class Type {
        Directional,  ///< 方向光（太阳光）
        Point,        ///< 点光源（灯泡）
        Spot          ///< 聚光灯（手电筒）
    };

    /**
     * @brief 构造函数
     * @param owner 拥有此组件的场景节点
     */
    explicit Light(SceneNode* owner);
    
    ~Light() override = default;

    // === 光源类型 ===
    
    /**
     * @brief 设置光源类型
     * @param type 光源类型
     */
    void SetType(Type type) { m_type = type; }
    
    /**
     * @brief 获取光源类型
     */
    Type GetType() const { return m_type; }

    // === 基本属性 ===
    
    /**
     * @brief 设置光源颜色
     * @param color RGB 颜色 [0.0 - 1.0]
     */
    void SetColor(const Vector3& color);
    
    /**
     * @brief 获取光源颜色
     */
    const Vector3& GetColor() const { return m_color; }
    
    /**
     * @brief 设置光源强度（亮度）
     * @param intensity 强度值 [0.0 - ∞]
     */
    void SetIntensity(float intensity);
    
    /**
     * @brief 获取光源强度
     */
    float GetIntensity() const { return m_intensity; }

    // === 方向光专用 ===
    
    /**
     * @brief 获取光源方向（世界坐标系）
     * 
     * 对于方向光，方向从 Transform 的 forward 向量获取
     * 对于点光源，此方法无意义
     * 对于聚光灯，表示光束中心方向
     * 
     * @return 归一化的方向向量
     */
    Vector3 GetDirection() const;

    // === 点光源/聚光灯专用 ===
    
    /**
     * @brief 设置光源范围（影响距离）
     * @param range 范围（单位：米）
     * 
     * 仅对 Point Light 和 Spot Light 有效
     */
    void SetRange(float range);
    
    /**
     * @brief 获取光源范围
     */
    float GetRange() const { return m_range; }
    
    /**
     * @brief 设置衰减系数
     * @param constant 常数衰减
     * @param linear 线性衰减
     * @param quadratic 二次衰减
     * 
     * 衰减公式：attenuation = 1.0 / (constant + linear * distance + quadratic * distance^2)
     * 仅对 Point Light 和 Spot Light 有效
     */
    void SetAttenuation(float constant, float linear, float quadratic);
    
    /**
     * @brief 获取衰减系数
     */
    void GetAttenuation(float& constant, float& linear, float& quadratic) const;

    // === 聚光灯专用 ===
    
    /**
     * @brief 设置聚光灯内锥角和外锥角
     * @param innerConeAngle 内锥角（度），光照完全强度的范围
     * @param outerConeAngle 外锥角（度），光照衰减到 0 的边界
     * 
     * 仅对 Spot Light 有效
     */
    void SetSpotAngles(float innerConeAngle, float outerConeAngle);
    
    /**
     * @brief 获取聚光灯角度
     */
    void GetSpotAngles(float& innerConeAngle, float& outerConeAngle) const;

    // === 阴影相关（未来扩展）===
    
    /**
     * @brief 设置是否投射阴影
     */
    void SetCastShadows(bool castShadows) { m_castShadows = castShadows; }
    
    /**
     * @brief 获取是否投射阴影
     */
    bool GetCastShadows() const { return m_castShadows; }

private:
    Type m_type;              ///< 光源类型
    Vector3 m_color;          ///< 光源颜色 (RGB)
    float m_intensity;        ///< 光源强度
    
    // 点光源/聚光灯属性
    float m_range;            ///< 影响范围
    float m_attenuationConstant;   ///< 常数衰减
    float m_attenuationLinear;     ///< 线性衰减
    float m_attenuationQuadratic;  ///< 二次衰减
    
    // 聚光灯属性
    float m_spotInnerConeAngle;    ///< 内锥角（度）
    float m_spotOuterConeAngle;    ///< 外锥角（度）
    
    // 阴影
    bool m_castShadows;       ///< 是否投射阴影
};

} // namespace Moon
