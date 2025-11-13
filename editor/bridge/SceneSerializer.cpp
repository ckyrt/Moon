#include "SceneSerializer.h"
#include "../../external/nlohmann/json.hpp"
#include "../../engine/core/Scene/MeshRenderer.h"
#include "../../engine/core/Logging/Logger.h"
#include <typeinfo>

using json = nlohmann::json;

// ============================================================================
// 序列化完整场景
// ============================================================================
std::string SceneSerializer::GetSceneHierarchy(Moon::Scene* scene) {
    if (!scene) {
        MOON_LOG_ERROR("SceneSerializer", "Scene is nullptr!");
        return "{}";
    }

    try {
        json result;
        result["name"] = scene->GetName();
        result["rootNodes"] = json::array();
        result["allNodes"] = json::object();

        // 遍历所有节点
        scene->Traverse([&](Moon::SceneNode* node) {
            if (!node) return;

            json nodeData;
            SerializeNode(node, &nodeData);
            
            // 添加到 allNodes 字典
            result["allNodes"][std::to_string(node->GetID())] = nodeData;
            
            // 如果是根节点，添加到 rootNodes 数组
            if (node->GetParent() == nullptr) {
                result["rootNodes"].push_back(node->GetID());
            }
        });

        return result.dump();
    }
    catch (const std::exception& e) {
        MOON_LOG_ERROR("SceneSerializer", "Failed to serialize scene: %s", e.what());
        return "{}";
    }
}

// ============================================================================
// 获取单个节点详情
// ============================================================================
std::string SceneSerializer::GetNodeDetails(Moon::Scene* scene, uint32_t nodeId) {
    if (!scene) {
        MOON_LOG_ERROR("SceneSerializer", "Scene is nullptr!");
        return "{}";
    }

    Moon::SceneNode* node = scene->FindNodeByID(nodeId);
    if (!node) {
        MOON_LOG_ERROR("SceneSerializer", "Node with ID %u not found!", nodeId);
        return "{}";
    }

    try {
        json nodeData;
        SerializeNode(node, &nodeData);
        return nodeData.dump();
    }
    catch (const std::exception& e) {
        MOON_LOG_ERROR("SceneSerializer", "Failed to serialize node %u: %s", nodeId, e.what());
        return "{}";
    }
}

// ============================================================================
// 序列化单个节点
// ============================================================================
void SceneSerializer::SerializeNode(Moon::SceneNode* node, void* jsonObject) {
    if (!node || !jsonObject) return;

    json& nodeData = *static_cast<json*>(jsonObject);

    // 基础信息
    nodeData["id"] = node->GetID();
    nodeData["name"] = node->GetName();
    nodeData["active"] = node->IsActive();

    // 父节点 ID
    if (node->GetParent()) {
        nodeData["parentId"] = node->GetParent()->GetID();
    } else {
        nodeData["parentId"] = nullptr;
    }

    // 子节点 ID 列表
    nodeData["children"] = json::array();
    for (size_t i = 0; i < node->GetChildCount(); ++i) {
        Moon::SceneNode* child = node->GetChild(i);
        if (child) {
            nodeData["children"].push_back(child->GetID());
        }
    }

    // Transform
    SerializeTransform(node->GetTransform(), &nodeData["transform"]);

    // Components
    nodeData["components"] = json::array();
    SerializeComponents(node, &nodeData["components"]);
}

// ============================================================================
// 序列化 Transform
// ============================================================================
void SceneSerializer::SerializeTransform(Moon::Transform* transform, void* jsonObject) {
    if (!transform || !jsonObject) return;

    json& transformData = *static_cast<json*>(jsonObject);

    // Position
    auto pos = transform->GetLocalPosition();
    transformData["position"] = {
        {"x", pos.x},
        {"y", pos.y},
        {"z", pos.z}
    };

    // Rotation (Euler angles)
    auto rot = transform->GetLocalRotation();
    transformData["rotation"] = {
        {"x", rot.x},
        {"y", rot.y},
        {"z", rot.z}
    };

    // Scale
    auto scale = transform->GetLocalScale();
    transformData["scale"] = {
        {"x", scale.x},
        {"y", scale.y},
        {"z", scale.z}
    };
}

// ============================================================================
// 序列化 Components
// ============================================================================
void SceneSerializer::SerializeComponents(Moon::SceneNode* node, void* jsonArray) {
    if (!node || !jsonArray) return;

    json& components = *static_cast<json*>(jsonArray);

    // 检查 MeshRenderer 组件
    if (auto* meshRenderer = node->GetComponent<Moon::MeshRenderer>()) {
        json comp;
        comp["type"] = "MeshRenderer";
        comp["enabled"] = meshRenderer->IsEnabled();
        comp["visible"] = meshRenderer->IsVisible();
        
        // Mesh 信息
        if (meshRenderer->GetMesh()) {
            comp["hasMesh"] = true;
            // TODO: 如果 Mesh 有名称属性，可以在这里添加
            // comp["meshName"] = meshRenderer->GetMesh()->GetName();
        } else {
            comp["hasMesh"] = false;
        }

        components.push_back(comp);
    }

    // TODO: 添加其他组件类型的序列化
    // - Light
    // - Camera
    // - RigidBody
    // 等等...
}
