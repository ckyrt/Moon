#include "MoonEngineMessageHandler.h"
#include "../../app/SceneSerializer.h"
#include "../../../engine/core/EngineCore.h"
#include "../../../engine/core/Logging/Logger.h"
#include "../../../engine/core/Scene/MeshRenderer.h"
#include "../../../engine/core/Scene/Material.h"
#include "../../../engine/core/Scene/Light.h"
#include "../../../engine/core/Scene/Skybox.h"
#include "../../../engine/core/CSG/CSGComponent.h"
#include "../../../engine/massing/MassRuleParser.h"
#include "../../../engine/massing/MassMeshBuilder.h"
#include "../../../external/nlohmann/json.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <fstream>

using json = nlohmann::json;

// 澶栭儴鍑芥暟澹版槑锛堝畾涔夊湪 EditorApp.cpp 涓級
extern void SetSelectedObject(Moon::SceneNode* node);
extern Moon::SceneNode* GetSelectedObject();
extern void SetGizmoOperation(const std::string& mode);
extern void SetGizmoMode(const std::string& mode);  // 馃幆 World/Local 鍒囨崲

// ============================================================================
// JSON 鍝嶅簲杈呭姪鍑芥暟
// ============================================================================
namespace {
    // 鍒涘缓鎴愬姛鍝嶅簲
    std::string CreateSuccessResponse() {
        json result;
        result["success"] = true;
        return result.dump();
    }

    // 鍒涘缓閿欒鍝嶅簲
    std::string CreateErrorResponse(const std::string& errorMessage) {
        json error;
        error["error"] = errorMessage;
        return error.dump();
    }

    // 瑙ｆ瀽 Vector3 鍙傛暟
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

    // 涓鸿妭鐐规坊鍔犻粯璁?PBR 鏉愯川锛堟櫘閫氬嚑浣曚綋浣跨敤锛?
    void AddDefaultMaterial(Moon::SceneNode* node, const Moon::Vector3& baseColor = Moon::Vector3(1.0f, 1.0f, 1.0f)) {
        Moon::Material* material = node->AddComponent<Moon::Material>();
        material->SetMetallic(0.0f);
        material->SetRoughness(0.5f);
        material->SetBaseColor(baseColor);
        // 鉁?鏅€歮esh浣跨敤UV鏄犲皠锛堥粯璁ゅ€硷紝鏄惧紡璁剧疆鏇存竻鏅帮級
        material->SetMappingMode(Moon::MappingMode::UV);
    }

    // 涓?CSG 鑺傜偣娣诲姞榛樿鐏拌壊鏉愯川锛堚渽 CSG蹇呴』鐢═riplanar鏄犲皠锛?
    void AddCSGMaterial(Moon::SceneNode* node) {
        Moon::Material* material = node->AddComponent<Moon::Material>();
        material->SetMetallic(0.0f);
        material->SetRoughness(0.5f);
        material->SetBaseColor(Moon::Vector3(0.8f, 0.8f, 0.8f));
        // 鉁?CSG鍑犱綍浣撳繀椤讳娇鐢═riplanar鏄犲皠锛圲V鍧愭爣涓?,0锛?
        material->SetMappingMode(Moon::MappingMode::Triplanar);
        material->SetTriplanarTiling(0.5f);  // 姣?绫抽噸澶?
        material->SetTriplanarBlend(4.0f);   // 榛樿閿愬害
    }
    Moon::MaterialPreset ParseMaterialPreset(const std::string& materialName) {
        std::string lowerName = materialName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        if (lowerName == "concrete" || lowerName == "concrete_floor" || lowerName == "concrete_polished") {
            return Moon::MaterialPreset::Concrete;
        }
        if (lowerName == "fabric" || lowerName == "leather" || lowerName == "carpet") {
            return Moon::MaterialPreset::Fabric;
        }
        if (lowerName == "metal" || lowerName == "steel" || lowerName == "aluminum" ||
            lowerName == "chrome" || lowerName == "copper") {
            return Moon::MaterialPreset::Metal;
        }
        if (lowerName == "plastic" || lowerName == "rubber") {
            return Moon::MaterialPreset::Plastic;
        }
        if (lowerName == "rock" || lowerName == "stone" || lowerName == "brick" ||
            lowerName == "plaster" || lowerName == "tile_ceramic") {
            return Moon::MaterialPreset::Rock;
        }
        if (lowerName == "wood" || lowerName == "wood_floor" || lowerName == "wood_polished" ||
            lowerName == "wood_painted") {
            return Moon::MaterialPreset::Wood;
        }
        if (lowerName == "glass" || lowerName == "glass_frosted" || lowerName == "glass_tinted") {
            return Moon::MaterialPreset::Glass;
        }

        return Moon::MaterialPreset::None;
    }

