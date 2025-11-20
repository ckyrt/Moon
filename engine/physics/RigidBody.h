#pragma once

#include "core/Scene/Component.h"
#include "core/Math/Vector3.h"
#include "core/Math/Quaternion.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

namespace Moon {

// 前置声明
class PhysicsSystem;

/**
 * @brief 物理形状类型
 */
enum class PhysicsShapeType {
    Box,        // 盒体
    Sphere,     // 球体
    Capsule,    // 胶囊体
    Cylinder    // 圆柱体
};

/**
 * @brief 刚体组件 - 为场景节点提供物理模拟
 * 
 * RigidBody 组件封装了 Jolt Physics 的刚体功能，
 * 允许场景节点参与物理模拟（碰撞、重力、力等）。
 */
class RigidBody : public Component {
public:
    /**
     * @brief 构造函数
     * @param owner 拥有此组件的场景节点
     */
    explicit RigidBody(SceneNode* owner);
    
    /**
     * @brief 析构函数 - 自动清理物理资源
     */
    ~RigidBody() override;

    // === 初始化 ===
    
    /**
     * @brief 创建物理刚体
     * @param physicsSystem 物理系统实例
     * @param shapeType 形状类型
     * @param size 形状尺寸（Box: 半尺寸, Sphere/Capsule: radius, Cylinder: radius + halfHeight）
     * @param mass 质量（0 = 静态物体）
     */
    void CreateBody(
        PhysicsSystem* physicsSystem,
        PhysicsShapeType shapeType,
        const Vector3& size,
        float mass = 1.0f
    );

    /**
     * @brief 销毁物理刚体
     */
    void DestroyBody();

    // === 属性访问 ===
    
    /**
     * @brief 获取 Jolt BodyID
     */
    JPH::BodyID GetBodyID() const { return m_bodyID; }
    
    /**
     * @brief 判断是否已创建物理体
     */
    bool HasBody() const { return m_bodyID.IsInvalid() == false; }
    
    /**
     * @brief 获取质量
     */
    float GetMass() const { return m_mass; }
    
    /**
     * @brief 获取形状类型
     */
    PhysicsShapeType GetShapeType() const { return m_shapeType; }
    
    /**
     * @brief 获取形状尺寸
     */
    const Vector3& GetSize() const { return m_size; }

    // === 物理操作 ===
    
    /**
     * @brief 施加力
     * @param force 力向量
     */
    void AddForce(const Vector3& force);
    
    /**
     * @brief 施加冲量
     * @param impulse 冲量向量
     */
    void AddImpulse(const Vector3& impulse);
    
    /**
     * @brief 设置线性速度
     * @param velocity 速度向量
     */
    void SetLinearVelocity(const Vector3& velocity);
    
    /**
     * @brief 获取线性速度
     */
    Vector3 GetLinearVelocity() const;
    
    /**
     * @brief 设置角速度
     * @param angularVelocity 角速度向量
     */
    void SetAngularVelocity(const Vector3& angularVelocity);
    
    /**
     * @brief 获取角速度
     */
    Vector3 GetAngularVelocity() const;

    // === 组件生命周期 ===
    
    /**
     * @brief 组件启用时调用
     */
    void OnEnable() override;
    
    /**
     * @brief 组件禁用时调用
     */
    void OnDisable() override;
    
    /**
     * @brief 每帧更新 - 同步物理状态到 Transform
     * @param deltaTime 时间增量
     */
    void Update(float deltaTime) override;

private:
    PhysicsSystem* m_physicsSystem;  ///< 物理系统引用
    JPH::BodyID m_bodyID;            ///< Jolt 物理体 ID
    PhysicsShapeType m_shapeType;    ///< 形状类型
    Vector3 m_size;                  ///< 形状尺寸
    float m_mass;                    ///< 质量
    bool m_syncToTransform;          ///< 是否同步物理状态到 Transform
};

} // namespace Moon
