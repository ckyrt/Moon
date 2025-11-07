#pragma once

namespace Moon {

// Forward declaration
class SceneNode;

/**
 * @brief 组件基类 - 所有组件的抽象基类
 * 
 * Component 是附加到 SceneNode 上的功能模块。
 * 未来可以扩展为完整的 ECS (Entity Component System) 架构。
 */
class Component {
public:
    /**
     * @brief 构造函数
     * @param owner 拥有此组件的场景节点
     */
    explicit Component(SceneNode* owner);
    
    /**
     * @brief 虚析构函数
     */
    virtual ~Component() = default;

    // === 状态管理 ===
    
    /**
     * @brief 获取组件是否启用
     */
    bool IsEnabled() const { return m_enabled; }
    
    /**
     * @brief 设置组件启用状态
     * @param enabled true=启用，false=禁用
     */
    void SetEnabled(bool enabled);
    
    /**
     * @brief 获取拥有此组件的场景节点
     */
    SceneNode* GetOwner() const { return m_owner; }

    // === 生命周期回调 (子类可重写) ===
    
    /**
     * @brief 组件被启用时调用
     */
    virtual void OnEnable() {}
    
    /**
     * @brief 组件被禁用时调用
     */
    virtual void OnDisable() {}
    
    /**
     * @brief 每帧更新
     * @param deltaTime 距离上一帧的时间（秒）
     */
    virtual void Update(float deltaTime) {}

protected:
    SceneNode* m_owner;  ///< 拥有此组件的节点
    bool m_enabled;      ///< 是否启用
};

} // namespace Moon
