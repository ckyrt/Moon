#pragma once
#include "Transform.h"
#include "Component.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Moon {

// Forward declaration
class Scene;

/**
 * @brief 场景节点 - 场景中的实体对象（类似 Unity 的 GameObject）
 * 
 * SceneNode 是场景中的基本单位，代表一个游戏对象。
 * 每个节点包含：
 * - Transform 组件（必需）
 * - 父子层级关系
 * - 其他组件列表
 * - 唯一 ID 和名称
 */
class SceneNode {
public:
    /**
     * @brief 构造函数
     * @param name 节点名称
     */
    explicit SceneNode(const std::string& name = "GameObject");
    
    /**
     * @brief 构造函数（指定 ID）
     * @param id 节点 ID
     * @param name 节点名称
     * 
     * ⚠️ 内部 API：仅供 Scene::CreateNodeWithID 使用（用于 Undo/Redo）
     * ⚠️ 注意：调用者需要确保 ID 不重复，并更新 s_nextID
     */
    SceneNode(uint32_t id, const std::string& name);
    
    /**
     * @brief 析构函数
     */
    ~SceneNode();

    // === 身份标识 ===
    
    /**
     * @brief 获取节点唯一 ID
     */
    uint32_t GetID() const { return m_id; }
    
    /**
     * @brief 获取节点名称
     */
    const std::string& GetName() const { return m_name; }
    
    /**
     * @brief 设置节点名称
     */
    void SetName(const std::string& name) { m_name = name; }

    // === 激活状态 ===
    
    /**
     * @brief 获取节点是否激活
     */
    bool IsActive() const { return m_active; }
    
    /**
     * @brief 设置节点激活状态
     * @param active true=激活，false=禁用
     */
    void SetActive(bool active);

    // === Transform 组件 ===
    
    /**
     * @brief 获取 Transform 组件
     */
    Transform* GetTransform() { return &m_transform; }
    
    /**
     * @brief 获取 Transform 组件（const 版本）
     */
    const Transform* GetTransform() const { return &m_transform; }

    // === 父子层级关系 ===
    
    /**
     * @brief 获取父节点
     */
    SceneNode* GetParent() const { return m_parent; }
    
    /**
     * @brief 设置父节点
     * @param parent 新的父节点（nullptr 表示设为根节点）
     * @param worldPositionStays 是否保持世界坐标不变（默认为 true，与 Unity 一致）
     */
    void SetParent(SceneNode* parent, bool worldPositionStays = true);
    
    /**
     * @brief 获取子节点数量
     */
    size_t GetChildCount() const { return m_children.size(); }
    
    /**
     * @brief 获取指定索引的子节点
     * @param index 子节点索引
     * @return 子节点指针，如果索引越界返回 nullptr
     */
    SceneNode* GetChild(size_t index) const;
    
    /**
     * @brief 添加子节点
     * @param child 要添加的子节点
     */
    void AddChild(SceneNode* child);
    
    /**
     * @brief 移除子节点
     * @param child 要移除的子节点
     */
    void RemoveChild(SceneNode* child);
    
    /**
     * @brief 查找子节点（按名称）
     * @param name 子节点名称
     * @param recursive 是否递归查找
     * @return 找到的节点，未找到返回 nullptr
     */
    SceneNode* FindChild(const std::string& name, bool recursive = false);

    // === 组件系统（简化版）===
    
    /**
     * @brief 添加组件
     * @tparam T 组件类型
     * @return 新创建的组件指针
     */
    template<typename T>
    T* AddComponent();
    
    /**
     * @brief 获取组件
     * @tparam T 组件类型
     * @return 组件指针，未找到返回 nullptr
     */
    template<typename T>
    T* GetComponent() const;
    
    /**
     * @brief 移除组件
     * @tparam T 组件类型
     */
    template<typename T>
    void RemoveComponent();
    
    /**
     * @brief 添加组件（基类指针版本）
     * @param component 组件指针
     */
    void AddComponentInternal(Component* component);

    // === 更新 ===
    
    /**
     * @brief 更新节点及其所有组件
     * @param deltaTime 距离上一帧的时间（秒）
     */
    void Update(float deltaTime);

    // === 内部使用 ===
    
    /**
     * @brief 获取所属场景
     */
    Scene* GetScene() const { return m_scene; }

private:
    static uint32_t s_nextID;  ///< 下一个可用的 ID
    
    uint32_t m_id;             ///< 节点唯一 ID
    std::string m_name;        ///< 节点名称
    bool m_active;             ///< 是否激活
    
    Transform m_transform;     ///< Transform 组件
    
    SceneNode* m_parent;                   ///< 父节点
    std::vector<SceneNode*> m_children;    ///< 子节点列表
    std::vector<Component*> m_components;  ///< 组件列表
    
    Scene* m_scene;            ///< 所属场景
    
    // 供 Scene 类使用的内部方法
    friend class Scene;
    void SetScene(Scene* scene);
    void NotifyTransformChanged();
};

// === 模板实现 ===

template<typename T>
T* SceneNode::AddComponent() {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
    
    T* component = new T(this);
    AddComponentInternal(component);
    return component;
}

template<typename T>
T* SceneNode::GetComponent() const {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
    
    for (Component* comp : m_components) {
        T* result = dynamic_cast<T*>(comp);
        if (result) {
            return result;
        }
    }
    return nullptr;
}

template<typename T>
void SceneNode::RemoveComponent() {
    static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");
    
    for (auto it = m_components.begin(); it != m_components.end(); ++it) {
        T* result = dynamic_cast<T*>(*it);
        if (result) {
            delete *it;
            m_components.erase(it);
            return;
        }
    }
}

} // namespace Moon
