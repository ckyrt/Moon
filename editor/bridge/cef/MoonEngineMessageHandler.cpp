#include "MoonEngineMessageHandler.h"
#include "../../app/SceneSerializer.h"
#include "../../../engine/core/EngineCore.h"
#include "../../../engine/core/Logging/Logger.h"
#include "../../../engine/core/Scene/MeshRenderer.h"
#include "../../../engine/core/Scene/Material.h"
#include "../../../engine/core/Scene/Light.h"
#include "../../../engine/core/Scene/Skybox.h"
#include "../../../engine/core/CSG/CSGComponent.h"
#include "../../../external/nlohmann/json.hpp"
#include <functional>
#include <unordered_map>
#include <fstream>

using json = nlohmann::json;

// 外部函数声明（定义在 EditorApp.cpp 中）
extern void SetSelectedObject(Moon::SceneNode* node);
extern Moon::SceneNode* GetSelectedObject();
extern void SetGizmoOperation(const std::string& mode);
extern void SetGizmoMode(const std::string& mode);  // 🎯 World/Local 切换

// ============================================================================
// JSON 响应辅助函数
// ============================================================================
namespace {
    // 创建成功响应
    std::string CreateSuccessResponse() {
        json result;
        result["success"] = true;
        return result.dump();
    }

    // 创建错误响应
    std::string CreateErrorResponse(const std::string& errorMessage) {
        json error;
        error["error"] = errorMessage;
        return error.dump();
    }

    // 解析 Vector3 参数
    Moon::Vector3 ParseVector3(const json& obj) {
        return {
            obj["x"].get<float>(),
            obj["y"].get<float>(),
            obj["z"].get<float>()
        };
    }

    Moon::Quaternion ParseQuaternion(const json& obj) {
        return {
            obj["x"].get<float>(),
            obj["y"].get<float>(),
            obj["z"].get<float>(),
            obj["w"].get<float>()
        };
    }

    // 为节点添加默认 PBR 材质（普通几何体使用）
    void AddDefaultMaterial(Moon::SceneNode* node, const Moon::Vector3& baseColor = Moon::Vector3(1.0f, 1.0f, 1.0f)) {
        Moon::Material* material = node->AddComponent<Moon::Material>();
        material->SetMetallic(0.0f);
        material->SetRoughness(0.5f);
        material->SetBaseColor(baseColor);
    }

    // 为 CSG 节点添加默认灰色材质
    void AddCSGMaterial(Moon::SceneNode* node) {
        AddDefaultMaterial(node, Moon::Vector3(0.8f, 0.8f, 0.8f));
    }
}

// ============================================================================
// 命令处理函数类型定义
// ============================================================================
using CommandHandler = std::function<std::string(MoonEngineMessageHandler*, const json&, Moon::Scene*)>;

// ============================================================================
// 各个命令的处理函数
// ============================================================================
namespace CommandHandlers {
    // 获取场景层级
    std::string HandleGetScene(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        return Moon::SceneSerializer::GetSceneHierarchy(scene);
    }

    // 获取节点详情
    std::string HandleGetNodeDetails(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        return Moon::SceneSerializer::GetNodeDetails(scene, nodeId);
    }

    // 选中节点
    std::string HandleSelectNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        MOON_LOG_INFO("MoonEngineMessage", "Selected node: %s (ID=%u)", 
                     node->GetName().c_str(), nodeId);
        
        // 更新全局选中状态（这样 Gizmo 会显示在这个物体上）
        SetSelectedObject(node);
        
