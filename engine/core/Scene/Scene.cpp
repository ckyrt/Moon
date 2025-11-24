#include "Scene.h"
#include "../Logging/Logger.h"

namespace Moon {

Scene::Scene(const std::string& name)
    : m_name(name)
{
    MOON_LOG_INFO("Scene", "Created scene: %s", name.c_str());
}

Scene::~Scene() {
    MOON_LOG_INFO("Scene", "Destroying scene: %s", m_name.c_str());
    
    // 删除所有节点
    for (SceneNode* node : m_allNodes) {
        delete node;
    }
    m_allNodes.clear();
    m_rootNodes.clear();
    m_pendingDelete.clear();
}

// === 节点管理 ===

SceneNode* Scene::CreateNode(const std::string& name) {
    SceneNode* node = new SceneNode(name);
    node->SetScene(this);
    
    m_allNodes.push_back(node);
    m_rootNodes.push_back(node);  // 默认作为根节点
    
    return node;
}

// 🎯 Undo/Redo 专用：创建指定 ID 的节点
SceneNode* Scene::CreateNodeWithID(uint32_t id, const std::string& name) {
    // 🚨 检查 ID 是否已存在
    if (FindNodeByID(id)) {
        MOON_LOG_ERROR("Scene", "Cannot create node with ID %u: ID already exists", id);
        return nullptr;
    }
    
    SceneNode* node = new SceneNode(id, name);
    node->SetScene(this);
    
    m_allNodes.push_back(node);
    m_rootNodes.push_back(node);  // 默认作为根节点
    
    MOON_LOG_INFO("Scene", "Created node with ID=%u, name=%s", id, name.c_str());
    
    return node;
}

void Scene::DestroyNode(SceneNode* node) {
    if (!node) {
        return;
    }
    
    // 添加到待删除列表
    m_pendingDelete.push_back(node);
}

void Scene::DestroyNodeImmediate(SceneNode* node) {
    if (!node) {
        return;
    }
    
    // 从父节点移除
    if (node->GetParent()) {
        node->GetParent()->RemoveChild(node);
    }
    
    // 从根节点列表移除
    RemoveRootNode(node);
    
    // 从所有节点列表移除
    for (auto it = m_allNodes.begin(); it != m_allNodes.end(); ++it) {
        if (*it == node) {
            m_allNodes.erase(it);
            break;
        }
    }
    
    // 递归删除所有子节点
    std::vector<SceneNode*> children;
    for (size_t i = 0; i < node->GetChildCount(); ++i) {
        children.push_back(node->GetChild(i));
    }
    for (SceneNode* child : children) {
        DestroyNodeImmediate(child);
    }
    
    // 删除节点
    delete node;
}

SceneNode* Scene::FindNodeByName(const std::string& name) const {
    for (SceneNode* node : m_allNodes) {
        if (node && node->GetName() == name) {
            return node;
        }
    }
    return nullptr;
}

SceneNode* Scene::FindNodeByID(uint32_t id) const {
    for (SceneNode* node : m_allNodes) {
        if (node && node->GetID() == id) {
            return node;
        }
    }
    return nullptr;
}

// === 根节点管理 ===

SceneNode* Scene::GetRootNode(size_t index) const {
    if (index >= m_rootNodes.size()) {
        return nullptr;
    }
    return m_rootNodes[index];
}

void Scene::AddRootNode(SceneNode* node) {
    if (!node) {
        return;
    }
    
    // 检查是否已存在
    for (SceneNode* existing : m_rootNodes) {
        if (existing == node) {
            return;
        }
    }
    
    m_rootNodes.push_back(node);
}

void Scene::RemoveRootNode(SceneNode* node) {
    if (!node) {
        return;
    }
    
    for (auto it = m_rootNodes.begin(); it != m_rootNodes.end(); ++it) {
        if (*it == node) {
            m_rootNodes.erase(it);
            return;
        }
    }
}

// === 更新 ===

void Scene::Update(float deltaTime) {
    // 更新所有根节点（会递归更新子节点）
    for (SceneNode* node : m_rootNodes) {
        if (node) {
            node->Update(deltaTime);
        }
    }
    
    // 处理待删除节点
    ProcessPendingDeletes();
}

// === 遍历 ===

void Scene::Traverse(std::function<void(SceneNode*)> callback) {
    if (!callback) {
        return;
    }
    
    for (SceneNode* node : m_rootNodes) {
        if (node) {
            TraverseNode(node, callback);
        }
    }
}

void Scene::TraverseActive(std::function<void(SceneNode*)> callback) {
    if (!callback) {
        return;
    }
    
    Traverse([&](SceneNode* node) {
        if (node && node->IsActive()) {
            callback(node);
        }
    });
}

// === 私有方法 ===

void Scene::ProcessPendingDeletes() {
    if (m_pendingDelete.empty()) {
        return;
    }
    
    for (SceneNode* node : m_pendingDelete) {
        DestroyNodeImmediate(node);
    }
    
    m_pendingDelete.clear();
}

void Scene::TraverseNode(SceneNode* node, std::function<void(SceneNode*)> callback) {
    if (!node || !callback) {
        return;
    }
    
    // 先处理当前节点
    callback(node);
    
    // 递归处理子节点
    for (size_t i = 0; i < node->GetChildCount(); ++i) {
        SceneNode* child = node->GetChild(i);
        if (child) {
            TraverseNode(child, callback);
        }
    }
}

} // namespace Moon
