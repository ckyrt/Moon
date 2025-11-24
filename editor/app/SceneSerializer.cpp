#include "SceneSerializer.h"
#include "../../engine/core/Scene/Scene.h"
#include "../../engine/core/Scene/SceneNode.h"
#include "../../engine/core/Scene/Transform.h"
#include "../../engine/core/Scene/MeshRenderer.h"
#include "../../engine/physics/RigidBody.h"
#include "../../engine/physics/PhysicsSystem.h"
#include "../../engine/core/Logging/Logger.h"
#include "../../engine/core/EngineCore.h"
#include "../../external/nlohmann/json.hpp"
#include <fstream>

using json = nlohmann::json;

// 外部全局变量（定义在 EditorApp.cpp）
extern Moon::PhysicsSystem* g_PhysicsSystem;

namespace Moon {

// ============================================================================
// 完整场景序列化（Save/Load）
// ============================================================================

bool SceneSerializer::SaveSceneToFile(Scene* scene, const std::string& filePath) {
    if (!scene) {
        MOON_LOG_ERROR("SceneSerializer", "Scene is nullptr!");
        return false;
    }

    try {
        json sceneData;
        sceneData["version"] = "1.0";
        sceneData["name"] = scene->GetName();
        sceneData["nodes"] = json::array();

        // 序列化所有节点（完整数据）
        scene->Traverse([&](SceneNode* node) {
            if (!node) return;

            json nodeData;
            SerializeNodeFull(node, &nodeData);
            sceneData["nodes"].push_back(nodeData);
        });

        // 写入文件
        std::ofstream file(filePath);
        if (!file.is_open()) {
            MOON_LOG_ERROR("SceneSerializer", "Failed to open file: %s", filePath.c_str());
            return false;
        }

        file << sceneData.dump(4);  // 格式化输出，缩进 4 空格
        file.close();

        MOON_LOG_INFO("SceneSerializer", "Scene saved to: %s", filePath.c_str());
        return true;
    }
    catch (const std::exception& e) {
        MOON_LOG_ERROR("SceneSerializer", "Failed to save scene: %s", e.what());
        return false;
    }
}

bool SceneSerializer::LoadSceneFromFile(Scene* scene, EngineCore* engine, const std::string& filePath) {
    if (!scene || !engine) {
        MOON_LOG_ERROR("SceneSerializer", "Scene or Engine is nullptr!");
        return false;
    }

    try {
        // 读取文件
        std::ifstream file(filePath);
        if (!file.is_open()) {
            MOON_LOG_ERROR("SceneSerializer", "Failed to open file: %s", filePath.c_str());
            return false;
        }

        json sceneData = json::parse(file);
        file.close();

        // TODO: 清空当前场景（需要在 Scene 类中添加 Clear() 方法）
        // scene->Clear();

        // 设置场景名称
        if (sceneData.contains("name")) {
            scene->SetName(sceneData["name"]);
        }

        // 反序列化所有节点
        if (sceneData.contains("nodes")) {
            for (const auto& nodeData : sceneData["nodes"]) {
                std::string serializedNode = nodeData.dump();
                DeserializeNode(scene, engine, serializedNode);
            }
        }

        MOON_LOG_INFO("SceneSerializer", "Scene loaded from: %s", filePath.c_str());
        return true;
    }
    catch (const std::exception& e) {
        MOON_LOG_ERROR("SceneSerializer", "Failed to load scene: %s", e.what());
        return false;
    }
}

// ============================================================================
// 场景数据获取（用于编辑器 UI）
// ============================================================================

std::string SceneSerializer::GetSceneHierarchy(Scene* scene) {
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
        scene->Traverse([&](SceneNode* node) {
            if (!node) return;

            json nodeData;
            SerializeNodeBasic(node, &nodeData);
            
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

std::string SceneSerializer::GetNodeDetails(Scene* scene, uint32_t nodeId) {
    if (!scene) {
        MOON_LOG_ERROR("SceneSerializer", "Scene is nullptr!");
        return "{}";
    }

    SceneNode* node = scene->FindNodeByID(nodeId);
    if (!node) {
        MOON_LOG_ERROR("SceneSerializer", "Node with ID %u not found!", nodeId);
        return "{}";
    }

    try {
        json nodeData;
        SerializeNodeBasic(node, &nodeData);
        return nodeData.dump();
    }
    catch (const std::exception& e) {
        MOON_LOG_ERROR("SceneSerializer", "Failed to serialize node %u: %s", nodeId, e.what());
        return "{}";
    }
}

// ============================================================================
// 单节点序列化（用于 Undo/Redo、网络同步）
// ============================================================================

std::string SceneSerializer::SerializeNode(Scene* scene, uint32_t nodeId) {
    if (!scene) {
        MOON_LOG_ERROR("SceneSerializer", "Scene is nullptr!");
        return "{}";
    }

    SceneNode* node = scene->FindNodeByID(nodeId);
    if (!node) {
        MOON_LOG_ERROR("SceneSerializer", "Node with ID %u not found!", nodeId);
        return "{}";
    }

    try {
        json nodeData;
        SerializeNodeFull(node, &nodeData);
        
        MOON_LOG_INFO("SceneSerializer", "Serialized node %u with %zu components", 
                     nodeId, nodeData["components"].size());

        return nodeData.dump();
    }
    catch (const std::exception& e) {
        MOON_LOG_ERROR("SceneSerializer", "Failed to serialize node %u: %s", nodeId, e.what());
        return "{}";
    }
}

SceneNode* SceneSerializer::DeserializeNode(Scene* scene, EngineCore* engine, const std::string& serializedData) {
    if (!scene || !engine) {
        MOON_LOG_ERROR("SceneSerializer", "Scene or Engine is nullptr!");
        return nullptr;
    }

    try {
        json nodeData = json::parse(serializedData);

        uint32_t nodeId = nodeData["id"];
        std::string name = nodeData["name"];
        bool active = nodeData.value("active", true);

        // 🚨 检查 ID 是否已存在
        if (scene->FindNodeByID(nodeId)) {
            MOON_LOG_ERROR("SceneSerializer", "Node with ID %u already exists!", nodeId);
            return nullptr;
        }

        // 创建节点（使用原始 ID）
        SceneNode* node = scene->CreateNodeWithID(nodeId, name);
        if (!node) {
            MOON_LOG_ERROR("SceneSerializer", "Failed to create node with ID %u", nodeId);
            return nullptr;
        }

        node->SetActive(active);

        // 设置 Transform
        if (nodeData.contains("transform")) {
            const json& transform = nodeData["transform"];
            
            if (transform.contains("position")) {
                const json& pos = transform["position"];
                node->GetTransform()->SetLocalPosition(Vector3(
                    pos["x"], pos["y"], pos["z"]
                ));
            }
            
            if (transform.contains("rotation")) {
                const json& rot = transform["rotation"];
                node->GetTransform()->SetLocalRotation(Quaternion(
                    rot["x"], rot["y"], rot["z"], rot["w"]
                ));
            }
            
            if (transform.contains("scale")) {
                const json& scale = transform["scale"];
                node->GetTransform()->SetLocalScale(Vector3(
                    scale["x"], scale["y"], scale["z"]
                ));
            }
        }

        // 反序列化 Components
        if (nodeData.contains("components")) {
            json componentsArray = nodeData["components"];
            DeserializeComponents(node, engine, &componentsArray);
        }

        // 设置父节点（根据序列化数据中的 parentId）
        if (nodeData.contains("parentId") && !nodeData["parentId"].is_null()) {
            uint32_t parentId = nodeData["parentId"];
            SceneNode* parent = scene->FindNodeByID(parentId);
            if (parent) {
                node->SetParent(parent);
                MOON_LOG_INFO("SceneSerializer", "Set parent %u for node %u", parentId, nodeId);
            } else {
                MOON_LOG_WARN("SceneSerializer", "Parent node %u not found, node %u will be root", 
                             parentId, nodeId);
            }
        }

        // 🎯 递归反序列化所有子节点（恢复完整的子树）
        if (nodeData.contains("childrenData") && nodeData["childrenData"].is_array()) {
            for (const auto& childData : nodeData["childrenData"]) {
                std::string childSerialized = childData.dump();
                SceneNode* childNode = DeserializeNode(scene, engine, childSerialized);
                if (childNode) {
                    // 子节点在递归调用中已经通过 parentId 设置了父节点
                    // 这里只是确保关系正确（理论上不需要再设置）
                    if (childNode->GetParent() != node) {
                        MOON_LOG_WARN("SceneSerializer", "Child node %u parent mismatch, fixing...", 
                                     childNode->GetID());
                        childNode->SetParent(node);
                    }
                    MOON_LOG_INFO("SceneSerializer", "Restored child node %u under parent %u", 
                                 childNode->GetID(), nodeId);
                }
            }
        }

        MOON_LOG_INFO("SceneSerializer", "Deserialized node %u: %s with %zu children", 
                     nodeId, name.c_str(), 
                     nodeData.contains("childrenData") ? nodeData["childrenData"].size() : 0);
        return node;
    }
    catch (const std::exception& e) {
        MOON_LOG_ERROR("SceneSerializer", "Failed to deserialize node: %s", e.what());
        return nullptr;
    }
}

// ============================================================================
// 内部辅助函数 - 序列化（基础版本，用于 UI）
// ============================================================================

void SceneSerializer::SerializeNodeBasic(SceneNode* node, void* jsonObject) {
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
        SceneNode* child = node->GetChild(i);
        if (child) {
            nodeData["children"].push_back(child->GetID());
        }
    }

    // Transform
    SerializeTransform(node->GetTransform(), &nodeData["transform"]);

    // Components（基础信息）
    nodeData["components"] = json::array();
    SerializeComponents(node, &nodeData["components"]);
}

// ============================================================================
// 内部辅助函数 - 序列化（完整版本，用于 Save/Load/Undo）
// ============================================================================

void SceneSerializer::SerializeNodeFull(SceneNode* node, void* jsonObject) {
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

    // 🎯 递归序列化所有子节点的完整数据（用于 Undo/Redo）
    nodeData["childrenData"] = json::array();
    for (size_t i = 0; i < node->GetChildCount(); ++i) {
        SceneNode* child = node->GetChild(i);
        if (child) {
            json childData;
            SerializeNodeFull(child, &childData);  // 递归序列化
            nodeData["childrenData"].push_back(childData);
        }
    }

    // Transform
    SerializeTransform(node->GetTransform(), &nodeData["transform"]);

    // Components（完整数据）
    nodeData["components"] = json::array();
    SerializeComponentsFull(node, &nodeData["components"]);
}

// ============================================================================
// Transform 序列化
// ============================================================================

void SceneSerializer::SerializeTransform(Transform* transform, void* jsonObject) {
    if (!transform || !jsonObject) return;

    json& transformData = *static_cast<json*>(jsonObject);

    // Position
    auto pos = transform->GetLocalPosition();
    transformData["position"] = {
        {"x", pos.x},
        {"y", pos.y},
        {"z", pos.z}
    };

    // Rotation (Quaternion)
    auto rot = transform->GetLocalRotation();
    transformData["rotation"] = {
        {"x", rot.x},
        {"y", rot.y},
        {"z", rot.z},
        {"w", rot.w}
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
// Components 序列化（基础版本）
// ============================================================================

void SceneSerializer::SerializeComponents(SceneNode* node, void* jsonArray) {
    if (!node || !jsonArray) return;

    json& components = *static_cast<json*>(jsonArray);

    // MeshRenderer
    if (auto* meshRenderer = node->GetComponent<MeshRenderer>()) {
        json comp;
        comp["type"] = "MeshRenderer";
        comp["enabled"] = meshRenderer->IsEnabled();
        comp["visible"] = meshRenderer->IsVisible();
        comp["hasMesh"] = (meshRenderer->GetMesh() != nullptr);
        components.push_back(comp);
    }

    // RigidBody
    if (auto* rigidBody = node->GetComponent<RigidBody>()) {
        json comp;
        comp["type"] = "RigidBody";
        comp["enabled"] = rigidBody->IsEnabled();
        comp["hasBody"] = rigidBody->HasBody();
        comp["mass"] = rigidBody->GetMass();
        
        const char* shapeTypeName = "Unknown";
        switch (rigidBody->GetShapeType()) {
            case PhysicsShapeType::Box:      shapeTypeName = "Box"; break;
            case PhysicsShapeType::Sphere:   shapeTypeName = "Sphere"; break;
            case PhysicsShapeType::Capsule:  shapeTypeName = "Capsule"; break;
            case PhysicsShapeType::Cylinder: shapeTypeName = "Cylinder"; break;
        }
        comp["shapeType"] = shapeTypeName;
        
        const Vector3& size = rigidBody->GetSize();
        comp["size"] = json::array({ size.x, size.y, size.z });
        
        if (rigidBody->HasBody()) {
            Vector3 linearVel = rigidBody->GetLinearVelocity();
            Vector3 angularVel = rigidBody->GetAngularVelocity();
            comp["linearVelocity"] = json::array({ linearVel.x, linearVel.y, linearVel.z });
            comp["angularVelocity"] = json::array({ angularVel.x, angularVel.y, angularVel.z });
        }

        components.push_back(comp);
    }

    // TODO: 添加其他组件类型
}

// ============================================================================
// Components 序列化（完整版本）
// ============================================================================

void SceneSerializer::SerializeComponentsFull(SceneNode* node, void* jsonArray) {
    if (!node || !jsonArray) return;

    json& components = *static_cast<json*>(jsonArray);

    // MeshRenderer（完整数据）
    if (auto* meshRenderer = node->GetComponent<MeshRenderer>()) {
        json comp;
        comp["type"] = "MeshRenderer";
        comp["enabled"] = meshRenderer->IsEnabled();
        comp["visible"] = meshRenderer->IsVisible();
        
        if (meshRenderer->GetMesh()) {
            comp["hasMesh"] = true;
            
            // 从节点名称推断 Mesh 类型
            std::string nodeName = node->GetName();
            std::string meshType = "unknown";
            
            if (nodeName.find("Cube") != std::string::npos) meshType = "cube";
            else if (nodeName.find("Sphere") != std::string::npos) meshType = "sphere";
            else if (nodeName.find("Cylinder") != std::string::npos) meshType = "cylinder";
            else if (nodeName.find("Plane") != std::string::npos) meshType = "plane";
            
            comp["meshType"] = meshType;
        } else {
            comp["hasMesh"] = false;
        }

        components.push_back(comp);
    }

    // RigidBody（完整数据）
    if (auto* rigidBody = node->GetComponent<RigidBody>()) {
        json comp;
        comp["type"] = "RigidBody";
        comp["enabled"] = rigidBody->IsEnabled();
        comp["hasBody"] = rigidBody->HasBody();
        comp["mass"] = rigidBody->GetMass();
        
        const char* shapeTypeName = "Unknown";
        switch (rigidBody->GetShapeType()) {
            case PhysicsShapeType::Box:      shapeTypeName = "Box"; break;
            case PhysicsShapeType::Sphere:   shapeTypeName = "Sphere"; break;
            case PhysicsShapeType::Capsule:  shapeTypeName = "Capsule"; break;
            case PhysicsShapeType::Cylinder: shapeTypeName = "Cylinder"; break;
        }
        comp["shapeType"] = shapeTypeName;
        
        const Vector3& size = rigidBody->GetSize();
        comp["size"] = json::array({ size.x, size.y, size.z });

        components.push_back(comp);
    }

    // TODO: 添加其他组件类型
}

// ============================================================================
// Components 反序列化
// ============================================================================

void SceneSerializer::DeserializeComponents(SceneNode* node, EngineCore* engine, void* jsonArray) {
    if (!node || !engine || !jsonArray) return;

    json& components = *static_cast<json*>(jsonArray);

    for (const auto& compData : components) {
        std::string type = compData["type"];

        if (type == "MeshRenderer") {
            MeshRenderer* renderer = node->AddComponent<MeshRenderer>();
            
            if (compData.contains("enabled")) {
                renderer->SetEnabled(compData["enabled"]);
            }
            
            if (compData.contains("visible")) {
                renderer->SetVisible(compData["visible"]);
            }
            
            if (compData.value("hasMesh", false)) {
                std::string meshType = compData.value("meshType", "unknown");
                
                // 🎯 根据 meshType 创建对应的 Mesh
                if (meshType == "cube") {
                    renderer->SetMesh(engine->GetMeshManager()->CreateCube(
                        1.0f, Vector3(1.0f, 0.5f, 0.2f)
                    ));
                }
                else if (meshType == "sphere") {
                    renderer->SetMesh(engine->GetMeshManager()->CreateSphere(
                        0.5f, 24, 16, Vector3(0.2f, 0.5f, 1.0f)
                    ));
                }
                else if (meshType == "cylinder") {
                    renderer->SetMesh(engine->GetMeshManager()->CreateCylinder(
                        0.5f, 0.5f, 1.0f, 24, Vector3(0.2f, 1.0f, 0.5f)
                    ));
                }
                else if (meshType == "plane") {
                    renderer->SetMesh(engine->GetMeshManager()->CreatePlane(
                        2.0f, 2.0f, 1, 1, Vector3(0.7f, 0.7f, 0.7f)
                    ));
                }
                
                MOON_LOG_INFO("SceneSerializer", "Restored MeshRenderer with meshType: %s", meshType.c_str());
            }
        }
        else if (type == "RigidBody") {
            RigidBody* rigidBody = node->AddComponent<RigidBody>();
            
            if (compData.contains("enabled")) {
                rigidBody->SetEnabled(compData["enabled"]);
            }
            
            if (compData.contains("mass")) {
                float mass = compData["mass"];
                
                PhysicsShapeType shapeType = PhysicsShapeType::Box;
                if (compData.contains("shapeType")) {
                    std::string shapeTypeName = compData["shapeType"];
                    if (shapeTypeName == "Sphere") shapeType = PhysicsShapeType::Sphere;
                    else if (shapeTypeName == "Capsule") shapeType = PhysicsShapeType::Capsule;
                    else if (shapeTypeName == "Cylinder") shapeType = PhysicsShapeType::Cylinder;
                }
                
                Vector3 size(1.0f, 1.0f, 1.0f);
                if (compData.contains("size") && compData["size"].is_array()) {
                    auto sizeArray = compData["size"];
                    if (sizeArray.size() >= 3) {
                        size.x = sizeArray[0];
                        size.y = sizeArray[1];
                        size.z = sizeArray[2];
                    }
                }
                
                if (g_PhysicsSystem) {
                    rigidBody->CreateBody(g_PhysicsSystem, shapeType, size, mass);
                    MOON_LOG_INFO("SceneSerializer", "Restored RigidBody with mass: %.2f", mass);
                } else {
                    MOON_LOG_ERROR("SceneSerializer", "PhysicsSystem is nullptr, cannot restore RigidBody");
                }
            }
        }
        
        // TODO: 添加其他组件类型的反序列化
    }
}

} // namespace Moon