        return CreateSuccessResponse();
    }

    // 设置位置
    std::string HandleSetPosition(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        Moon::Vector3 position = ParseVector3(req["position"]);

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        node->GetTransform()->SetLocalPosition(position);
        MOON_LOG_INFO("MoonEngineMessage", "Set position of node %u to (%.2f, %.2f, %.2f)",
                     nodeId, position.x, position.y, position.z);
        
        return CreateSuccessResponse();
    }

    // 设置旋转
    std::string HandleSetRotation(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        Moon::Quaternion rotation = ParseQuaternion(req["rotation"]);

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        node->GetTransform()->SetLocalRotation(rotation);
        MOON_LOG_INFO("MoonEngineMessage", "Set rotation of node %u to quaternion (%.2f, %.2f, %.2f, %.2f)",
                     nodeId, rotation.x, rotation.y, rotation.z, rotation.w);
        
        return CreateSuccessResponse();
    }

    // 设置缩放
    std::string HandleSetScale(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        Moon::Vector3 scale = ParseVector3(req["scale"]);

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        node->GetTransform()->SetLocalScale(scale);
        MOON_LOG_INFO("MoonEngineMessage", "Set scale of node %u to (%.2f, %.2f, %.2f)",
                     nodeId, scale.x, scale.y, scale.z);
        
        return CreateSuccessResponse();
    }

    // 设置 Gizmo 操作模式（translate/rotate/scale）
    std::string HandleSetGizmoMode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string mode = req["mode"];
        
        SetGizmoOperation(mode);
        MOON_LOG_INFO("MoonEngineMessage", "Gizmo operation set to %s", mode.c_str());
        
        return CreateSuccessResponse();
    }

    // ========================================================================
    // Light 组件属性设置
    // ========================================================================

    // 设置 Light 颜色
    std::string HandleSetLightColor(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        Moon::Vector3 color = ParseVector3(req["color"]);

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Light* light = node->GetComponent<Moon::Light>();
        if (!light) {
            return CreateErrorResponse("Node does not have Light component");
        }

        light->SetColor(color);
        MOON_LOG_INFO("MoonEngineMessage", "Set light color of node %u to (%.2f, %.2f, %.2f)",
                     nodeId, color.x, color.y, color.z);
        
        return CreateSuccessResponse();
    }

    // 设置 Light 强度
    std::string HandleSetLightIntensity(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        float intensity = req["intensity"];

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Light* light = node->GetComponent<Moon::Light>();
        if (!light) {
            return CreateErrorResponse("Node does not have Light component");
        }

        light->SetIntensity(intensity);
        MOON_LOG_INFO("MoonEngineMessage", "Set light intensity of node %u to %.2f", nodeId, intensity);
        
        return CreateSuccessResponse();
    }

    // 设置 Light 范围（Point/Spot）
    std::string HandleSetLightRange(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        float range = req["range"];

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Light* light = node->GetComponent<Moon::Light>();
        if (!light) {
            return CreateErrorResponse("Node does not have Light component");
        }

        light->SetRange(range);
        MOON_LOG_INFO("MoonEngineMessage", "Set light range of node %u to %.2f", nodeId, range);
        
        return CreateSuccessResponse();
    }

    // 设置 Light 类型
    std::string HandleSetLightType(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        std::string lightType = req["lightType"];

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Light* light = node->GetComponent<Moon::Light>();
        if (!light) {
            return CreateErrorResponse("Node does not have Light component");
        }

        Moon::Light::Type type;
        if (lightType == "Directional") {
            type = Moon::Light::Type::Directional;
        } else if (lightType == "Point") {
            type = Moon::Light::Type::Point;
        } else if (lightType == "Spot") {
            type = Moon::Light::Type::Spot;
        } else {
            return CreateErrorResponse("Unknown light type: " + lightType);
        }

        light->SetType(type);
        MOON_LOG_INFO("MoonEngineMessage", "Set light type of node %u to %s", nodeId, lightType.c_str());
        
        return CreateSuccessResponse();
    }

    // ========================================================================
    // Skybox 组件属性设置
    // ========================================================================

    // 设置 Skybox 强度
    std::string HandleSetSkyboxIntensity(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        float intensity = req["intensity"];

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Skybox* skybox = node->GetComponent<Moon::Skybox>();
        if (!skybox) {
            return CreateErrorResponse("Node does not have Skybox component");
        }

        skybox->SetIntensity(intensity);
        MOON_LOG_INFO("MoonEngineMessage", "Set skybox intensity of node %u to %.2f", nodeId, intensity);
        
        return CreateSuccessResponse();
    }

    // 设置 Skybox 旋转
    std::string HandleSetSkyboxRotation(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        float rotation = req["rotation"];

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Skybox* skybox = node->GetComponent<Moon::Skybox>();
        if (!skybox) {
            return CreateErrorResponse("Node does not have Skybox component");
        }

        skybox->SetRotation(rotation);
        MOON_LOG_INFO("MoonEngineMessage", "Set skybox rotation of node %u to %.2f", nodeId, rotation);
        
        return CreateSuccessResponse();
    }

    // 设置 Skybox 色调
    std::string HandleSetSkyboxTint(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        Moon::Vector3 tint = ParseVector3(req["tint"]);

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Skybox* skybox = node->GetComponent<Moon::Skybox>();
        if (!skybox) {
            return CreateErrorResponse("Node does not have Skybox component");
        }

        skybox->SetTint(tint);
        MOON_LOG_INFO("MoonEngineMessage", "Set skybox tint of node %u to (%.2f, %.2f, %.2f)",
                     nodeId, tint.x, tint.y, tint.z);
        
        return CreateSuccessResponse();
    }

    // 设置 Skybox IBL
    std::string HandleSetSkyboxIBL(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        bool enableIBL = req["enableIBL"];

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Skybox* skybox = node->GetComponent<Moon::Skybox>();
        if (!skybox) {
            return CreateErrorResponse("Node does not have Skybox component");
        }

        skybox->SetEnableIBL(enableIBL);
        MOON_LOG_INFO("MoonEngineMessage", "Set skybox IBL of node %u to %s", 
                     nodeId, enableIBL ? "enabled" : "disabled");
        
        return CreateSuccessResponse();
    }

    // 设置 Skybox 环境贴图路径
    std::string HandleSetSkyboxEnvironmentMap(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        std::string path = req["path"];

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Skybox* skybox = node->GetComponent<Moon::Skybox>();
        if (!skybox) {
            return CreateErrorResponse("Node does not have Skybox component");
        }

        bool success = skybox->LoadEnvironmentMap(path);
        if (!success) {
            MOON_LOG_WARN("MoonEngineMessage", "Failed to load environment map: %s", path.c_str());
            return CreateErrorResponse("Failed to load environment map: " + path);
        }

        MOON_LOG_INFO("MoonEngineMessage", "Set skybox environment map of node %u to %s", 
                     nodeId, path.c_str());
        
        return CreateSuccessResponse();
    }

    // ========================================================================
    // Material 组件命令处理器
    // ========================================================================

    // 设置 Material 金属度
    std::string HandleSetMaterialMetallic(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        float metallic = req["metallic"];

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Material* material = node->GetComponent<Moon::Material>();
        if (!material) {
            return CreateErrorResponse("Node does not have Material component");
        }

        material->SetMetallic(metallic);
        MOON_LOG_INFO("MoonEngineMessage", "Set material metallic of node %u to %.2f", nodeId, metallic);
        
        return CreateSuccessResponse();
    }

    // 设置 Material 粗糙度
    std::string HandleSetMaterialRoughness(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        float roughness = req["roughness"];

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Material* material = node->GetComponent<Moon::Material>();
        if (!material) {
            return CreateErrorResponse("Node does not have Material component");
        }

        material->SetRoughness(roughness);
        MOON_LOG_INFO("MoonEngineMessage", "Set material roughness of node %u to %.2f", nodeId, roughness);
        
        return CreateSuccessResponse();
    }

    // 设置 Material 基础颜色
    std::string HandleSetMaterialBaseColor(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        Moon::Vector3 baseColor = ParseVector3(req["baseColor"]);

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Material* material = node->GetComponent<Moon::Material>();
        if (!material) {
            return CreateErrorResponse("Node does not have Material component");
        }

        material->SetBaseColor(baseColor);
        MOON_LOG_INFO("MoonEngineMessage", "Set material base color of node %u to (%.2f, %.2f, %.2f)",
                     nodeId, baseColor.x, baseColor.y, baseColor.z);
        
        return CreateSuccessResponse();
    }

    // 设置 Material 贴图
    std::string HandleSetMaterialTexture(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        std::string textureType = req["textureType"];  // "albedo", "normal", "ao", "roughness", "metalness"
        std::string path = req["path"];

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Material* material = node->GetComponent<Moon::Material>();
        if (!material) {
            return CreateErrorResponse("Node does not have Material component");
        }

        if (textureType == "albedo") {
            material->SetAlbedoMap(path);
        } else if (textureType == "normal") {
            material->SetNormalMap(path);
        } else if (textureType == "ao") {
            material->SetAOMap(path);
        } else if (textureType == "roughness") {
            material->SetRoughnessMap(path);
        } else if (textureType == "metalness") {
            material->SetMetalnessMap(path);
        } else {
            return CreateErrorResponse("Unknown texture type: " + textureType);
        }

        MOON_LOG_INFO("MoonEngineMessage", "Set material %s texture of node %u to %s",
                     textureType.c_str(), nodeId, path.c_str());
        
        return CreateSuccessResponse();
    }

    // 设置 Material 预设
    std::string HandleSetMaterialPreset(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        std::string presetName = req["preset"];

        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        Moon::Material* material = node->GetComponent<Moon::Material>();
        if (!material) {
            return CreateErrorResponse("Node does not have Material component");
        }

        // 将字符串转换为枚举
        Moon::MaterialPreset preset = Moon::MaterialPreset::None;
        if (presetName == "Concrete") {
            preset = Moon::MaterialPreset::Concrete;
        } else if (presetName == "Fabric") {
            preset = Moon::MaterialPreset::Fabric;
        } else if (presetName == "Metal") {
            preset = Moon::MaterialPreset::Metal;
        } else if (presetName == "Plastic") {
            preset = Moon::MaterialPreset::Plastic;
        } else if (presetName == "Rock") {
            preset = Moon::MaterialPreset::Rock;
        } else if (presetName == "Wood") {
            preset = Moon::MaterialPreset::Wood;
        } else if (presetName == "Glass") {
            preset = Moon::MaterialPreset::Glass;
        } else if (presetName == "PolishedMetal") {
            preset = Moon::MaterialPreset::PolishedMetal;
        } else if (presetName == "None") {
            preset = Moon::MaterialPreset::None;
        } else {
            return CreateErrorResponse("Unknown material preset: " + presetName);
        }

        material->SetMaterialPreset(preset);
        MOON_LOG_INFO("MoonEngineMessage", "Set material preset of node %u to %s", nodeId, presetName.c_str());
        
        return CreateSuccessResponse();
    }

    // ========================================================================

    // 🎯 设置 Gizmo 坐标系模式（world/local）
    std::string HandleSetGizmoCoordinateMode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string mode = req["mode"];
        
        SetGizmoMode(mode);
        MOON_LOG_INFO("MoonEngineMessage", "Gizmo coordinate mode set to %s", mode.c_str());
        
        return CreateSuccessResponse();
    }

    // 创建节点
    std::string HandleCreateNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string type = req["type"];
        
        MOON_LOG_INFO("MoonEngineMessage", "Creating node of type: %s", type.c_str());
        
        // 获取引擎核心
        EngineCore* engine = handler->GetEngineCore();
        if (!engine) {
            return CreateErrorResponse("Engine core not available");
        }
        
        // 创建场景节点
        Moon::SceneNode* newNode = nullptr;
        
        if (type == "empty") {
            newNode = scene->CreateNode("Empty Node");
        }
        // === 普通几何体（通过 MeshGenerator 创建，支持UV和材质） ===
        else if (type == "cube") {
            newNode = scene->CreateNode("Cube");
            Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(engine->GetMeshManager()->CreateCube(
                1.0f,  // size
                Moon::Vector3(1.0f, 0.5f, 0.2f)  // vertex color (orange)
            ));
            AddDefaultMaterial(newNode);
        }
        else if (type == "sphere") {
            newNode = scene->CreateNode("Sphere");
            Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(engine->GetMeshManager()->CreateSphere(
                0.5f,  // radius
                24,    // segments
                16,    // rings
                Moon::Vector3(0.2f, 0.5f, 1.0f)  // vertex color (blue)
            ));
            AddDefaultMaterial(newNode);
        }
        else if (type == "cylinder") {
            newNode = scene->CreateNode("Cylinder");
            Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(engine->GetMeshManager()->CreateCylinder(
                0.5f,  // radiusTop
                0.5f,  // radiusBottom
                1.0f,  // height
                24,    // segments
                Moon::Vector3(0.2f, 1.0f, 0.5f)  // vertex color (green)
            ));
            AddDefaultMaterial(newNode);
        }
        // === CSG几何体（基于Manifold库，支持布尔运算，暂无UV坐标） ===
        else if (type == "box" || type == "csg_box") {
            newNode = scene->CreateNode("CSG_Box");
            Moon::CSGComponent* csgComp = newNode->AddComponent<Moon::CSGComponent>();
            csgComp->SetCSGTree(Moon::CSGNode::CreateBox(1.0f, 1.0f, 1.0f));
            
            Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(csgComp->GetMesh());
            AddCSGMaterial(newNode);
            
            MOON_LOG_INFO("MoonEngineMessage", "Created CSG Box (1x1x1)");
        }
        else if (type == "csg_sphere") {
            newNode = scene->CreateNode("CSG_Sphere");
            Moon::CSGComponent* csgComp = newNode->AddComponent<Moon::CSGComponent>();
            csgComp->SetCSGTree(Moon::CSGNode::CreateSphere(0.5f, 32));
            
            Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(csgComp->GetMesh());
            AddCSGMaterial(newNode);
            
            MOON_LOG_INFO("MoonEngineMessage", "Created CSG Sphere (r=0.5, segments=32)");
        }
        else if (type == "csg_cylinder") {
            newNode = scene->CreateNode("CSG_Cylinder");
            Moon::CSGComponent* csgComp = newNode->AddComponent<Moon::CSGComponent>();
            csgComp->SetCSGTree(Moon::CSGNode::CreateCylinder(0.5f, 1.0f, 32));
            
            Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(csgComp->GetMesh());
            AddCSGMaterial(newNode);
            
            MOON_LOG_INFO("MoonEngineMessage", "Created CSG Cylinder (r=0.5, h=1.0, segments=32)");
        }
        else if (type == "cone" || type == "csg_cone") {
            newNode = scene->CreateNode("CSG_Cone");
            Moon::CSGComponent* csgComp = newNode->AddComponent<Moon::CSGComponent>();
            csgComp->SetCSGTree(Moon::CSGNode::CreateCone(0.5f, 1.0f, 32));
            
            Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(csgComp->GetMesh());
            AddCSGMaterial(newNode);
            
            MOON_LOG_INFO("MoonEngineMessage", "Created CSG Cone (r=0.5, h=1.0, segments=32)");
        }
        else if (type == "plane") {
            newNode = scene->CreateNode("Plane");
            Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(engine->GetMeshManager()->CreatePlane(
                2.0f,  // width
                2.0f,  // depth
                1,     // subdivisionsX
                1,     // subdivisionsZ
                Moon::Vector3(0.7f, 0.7f, 0.7f)  // vertex color (gray)
            ));
            AddDefaultMaterial(newNode);
        }
        else if (type == "light") {
            newNode = scene->CreateNode("Directional Light");
            
            // 设置光源默认方向：从右上前方照射（类似正午太阳）
            // Rotation: (45°, -30°, 0°) 表示向下 45° 并向左转 30°
            newNode->GetTransform()->SetLocalRotation(Moon::Vector3(45.0f, -30.0f, 0.0f));
            
            // 添加 Light 组件
            Moon::Light* light = newNode->AddComponent<Moon::Light>();
            light->SetType(Moon::Light::Type::Directional);
            light->SetColor(Moon::Vector3(1.0f, 0.95f, 0.9f));  // 暖白色（太阳光）
            light->SetIntensity(1.5f);  // 强度
            
            MOON_LOG_INFO("MoonEngineMessage", "Created directional light (Intensity: %.1f)", 
                         light->GetIntensity());
        }
        else if (type == "skybox") {
            newNode = scene->CreateNode("Skybox");

            // 添加 Skybox 组件
            Moon::Skybox* skybox = newNode->AddComponent<Moon::Skybox>();
            skybox->SetType(Moon::Skybox::Type::EquirectangularHDR);
            skybox->LoadEnvironmentMap("assets/textures/environment.hdr");
            skybox->SetIntensity(1.0f);
            skybox->SetRotation(0.0f);
            skybox->SetTint(Moon::Vector3(1.0f, 1.0f, 1.0f));  // No tint by default
            skybox->SetEnableIBL(true);  // Enable IBL by default

            MOON_LOG_INFO("MoonEngineMessage", "Created skybox (Intensity: %.1f, IBL: %s)", 
                         skybox->GetIntensity(), skybox->IsIBLEnabled() ? "enabled" : "disabled");
        }
        else {
            return CreateErrorResponse("Unknown node type: " + type);
        }

        if (!newNode) {
            return CreateErrorResponse("Failed to create node");
        }
        
        // 🎯 支持父节点设置
        if (req.contains("parentId") && !req["parentId"].is_null()) {
            uint32_t parentId = req["parentId"];
            Moon::SceneNode* parent = scene->FindNodeByID(parentId);
            if (parent) {
                newNode->SetParent(parent);
                MOON_LOG_INFO("MoonEngineMessage", "Set parent of node %u to %u", 
                             newNode->GetID(), parentId);
            } else {
                MOON_LOG_WARN("MoonEngineMessage", "Parent node %u not found", parentId);
            }
        }
        
        MOON_LOG_INFO("MoonEngineMessage", "Created node: %s (ID=%u)", 
                     newNode->GetName().c_str(), newNode->GetID());
        
        return CreateSuccessResponse();
    }

    // 删除节点
    std::string HandleDeleteNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }
        
        MOON_LOG_INFO("MoonEngineMessage", "Deleting node: %s (ID=%u)", 
                     node->GetName().c_str(), nodeId);
        
        // 如果当前删除的节点是选中的节点，清除选择
        if (GetSelectedObject() == node) {
            SetSelectedObject(nullptr);
        }
        
        // 销毁节点（延迟删除，在帧结束时删除）
        scene->DestroyNode(node);
        
        return CreateSuccessResponse();
    }

    // 设置节点父级
    std::string HandleSetNodeParent(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }
        
        Moon::SceneNode* newParent = nullptr;
        if (req.contains("parentId") && !req["parentId"].is_null()) {
            uint32_t parentId = req["parentId"];
            newParent = scene->FindNodeByID(parentId);
            if (!newParent) {
                return CreateErrorResponse("Parent node not found");
            }
            
            // 检查循环依赖：不能将父节点设为自己的子孙节点
            Moon::SceneNode* checkNode = newParent;
            while (checkNode) {
                if (checkNode == node) {
                    return CreateErrorResponse("Cannot set parent to descendant node");
                }
                checkNode = checkNode->GetParent();
            }
        }
        
        node->SetParent(newParent);
        
        MOON_LOG_INFO("MoonEngineMessage", "Set parent of node %u to %s", 
                     nodeId, newParent ? std::to_string(newParent->GetID()).c_str() : "null");
        
        return CreateSuccessResponse();
    }

    // 重命名节点
    std::string HandleRenameNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        std::string newName = req["newName"];
        
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }
        
        node->SetName(newName);
        
        MOON_LOG_INFO("MoonEngineMessage", "Renamed node %u to \"%s\"", nodeId, newName.c_str());
        
        return CreateSuccessResponse();
    }

    // 设置节点激活状态
    std::string HandleSetNodeActive(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        bool active = req["active"];
        
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }
        
        node->SetActive(active);
        
        MOON_LOG_INFO("MoonEngineMessage", "Set node %u active = %s", nodeId, active ? "true" : "false");
        
        return CreateSuccessResponse();
    }

    // ========================================================================
    // 🎯 Undo/Redo 专用 API（内部使用）
    // ========================================================================
    
    /**
     * 获取节点的完整序列化数据（用于 Delete Undo）
     * 
     * 请求格式：
     * {
     *   "command": "serializeNode",
     *   "nodeId": 123
     * }
     * 
     * 响应格式：
     * {
     *   "success": true,
     *   "data": "{ ... 完整的节点 JSON 数据 ... }"
     * }
     * 
     * ⚠️ 内部 API：仅供 WebUI Undo/Redo 系统使用
     */
    std::string HandleSerializeNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        
        std::string serializedData = Moon::SceneSerializer::SerializeNode(scene, nodeId);
        
        json result;
        result["success"] = true;
        result["data"] = serializedData;
        
        return result.dump();
    }

    /**
     * 从序列化数据重建节点（用于 Delete Undo）
     * 
     * 请求格式：
     * {
     *   "command": "deserializeNode",
     *   "data": "{ ... 完整的节点 JSON 数据 ... }"
     * }
     * 
     * ⚠️ 内部 API：仅供 WebUI Undo/Redo 系统使用
     */
    std::string HandleDeserializeNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string serializedData = req["data"];
        
        EngineCore* engine = handler->GetEngineCore();
        if (!engine) {
            return CreateErrorResponse("Engine core not available");
        }
        
        Moon::SceneNode* node = Moon::SceneSerializer::DeserializeNode(scene, engine, serializedData);
        
        if (!node) {
            return CreateErrorResponse("Failed to deserialize node");
        }
        
        MOON_LOG_INFO("MoonEngineMessage", "[Undo] Successfully deserialized node %u", node->GetID());
        
        return CreateSuccessResponse();
    }

    /**
     * 批量设置 Transform（position + rotation + scale）
     * 用于 Undo/Redo 快速恢复节点状态
     * 
     * 请求格式：
     * {
     *   "command": "setNodeTransform",
     *   "nodeId": 123,
     *   "transform": {
     *     "position": {"x": 1.0, "y": 2.0, "z": 3.0},
     *     "rotation": {"x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0},
     *     "scale": {"x": 1.0, "y": 1.0, "z": 1.0}
     *   }
     * }
     * 
     * ⚠️ 内部 API：仅供 WebUI Undo/Redo 系统使用
     */
    std::string HandleSetNodeTransform(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }
        
        const json& transform = req["transform"];
        
        // 批量设置 Transform（避免多次 API 调用）
        if (transform.contains("position")) {
            node->GetTransform()->SetLocalPosition(ParseVector3(transform["position"]));
        }
        if (transform.contains("rotation")) {
            node->GetTransform()->SetLocalRotation(ParseQuaternion(transform["rotation"]));
        }
        if (transform.contains("scale")) {
            node->GetTransform()->SetLocalScale(ParseVector3(transform["scale"]));
        }
        
        MOON_LOG_INFO("MoonEngineMessage", "[Undo] Restored transform for node %u", nodeId);
        
        return CreateSuccessResponse();
    }

    /**
     * 创建节点并指定 ID（用于 Undo 恢复被删除的节点）
     * 
     * 请求格式：
     * {
     *   "command": "createNodeWithId",
     *   "nodeId": 123,  // 原始节点 ID
     *   "name": "MyNode",
     *   "type": "empty",  // "empty", "cube", "sphere", etc.
     *   "parentId": 456,  // 可选
     *   "transform": {    // 可选
     *     "position": {...},
     *     "rotation": {...},
     *     "scale": {...}
     *   }
     * }
     * 
     * ⚠️ 内部 API：仅供 WebUI Undo/Redo 系统使用
     * ⚠️ 注意：如果指定的 ID 已存在，会返回错误
     */
    std::string HandleCreateNodeWithId(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t targetId = req["nodeId"];
        std::string name = req["name"];
        std::string type = req.contains("type") ? req["type"].get<std::string>() : "empty";
        
        // 🚨 检查 ID 是否已存在
        if (scene->FindNodeByID(targetId)) {
            return CreateErrorResponse("Node with ID already exists: " + std::to_string(targetId));
        }
        
        MOON_LOG_INFO("MoonEngineMessage", "[Undo] Creating node with ID=%u, name=%s, type=%s", 
                     targetId, name.c_str(), type.c_str());
        
        // 获取引擎核心
        EngineCore* engine = handler->GetEngineCore();
        if (!engine) {
            return CreateErrorResponse("Engine core not available");
        }
        
        // 创建场景节点（使用指定 ID）
        Moon::SceneNode* newNode = scene->CreateNodeWithID(targetId, name);
        
        if (!newNode) {
            return CreateErrorResponse("Failed to create node with specified ID");
        }
        
        // 根据类型添加组件
        if (type == "cube") {
            Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(engine->GetMeshManager()->CreateCube(
                1.0f, Moon::Vector3(1.0f, 0.5f, 0.2f)
            ));
        }
        else if (type == "sphere") {
            Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(engine->GetMeshManager()->CreateSphere(
                0.5f, 24, 16, Moon::Vector3(0.2f, 0.5f, 1.0f)
            ));
        }
        else if (type == "cylinder") {
            Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(engine->GetMeshManager()->CreateCylinder(
                0.5f, 0.5f, 1.0f, 24, Moon::Vector3(0.2f, 1.0f, 0.5f)
            ));
        }
        else if (type == "plane") {
            Moon::MeshRenderer* renderer = newNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(engine->GetMeshManager()->CreatePlane(
                2.0f, 2.0f, 1, 1, Moon::Vector3(0.7f, 0.7f, 0.7f)
            ));
        }
        
        // 设置父节点
        if (req.contains("parentId") && !req["parentId"].is_null()) {
            uint32_t parentId = req["parentId"];
            Moon::SceneNode* parent = scene->FindNodeByID(parentId);
            if (parent) {
                newNode->SetParent(parent);
            } else {
                MOON_LOG_WARN("MoonEngineMessage", "Parent node %u not found", parentId);
            }
        }
        
        // 设置 Transform
        if (req.contains("transform")) {
            const json& transform = req["transform"];
            if (transform.contains("position")) {
                newNode->GetTransform()->SetLocalPosition(ParseVector3(transform["position"]));
            }
            if (transform.contains("rotation")) {
                newNode->GetTransform()->SetLocalRotation(ParseQuaternion(transform["rotation"]));
            }
            if (transform.contains("scale")) {
                newNode->GetTransform()->SetLocalScale(ParseVector3(transform["scale"]));
            }
        }
        
        MOON_LOG_INFO("MoonEngineMessage", "[Undo] Successfully created node with ID=%u", targetId);
        
        return CreateSuccessResponse();
    }

    // 写日志到文件
    std::string HandleWriteLog(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        if (!req.contains("logContent")) {
            return CreateErrorResponse("Missing 'logContent' field");
        }

        std::string logContent = req["logContent"];
        
        // 获取日志目录路径
        std::string logDir = "E:\\game_engine\\Moon\\bin\\x64\\Debug\\logs";
        std::string logFilePath = logDir + "\\frontend.log";
        
        try {
            // 打开文件（追加模式）
            std::ofstream logFile(logFilePath, std::ios::app);
            if (!logFile.is_open()) {
                return CreateErrorResponse("Failed to open log file: " + logFilePath);
            }
            
            // 写入日志内容
            logFile << logContent;
            logFile.close();
            
            return CreateSuccessResponse();
        }
        catch (const std::exception& e) {
            return CreateErrorResponse(std::string("Exception writing log: ") + e.what());
        }
    }
}

