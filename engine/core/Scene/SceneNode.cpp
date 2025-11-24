#include "SceneNode.h"
#include "Scene.h"
#include "../Logging/Logger.h"

namespace Moon {

// 静态成员初始化
uint32_t SceneNode::s_nextID = 1;

SceneNode::SceneNode(const std::string& name)
    : m_id(s_nextID++)
    , m_name(name)
    , m_active(true)
    , m_transform(this)
    , m_parent(nullptr)
    , m_scene(nullptr)
{
}

// 🎯 Undo/Redo 专用构造函数（指定 ID）
SceneNode::SceneNode(uint32_t id, const std::string& name)
    : m_id(id)
    , m_name(name)
    , m_active(true)
    , m_transform(this)
    , m_parent(nullptr)
    , m_scene(nullptr)
{
    // 🚨 更新全局 ID 计数器（防止 ID 冲突）
    if (id >= s_nextID) {
        s_nextID = id + 1;
    }
}

SceneNode::~SceneNode() {
    // 清理所有组件
    for (Component* comp : m_components) {
        delete comp;
    }
    m_components.clear();
    
    // 移除所有子节点的父指针（但不删除子节点，由 Scene 管理）
    for (SceneNode* child : m_children) {
        if (child) {
            child->m_parent = nullptr;
        }
    }
    m_children.clear();
}

// === 激活状态 ===

void SceneNode::SetActive(bool active) {
    if (m_active == active) {
        return;
    }
    
    m_active = active;
    
    // 递归设置所有子节点
    for (SceneNode* child : m_children) {
        if (child) {
            child->SetActive(active);
        }
    }
}

// === 父子层级关系 ===

void SceneNode::SetParent(SceneNode* parent, bool worldPositionStays) {
    if (m_parent == parent) {
        return;
    }
    
    // 如果需要保持世界坐标，先保存当前世界坐标
    Vector3 worldPos, worldScale;
    Quaternion worldRot;
    
    if (worldPositionStays) {
        worldPos = m_transform.GetWorldPosition();
        worldRot = m_transform.GetWorldRotation();
        worldScale = m_transform.GetWorldScale();
    }
    
    // 从旧父节点移除
    if (m_parent) {
        m_parent->RemoveChild(this);
    }
    
    // 设置新父节点
    m_parent = parent;
    
    // 添加到新父节点的子列表
    if (m_parent) {
        m_parent->AddChild(this);
    }
    
    // 如果需要保持世界坐标，重新计算本地坐标
    if (worldPositionStays) {
        m_transform.SetWorldPosition(worldPos);
        m_transform.SetWorldRotation(worldRot);
        m_transform.SetWorldScale(worldScale);
    } else {
        // 标记 Transform 为脏
        m_transform.MarkDirty();
    }
    
    // 通知场景（用于更新根节点列表）
    if (m_scene) {
        if (m_parent == nullptr) {
            m_scene->AddRootNode(this);
        } else {
            m_scene->RemoveRootNode(this);
        }
    }
}

SceneNode* SceneNode::GetChild(size_t index) const {
    if (index >= m_children.size()) {
        return nullptr;
    }
    return m_children[index];
}

void SceneNode::AddChild(SceneNode* child) {
    if (!child || child == this) {
        return;
    }
    
    // 检查是否已存在
    for (SceneNode* existing : m_children) {
        if (existing == child) {
            return;  // 已经是子节点了
        }
    }
    
    m_children.push_back(child);
    
    // 如果子节点的父指针还没设置，设置它
    if (child->m_parent != this) {
        child->m_parent = this;
        child->m_transform.MarkDirty();
    }
}

void SceneNode::RemoveChild(SceneNode* child) {
    if (!child) {
        return;
    }
    
    for (auto it = m_children.begin(); it != m_children.end(); ++it) {
        if (*it == child) {
            m_children.erase(it);
            return;
        }
    }
}

SceneNode* SceneNode::FindChild(const std::string& name, bool recursive) {
    // 查找直接子节点
    for (SceneNode* child : m_children) {
        if (child && child->GetName() == name) {
            return child;
        }
    }
    
    // 递归查找
    if (recursive) {
        for (SceneNode* child : m_children) {
            if (child) {
                SceneNode* found = child->FindChild(name, true);
                if (found) {
                    return found;
                }
            }
        }
    }
    
    return nullptr;
}

// === 组件系统 ===

void SceneNode::AddComponentInternal(Component* component) {
    if (!component) {
        return;
    }
    
    m_components.push_back(component);
    
    if (m_active && component->IsEnabled()) {
        component->OnEnable();
    }
}

// === 更新 ===

void SceneNode::Update(float deltaTime) {
    if (!m_active) {
        return;
    }
    
    // 更新所有组件
    for (Component* comp : m_components) {
        if (comp && comp->IsEnabled()) {
            comp->Update(deltaTime);
        }
    }
    
    // 递归更新子节点
    for (SceneNode* child : m_children) {
        if (child) {
            child->Update(deltaTime);
        }
    }
}

// === 内部方法 ===

void SceneNode::SetScene(Scene* scene) {
    m_scene = scene;
    
    // 递归设置所有子节点的场景
    for (SceneNode* child : m_children) {
        if (child) {
            child->SetScene(scene);
        }
    }
}

void SceneNode::NotifyTransformChanged() {
    m_transform.MarkDirty();
}

} // namespace Moon