    Moon::SceneNode* FindMassingPreviewRoot(Moon::Scene* scene) {
        return scene ? scene->FindNodeByName("__MassingPreview") : nullptr;
    }

    void ClearMassingPreviewNodes(Moon::Scene* scene) {
        Moon::SceneNode* previewRoot = FindMassingPreviewRoot(scene);
        if (previewRoot) {
            scene->DestroyNodeImmediate(previewRoot);
        }
    }

    void AddMassingMaterial(Moon::SceneNode* node, const std::string& materialName) {
        Moon::Material* material = node->AddComponent<Moon::Material>();
        material->SetMaterialPreset(ParseMaterialPreset(materialName));
        material->SetMappingMode(Moon::MappingMode::Triplanar);
        material->SetTriplanarTiling(0.5f);
        material->SetTriplanarBlend(4.0f);
    }

    struct Bounds3 {
        Moon::Vector3 min;
        Moon::Vector3 max;
        bool valid = false;
    };

    void ExpandBounds(Bounds3& bounds, const Moon::Vector3& point) {
        if (!bounds.valid) {
            bounds.min = point;
            bounds.max = point;
            bounds.valid = true;
            return;
        }

        bounds.min.x = std::min(bounds.min.x, point.x);
        bounds.min.y = std::min(bounds.min.y, point.y);
        bounds.min.z = std::min(bounds.min.z, point.z);
        bounds.max.x = std::max(bounds.max.x, point.x);
        bounds.max.y = std::max(bounds.max.y, point.y);
        bounds.max.z = std::max(bounds.max.z, point.z);
    }

    Bounds3 ComputePreviewBounds(const Moon::Massing::MassBuildResult& buildResult) {
        Bounds3 bounds;

        for (const Moon::Massing::MassBuildItem& item : buildResult.items) {
            if (!item.mesh) {
                continue;
            }

            for (const Moon::Vertex& vertex : item.mesh->GetVertices()) {
                ExpandBounds(bounds, vertex.position);
            }
        }

        return bounds;
    }

    void FrameCameraToBounds(MoonEngineMessageHandler* handler, const Bounds3& bounds) {
        if (!handler || !bounds.valid || !handler->GetEngineCore() || !handler->GetEngineCore()->GetCamera()) {
            return;
        }

        Moon::PerspectiveCamera* camera = handler->GetEngineCore()->GetCamera();
        const Moon::Vector3 center(
            (bounds.min.x + bounds.max.x) * 0.5f,
            (bounds.min.y + bounds.max.y) * 0.5f,
            (bounds.min.z + bounds.max.z) * 0.5f
        );

        const Moon::Vector3 extents(
            (bounds.max.x - bounds.min.x) * 0.5f,
            (bounds.max.y - bounds.min.y) * 0.5f,
            (bounds.max.z - bounds.min.z) * 0.5f
        );

        const float radius = std::max(1.0f, std::sqrt(
            extents.x * extents.x +
            extents.y * extents.y +
            extents.z * extents.z));

        const float verticalFovRadians = 60.0f * 3.1415926535f / 180.0f;
        const float distance = std::max(radius * 2.2f, radius / std::tan(verticalFovRadians * 0.5f));
        const Moon::Vector3 offset(radius * 0.55f, radius * 0.4f, -distance);

        camera->SetPosition(center + offset);
        camera->SetTarget(center);
    }
}