// ============================================================================
// 命令映射表（静态初始化）
// ============================================================================
static const std::unordered_map<std::string, CommandHandler> s_commandHandlers = {
    {"getScene",                 CommandHandlers::HandleGetScene},
    {"getNodeDetails",           CommandHandlers::HandleGetNodeDetails},
    {"selectNode",               CommandHandlers::HandleSelectNode},
    {"setPosition",              CommandHandlers::HandleSetPosition},
    {"setRotation",              CommandHandlers::HandleSetRotation},
    {"setScale",                 CommandHandlers::HandleSetScale},
    {"setGizmoMode",             CommandHandlers::HandleSetGizmoMode},
    {"setGizmoCoordinateMode",   CommandHandlers::HandleSetGizmoCoordinateMode},  // 🎯 World/Local 切换
    {"setLightColor",            CommandHandlers::HandleSetLightColor},  // 🎯 Light: 设置颜色
    {"setLightIntensity",        CommandHandlers::HandleSetLightIntensity},  // 🎯 Light: 设置强度
    {"setLightRange",            CommandHandlers::HandleSetLightRange},  // 🎯 Light: 设置范围
    {"setLightType",             CommandHandlers::HandleSetLightType},  // 🎯 Light: 设置类型
    {"setSkyboxIntensity",       CommandHandlers::HandleSetSkyboxIntensity},  // 🎯 Skybox: 设置强度
    {"setSkyboxRotation",        CommandHandlers::HandleSetSkyboxRotation},  // 🎯 Skybox: 设置旋转
    {"setSkyboxTint",            CommandHandlers::HandleSetSkyboxTint},  // 🎯 Skybox: 设置色调
    {"setSkyboxIBL",             CommandHandlers::HandleSetSkyboxIBL},  // 🎯 Skybox: 设置IBL
    {"setSkyboxEnvironmentMap",  CommandHandlers::HandleSetSkyboxEnvironmentMap},  // 🎯 Skybox: 设置环境贴图
    {"setMaterialMetallic",      CommandHandlers::HandleSetMaterialMetallic},  // 🎯 Material: 设置金属度
    {"setMaterialRoughness",     CommandHandlers::HandleSetMaterialRoughness},  // 🎯 Material: 设置粗糙度
    {"setMaterialBaseColor",     CommandHandlers::HandleSetMaterialBaseColor},  // 🎯 Material: 设置基础颜色
    {"setMaterialTexture",       CommandHandlers::HandleSetMaterialTexture},  // 🎯 Material: 设置贴图
    {"setMaterialPreset",        CommandHandlers::HandleSetMaterialPreset},  // 🎯 Material: 设置材质预设
    {"createNode",               CommandHandlers::HandleCreateNode},
    {"deleteNode",               CommandHandlers::HandleDeleteNode},  // 🎯 删除节点
    {"setNodeParent",            CommandHandlers::HandleSetNodeParent},  // 🎯 设置父节点
    {"renameNode",               CommandHandlers::HandleRenameNode},  // 🎯 重命名节点
    {"setNodeActive",            CommandHandlers::HandleSetNodeActive},  // 🎯 设置节点激活状态
    {"serializeNode",            CommandHandlers::HandleSerializeNode},  // 🎯 Undo: 序列化节点
    {"deserializeNode",          CommandHandlers::HandleDeserializeNode},  // 🎯 Undo: 反序列化节点
    {"setNodeTransform",         CommandHandlers::HandleSetNodeTransform},  // 🎯 Undo: 批量设置 Transform
    {"createNodeWithId",         CommandHandlers::HandleCreateNodeWithId},  // 🎯 Undo: 创建指定 ID 的节点（已弃用，用 deserializeNode 替代）
    {"writeLog",                 CommandHandlers::HandleWriteLog}  // 🎯 Logger: 写日志到文件
};

MoonEngineMessageHandler::MoonEngineMessageHandler()
    : m_engine(nullptr) {
}

void MoonEngineMessageHandler::SetEngineCore(EngineCore* engine) {
    m_engine = engine;
}

// ============================================================================
// 处理来自 JavaScript 的查询
// ============================================================================
bool MoonEngineMessageHandler::OnQuery(CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        int64_t query_id,
                                        const CefString& request,
                                        bool persistent,
                                        CefRefPtr<Callback> callback) {
    
    std::string requestStr = request.ToString();
    
    MOON_LOG_INFO("MoonEngineMessage", "OnQuery called with request: %s", requestStr.c_str());
    
    // 处理请求并获取响应
    std::string response = ProcessRequest(requestStr);
    
    MOON_LOG_INFO("MoonEngineMessage", "Response: %s", response.c_str());
    
    // 返回响应给 JavaScript
    callback->Success(response);
    
    return true;
}

void MoonEngineMessageHandler::OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                                                CefRefPtr<CefFrame> frame,
                                                int64_t query_id) {
    // 查询被取消，无需处理
}

// ============================================================================
// 处理具体请求（重构后：使用命令映射表）
// ============================================================================
std::string MoonEngineMessageHandler::ProcessRequest(const std::string& request) {
    if (!m_engine) {
        return CreateErrorResponse("Engine not initialized");
    }

    try {
        // 解析 JSON 请求
        json req = json::parse(request);
        
        if (!req.contains("command")) {
            return CreateErrorResponse("Missing 'command' field");
        }

        std::string command = req["command"];
        Moon::Scene* scene = m_engine->GetScene();

        // 查找命令处理器
        auto it = s_commandHandlers.find(command);
        if (it != s_commandHandlers.end()) {
            // 调用对应的处理函数
            return it->second(this, req, scene);
        }

        // 未知命令
        return CreateErrorResponse("Unknown command: " + command);
    }
    catch (const json::exception& e) {
        return CreateErrorResponse(std::string("JSON parse error: ") + e.what());
    }
    catch (const std::exception& e) {
        return CreateErrorResponse(std::string("Exception: ") + e.what());
    }
}