// ============================================================================
// 鍛戒护澶勭悊鍑芥暟绫诲瀷瀹氫箟
// ============================================================================
using CommandHandler = std::function<std::string(MoonEngineMessageHandler*, const json&, Moon::Scene*)>;

// ============================================================================
// 鍚勪釜鍛戒护鐨勫鐞嗗嚱鏁?
// ============================================================================
namespace CommandHandlers {
    // 鑾峰彇鍦烘櫙灞傜骇
    std::string HandleGetScene(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        return Moon::SceneSerializer::GetSceneHierarchy(scene);
    }

    // 鑾峰彇鑺傜偣璇︽儏
    std::string HandleGetNodeDetails(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        return Moon::SceneSerializer::GetNodeDetails(scene, nodeId);
    }

    // 閫変腑鑺傜偣
    std::string HandleSelectNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        MOON_LOG_INFO("MoonEngineMessage", "Selected node: %s (ID=%u)", 
                     node->GetName().c_str(), nodeId);
        
        // 鏇存柊鍏ㄥ眬閫変腑鐘舵€侊紙杩欐牱 Gizmo 浼氭樉绀哄湪杩欎釜鐗╀綋涓婏級
        SetSelectedObject(node);
        
        return CreateSuccessResponse();
    }

    // 璁剧疆浣嶇疆
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

    // 璁剧疆鏃嬭浆
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

    // 璁剧疆缂╂斁
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

    // 璁剧疆 Gizmo 鎿嶄綔妯″紡锛坱ranslate/rotate/scale锛?
    std::string HandleSetGizmoMode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string mode = req["mode"];
        
        SetGizmoOperation(mode);
        MOON_LOG_INFO("MoonEngineMessage", "Gizmo operation set to %s", mode.c_str());
        
        return CreateSuccessResponse();
    }

    // ========================================================================
    // Light 缁勪欢灞炴€ц缃?
    // ========================================================================

    // 璁剧疆 Light 棰滆壊
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

    // 璁剧疆 Light 寮哄害
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

    // 璁剧疆 Light 鑼冨洿锛圥oint/Spot锛?
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

    // 璁剧疆 Light 绫诲瀷
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
    // Skybox 缁勪欢灞炴€ц缃?
    // ========================================================================

    // 璁剧疆 Skybox 寮哄害
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

    // 璁剧疆 Skybox 鏃嬭浆
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

    // 璁剧疆 Skybox 鑹茶皟
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

    // 璁剧疆 Skybox IBL
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

    // 璁剧疆 Skybox 鐜璐村浘璺緞
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
    // Material 缁勪欢鍛戒护澶勭悊鍣?
    // ========================================================================

    // 璁剧疆 Material 閲戝睘搴?
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

    // 璁剧疆 Material 绮楃硻搴?
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

    // 璁剧疆 Material 鍩虹棰滆壊
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

    // 璁剧疆 Material 璐村浘
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

    // 璁剧疆 Material 棰勮
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

        // 灏嗗瓧绗︿覆杞崲涓烘灇涓?
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

    // 馃幆 璁剧疆 Gizmo 鍧愭爣绯绘ā寮忥紙world/local锛?
    std::string HandleSetGizmoCoordinateMode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string mode = req["mode"];
        
        SetGizmoMode(mode);
        MOON_LOG_INFO("MoonEngineMessage", "Gizmo coordinate mode set to %s", mode.c_str());
        
        return CreateSuccessResponse();
    }

    // 鍒涘缓鑺傜偣
    std::string HandleCreateNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string type = req["type"];
        
        MOON_LOG_INFO("MoonEngineMessage", "Creating node of type: %s", type.c_str());
        
        // 鑾峰彇寮曟搸鏍稿績
        EngineCore* engine = handler->GetEngineCore();
        if (!engine) {
            return CreateErrorResponse("Engine core not available");
        }
        
        // 鍒涘缓鍦烘櫙鑺傜偣
        Moon::SceneNode* newNode = nullptr;
        
        if (type == "empty") {
            newNode = scene->CreateNode("Empty Node");
        }
        // === 鏅€氬嚑浣曚綋锛堥€氳繃 MeshGenerator 鍒涘缓锛屾敮鎸乁V鍜屾潗璐級 ===
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
        // === CSG鍑犱綍浣擄紙鍩轰簬Manifold搴擄紝鏀寔甯冨皵杩愮畻锛屾殏鏃燯V鍧愭爣锛?===
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
            
            // 璁剧疆鍏夋簮榛樿鏂瑰悜锛氫粠鍙充笂鍓嶆柟鐓у皠锛堢被浼兼鍗堝お闃筹級
            // Rotation: (45掳, -30掳, 0掳) 琛ㄧず鍚戜笅 45掳 骞跺悜宸﹁浆 30掳
            newNode->GetTransform()->SetLocalRotation(Moon::Vector3(45.0f, -30.0f, 0.0f));
            
            // 娣诲姞 Light 缁勪欢
            Moon::Light* light = newNode->AddComponent<Moon::Light>();
            light->SetType(Moon::Light::Type::Directional);
            light->SetColor(Moon::Vector3(1.0f, 0.95f, 0.9f));  // 鏆栫櫧鑹诧紙澶槼鍏夛級
            light->SetIntensity(1.5f);  // 寮哄害
            
            MOON_LOG_INFO("MoonEngineMessage", "Created directional light (Intensity: %.1f)", 
                         light->GetIntensity());
        }
        else if (type == "skybox") {
            newNode = scene->CreateNode("Skybox");

            // 娣诲姞 Skybox 缁勪欢
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
        
        // 馃幆 鏀寔鐖惰妭鐐硅缃?
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

    // 鍒犻櫎鑺傜偣
    std::string HandleDeleteNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }
        
        MOON_LOG_INFO("MoonEngineMessage", "Deleting node: %s (ID=%u)", 
                     node->GetName().c_str(), nodeId);
        
        // 濡傛灉褰撳墠鍒犻櫎鐨勮妭鐐规槸閫変腑鐨勮妭鐐癸紝娓呴櫎閫夋嫨
        if (GetSelectedObject() == node) {
            SetSelectedObject(nullptr);
        }
        
        // 閿€姣佽妭鐐癸紙寤惰繜鍒犻櫎锛屽湪甯х粨鏉熸椂鍒犻櫎锛?
        scene->DestroyNode(node);
        
        return CreateSuccessResponse();
    }

    // 璁剧疆鑺傜偣鐖剁骇
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
            
            // 妫€鏌ュ惊鐜緷璧栵細涓嶈兘灏嗙埗鑺傜偣璁句负鑷繁鐨勫瓙瀛欒妭鐐?
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

    // 閲嶅懡鍚嶈妭鐐?
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

    // 璁剧疆鑺傜偣婵€娲荤姸鎬?
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
    // 馃幆 Undo/Redo 涓撶敤 API锛堝唴閮ㄤ娇鐢級
    // ========================================================================
    
    /**
     * 鑾峰彇鑺傜偣鐨勫畬鏁村簭鍒楀寲鏁版嵁锛堢敤浜?Delete Undo锛?
     * 
     * 璇锋眰鏍煎紡锛?
     * {
     *   "command": "serializeNode",
     *   "nodeId": 123
     * }
     * 
     * 鍝嶅簲鏍煎紡锛?
     * {
     *   "success": true,
     *   "data": "{ ... 瀹屾暣鐨勮妭鐐?JSON 鏁版嵁 ... }"
     * }
     * 
     * 鈿狅笍 鍐呴儴 API锛氫粎渚?WebUI Undo/Redo 绯荤粺浣跨敤
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
     * 浠庡簭鍒楀寲鏁版嵁閲嶅缓鑺傜偣锛堢敤浜?Delete Undo锛?
     * 
     * 璇锋眰鏍煎紡锛?
     * {
     *   "command": "deserializeNode",
     *   "data": "{ ... 瀹屾暣鐨勮妭鐐?JSON 鏁版嵁 ... }"
     * }
     * 
     * 鈿狅笍 鍐呴儴 API锛氫粎渚?WebUI Undo/Redo 绯荤粺浣跨敤
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
     * 鎵归噺璁剧疆 Transform锛坧osition + rotation + scale锛?
     * 鐢ㄤ簬 Undo/Redo 蹇€熸仮澶嶈妭鐐圭姸鎬?
     * 
     * 璇锋眰鏍煎紡锛?
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
     * 鈿狅笍 鍐呴儴 API锛氫粎渚?WebUI Undo/Redo 绯荤粺浣跨敤
     */
    std::string HandleSetNodeTransform(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }
        
        const json& transform = req["transform"];
        
        // 鎵归噺璁剧疆 Transform锛堥伩鍏嶅娆?API 璋冪敤锛?
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
     * 鍒涘缓鑺傜偣骞舵寚瀹?ID锛堢敤浜?Undo 鎭㈠琚垹闄ょ殑鑺傜偣锛?
     * 
     * 璇锋眰鏍煎紡锛?
     * {
     *   "command": "createNodeWithId",
     *   "nodeId": 123,  // 鍘熷鑺傜偣 ID
     *   "name": "MyNode",
     *   "type": "empty",  // "empty", "cube", "sphere", etc.
     *   "parentId": 456,  // 鍙€?
     *   "transform": {    // 鍙€?
     *     "position": {...},
     *     "rotation": {...},
     *     "scale": {...}
     *   }
     * }
     * 
     * 鈿狅笍 鍐呴儴 API锛氫粎渚?WebUI Undo/Redo 绯荤粺浣跨敤
     * 鈿狅笍 娉ㄦ剰锛氬鏋滄寚瀹氱殑 ID 宸插瓨鍦紝浼氳繑鍥為敊璇?
     */
    std::string HandleCreateNodeWithId(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t targetId = req["nodeId"];
        std::string name = req["name"];
        std::string type = req.contains("type") ? req["type"].get<std::string>() : "empty";
        
        // 馃毃 妫€鏌?ID 鏄惁宸插瓨鍦?
        if (scene->FindNodeByID(targetId)) {
            return CreateErrorResponse("Node with ID already exists: " + std::to_string(targetId));
        }
        
        MOON_LOG_INFO("MoonEngineMessage", "[Undo] Creating node with ID=%u, name=%s, type=%s", 
                     targetId, name.c_str(), type.c_str());
        
        // 鑾峰彇寮曟搸鏍稿績
        EngineCore* engine = handler->GetEngineCore();
        if (!engine) {
            return CreateErrorResponse("Engine core not available");
        }
        
        // 鍒涘缓鍦烘櫙鑺傜偣锛堜娇鐢ㄦ寚瀹?ID锛?
        Moon::SceneNode* newNode = scene->CreateNodeWithID(targetId, name);
        
        if (!newNode) {
            return CreateErrorResponse("Failed to create node with specified ID");
        }
        
        // 鏍规嵁绫诲瀷娣诲姞缁勪欢
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
        
        // 璁剧疆鐖惰妭鐐?
        if (req.contains("parentId") && !req["parentId"].is_null()) {
            uint32_t parentId = req["parentId"];
            Moon::SceneNode* parent = scene->FindNodeByID(parentId);
            if (parent) {
                newNode->SetParent(parent);
            } else {
                MOON_LOG_WARN("MoonEngineMessage", "Parent node %u not found", parentId);
            }
        }
        
        // 璁剧疆 Transform
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

    // 鍐欐棩蹇楀埌鏂囦欢
    std::string HandlePreviewMassing(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        if (!req.contains("ruleJson")) {
            return CreateErrorResponse("Missing 'ruleJson' field");
        }

        Moon::Massing::RuleSet ruleSet;
        std::string parseError;
        if (!Moon::Massing::MassRuleParser::ParseFromString(req["ruleJson"].get<std::string>(), ruleSet, parseError)) {
            return CreateErrorResponse("Failed to parse massing rules: " + parseError);
        }

        Moon::Massing::MassBuildResult buildResult;
        std::string buildError;
        if (!Moon::Massing::MassMeshBuilder::Build(ruleSet, buildResult, buildError)) {
            return CreateErrorResponse("Failed to build massing preview: " + buildError);
        }

        ClearMassingPreviewNodes(scene);

        Moon::SceneNode* previewRoot = scene->CreateNode("__MassingPreview");

        for (size_t i = 0; i < buildResult.items.size(); ++i) {
            const Moon::Massing::MassBuildItem& item = buildResult.items[i];
            const std::string childName = item.name.empty() ? ("MassingPart_" + std::to_string(i)) : item.name;

            Moon::SceneNode* childNode = scene->CreateNode(childName);
            childNode->SetParent(previewRoot, false);

            Moon::MeshRenderer* renderer = childNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(item.mesh);
            AddMassingMaterial(childNode, item.material);
        }

        FrameCameraToBounds(handler, ComputePreviewBounds(buildResult));

        json response;
        response["success"] = true;
        response["rootNodeId"] = previewRoot->GetID();
        response["meshCount"] = buildResult.items.size();
        response["warnings"] = buildResult.warnings;
        return response.dump();
    }

    std::string HandleClearMassingPreview(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        ClearMassingPreviewNodes(scene);
        return CreateSuccessResponse();
    }

    std::string HandleWriteLog(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        if (!req.contains("logContent")) {
            return CreateErrorResponse("Missing 'logContent' field");
        }

        std::string logContent = req["logContent"];
        
        // 鑾峰彇鏃ュ織鐩綍璺緞
        std::string logDir = "E:\\game_engine\\Moon\\bin\\x64\\Debug\\logs";
        std::string logFilePath = logDir + "\\frontend.log";
        
        try {
            // 鎵撳紑鏂囦欢锛堣拷鍔犳ā寮忥級
            std::ofstream logFile(logFilePath, std::ios::app);
            if (!logFile.is_open()) {
                return CreateErrorResponse("Failed to open log file: " + logFilePath);
            }
            
            // 鍐欏叆鏃ュ織鍐呭
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
// 鍛戒护鏄犲皠琛紙闈欐€佸垵濮嬪寲锛?
// ============================================================================
static const std::unordered_map<std::string, CommandHandler> s_commandHandlers = {
    {"getScene",                 CommandHandlers::HandleGetScene},
    {"getNodeDetails",           CommandHandlers::HandleGetNodeDetails},
    {"selectNode",               CommandHandlers::HandleSelectNode},
    {"setPosition",              CommandHandlers::HandleSetPosition},
    {"setRotation",              CommandHandlers::HandleSetRotation},
    {"setScale",                 CommandHandlers::HandleSetScale},
    {"setGizmoMode",             CommandHandlers::HandleSetGizmoMode},
    {"setGizmoCoordinateMode",   CommandHandlers::HandleSetGizmoCoordinateMode},
    {"setLightColor",            CommandHandlers::HandleSetLightColor},
    {"setLightIntensity",        CommandHandlers::HandleSetLightIntensity},
    {"setLightRange",            CommandHandlers::HandleSetLightRange},
    {"setLightType",             CommandHandlers::HandleSetLightType},
    {"setSkyboxIntensity",       CommandHandlers::HandleSetSkyboxIntensity},
    {"setSkyboxRotation",        CommandHandlers::HandleSetSkyboxRotation},
    {"setSkyboxTint",            CommandHandlers::HandleSetSkyboxTint},
    {"setSkyboxIBL",             CommandHandlers::HandleSetSkyboxIBL},
    {"setSkyboxEnvironmentMap",  CommandHandlers::HandleSetSkyboxEnvironmentMap},
    {"setMaterialMetallic",      CommandHandlers::HandleSetMaterialMetallic},
    {"setMaterialRoughness",     CommandHandlers::HandleSetMaterialRoughness},
    {"setMaterialBaseColor",     CommandHandlers::HandleSetMaterialBaseColor},
    {"setMaterialTexture",       CommandHandlers::HandleSetMaterialTexture},
    {"setMaterialPreset",        CommandHandlers::HandleSetMaterialPreset},
    {"createNode",               CommandHandlers::HandleCreateNode},
    {"deleteNode",               CommandHandlers::HandleDeleteNode},
    {"setNodeParent",            CommandHandlers::HandleSetNodeParent},
    {"renameNode",               CommandHandlers::HandleRenameNode},
    {"setNodeActive",            CommandHandlers::HandleSetNodeActive},
    {"serializeNode",            CommandHandlers::HandleSerializeNode},
    {"deserializeNode",          CommandHandlers::HandleDeserializeNode},
    {"setNodeTransform",         CommandHandlers::HandleSetNodeTransform},
    {"createNodeWithId",         CommandHandlers::HandleCreateNodeWithId},
    {"previewMassing",           CommandHandlers::HandlePreviewMassing},
    {"clearMassingPreview",      CommandHandlers::HandleClearMassingPreview},
    {"writeLog",                 CommandHandlers::HandleWriteLog}
};

MoonEngineMessageHandler::MoonEngineMessageHandler()
    : m_engine(nullptr) {
}

void MoonEngineMessageHandler::SetEngineCore(EngineCore* engine) {
    m_engine = engine;
}

// ============================================================================
// 澶勭悊鏉ヨ嚜 JavaScript 鐨勬煡璇?
// ============================================================================
bool MoonEngineMessageHandler::OnQuery(CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        int64_t query_id,
                                        const CefString& request,
                                        bool persistent,
                                        CefRefPtr<Callback> callback) {
    
    std::string requestStr = request.ToString();
    
    MOON_LOG_INFO("MoonEngineMessage", "OnQuery called with request: %s", requestStr.c_str());
    
    // 澶勭悊璇锋眰骞惰幏鍙栧搷搴?
    std::string response = ProcessRequest(requestStr);
    
    MOON_LOG_INFO("MoonEngineMessage", "Response: %s", response.c_str());
    
    // 杩斿洖鍝嶅簲缁?JavaScript
    callback->Success(response);
    
    return true;
}

void MoonEngineMessageHandler::OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                                                CefRefPtr<CefFrame> frame,
                                                int64_t query_id) {
    // 鏌ヨ琚彇娑堬紝鏃犻渶澶勭悊
}

// ============================================================================
// 澶勭悊鍏蜂綋璇锋眰锛堥噸鏋勫悗锛氫娇鐢ㄥ懡浠ゆ槧灏勮〃锛?
// ============================================================================
std::string MoonEngineMessageHandler::ProcessRequest(const std::string& request) {
    if (!m_engine) {
        return CreateErrorResponse("Engine not initialized");
    }

    try {
        // 瑙ｆ瀽 JSON 璇锋眰
        json req = json::parse(request);
        
        if (!req.contains("command")) {
            return CreateErrorResponse("Missing 'command' field");
        }

        std::string command = req["command"];
        Moon::Scene* scene = m_engine->GetScene();

        // 鏌ユ壘鍛戒护澶勭悊鍣?
        auto it = s_commandHandlers.find(command);
        if (it != s_commandHandlers.end()) {
            // 璋冪敤瀵瑰簲鐨勫鐞嗗嚱鏁?
            return it->second(this, req, scene);
        }

        // 鏈煡鍛戒护
        return CreateErrorResponse("Unknown command: " + command);
    }
    catch (const json::exception& e) {
        return CreateErrorResponse(std::string("JSON parse error: ") + e.what());
    }
    catch (const std::exception& e) {
        return CreateErrorResponse(std::string("Exception: ") + e.what());
    }
}
