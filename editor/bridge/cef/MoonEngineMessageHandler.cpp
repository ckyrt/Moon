#include "MoonEngineMessageHandler.h"
#include "../../app/SceneSerializer.h"
#include "../../../engine/core/EngineCore.h"
#include "../../../engine/core/Logging/Logger.h"
#include "../../../engine/core/Assets/AssetPaths.h"
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
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <fstream>

using json = nlohmann::json;

// 婢舵牠鍎撮崙鑺ユ殶婢圭増妲戦敍鍫濈暰娑斿婀?EditorApp.cpp 娑擃叏绱?
extern void SetSelectedObject(Moon::SceneNode* node);
extern Moon::SceneNode* GetSelectedObject();
extern void SetGizmoOperation(const std::string& mode);
extern void SetGizmoMode(const std::string& mode);  // 棣冨箚 World/Local 閸掑洦宕?

// ============================================================================
// JSON 閸濆秴绨叉潏鍛И閸戣姤鏆?
// ============================================================================
namespace {
    // 閸掓稑缂撻幋鎰閸濆秴绨?
    std::string CreateSuccessResponse() {
        json result;
        result["success"] = true;
        return result.dump();
    }

    // 閸掓稑缂撻柨娆掝嚖閸濆秴绨?
    std::string CreateErrorResponse(const std::string& errorMessage) {
        json error;
        error["error"] = errorMessage;
        return error.dump();
    }

    // 鐟欙絾鐎?Vector3 閸欏倹鏆?
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

    // 娑撻缚濡悙瑙勫潑閸旂娀绮拋?PBR 閺夋劘宸濋敍鍫熸珮闁艾鍤戞担鏇氱秼娴ｈ法鏁ら敍?
    void AddDefaultMaterial(Moon::SceneNode* node, const Moon::Vector3& baseColor = Moon::Vector3(1.0f, 1.0f, 1.0f)) {
        Moon::Material* material = node->AddComponent<Moon::Material>();
        material->SetMetallic(0.0f);
        material->SetRoughness(0.5f);
        material->SetBaseColor(baseColor);
        // 閴?閺咁噣鈧esh娴ｈ法鏁V閺勭姴鐨犻敍鍫ョ帛鐠併倕鈧》绱濋弰鎯х础鐠佸墽鐤嗛弴瀛樼閺呭府绱?
        material->SetMappingMode(Moon::MappingMode::UV);
    }

    // 娑?CSG 閼哄倻鍋ｅǎ璇插姒涙顓婚悘鎷屽閺夋劘宸濋敍鍫氭附 CSG韫囧懘銆忛悽鈺恟iplanar閺勭姴鐨犻敍?
    void AddCSGMaterial(Moon::SceneNode* node) {
        Moon::Material* material = node->AddComponent<Moon::Material>();
        material->SetMetallic(0.0f);
        material->SetRoughness(0.5f);
        material->SetBaseColor(Moon::Vector3(0.8f, 0.8f, 0.8f));
        // 閴?CSG閸戠姳缍嶆担鎾崇箑妞よ濞囬悽鈺恟iplanar閺勭姴鐨犻敍鍦睼閸ф劖鐖ｆ稉?,0閿?
        material->SetMappingMode(Moon::MappingMode::Triplanar);
        material->SetTriplanarTiling(0.5f);  // 濮?缁娊鍣告径?
        material->SetTriplanarBlend(4.0f);   // 姒涙顓婚柨鎰
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

    std::vector<Moon::SceneNode*> FindMassingPreviewRoots(Moon::Scene* scene) {
        std::vector<Moon::SceneNode*> previewRoots;
        if (!scene) {
            return previewRoots;
        }

        scene->Traverse([&previewRoots](Moon::SceneNode* node) {
            if (node && node->GetName() == "__MassingPreview") {
                previewRoots.push_back(node);
            }
        });
        return previewRoots;
    }

    void ClearMassingPreviewNodes(Moon::Scene* scene) {
        const std::vector<Moon::SceneNode*> previewRoots = FindMassingPreviewRoots(scene);
        for (Moon::SceneNode* previewRoot : previewRoots) {
            scene->DestroyNodeImmediate(previewRoot);
        }
    }

    void AddMassingMaterial(Moon::SceneNode* node, const std::string& materialName) {
        Moon::Material* material = node->AddComponent<Moon::Material>();
        (void)materialName;
        // Clay preview keeps validation focused on shape instead of noisy PBR presets.
        material->SetBaseColor(Moon::Vector3(0.72f, 0.74f, 0.78f));
        material->SetMetallic(0.0f);
        material->SetRoughness(0.92f);
        material->SetOpacity(1.0f);
        material->SetMappingMode(Moon::MappingMode::UV);
    }

    std::filesystem::path GetMassingPresetDirectory() {
        return std::filesystem::path(Moon::Assets::BuildAssetPath("massing"));
    }

    std::string ReadTextFile(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::in | std::ios::binary);
        if (!input.is_open()) {
            return std::string();
        }
        std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (contents.size() >= 3 &&
            static_cast<unsigned char>(contents[0]) == 0xEF &&
            static_cast<unsigned char>(contents[1]) == 0xBB &&
            static_cast<unsigned char>(contents[2]) == 0xBF) {
            contents.erase(0, 3);
        }
        return contents;
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

    std::string MakePresetId(const std::filesystem::path& path) {
        std::string id = path.stem().string();
        std::replace(id.begin(), id.end(), ' ', '_');
        return id;
    }
}

// ============================================================================
// 閸涙垝鎶ゆ径鍕倞閸戣姤鏆熺猾璇茬€风€规矮绠?
// ============================================================================
using CommandHandler = std::function<std::string(MoonEngineMessageHandler*, const json&, Moon::Scene*)>;

// ============================================================================
// 閸氬嫪閲滈崨鎴掓姢閻ㄥ嫬顦╅悶鍡楀毐閺?
// ============================================================================
namespace CommandHandlers {
    // 閼惧嘲褰囬崷鐑樻珯鐏炲倻楠?
    std::string HandleGetScene(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        return Moon::SceneSerializer::GetSceneHierarchy(scene);
    }

    // 閼惧嘲褰囬懞鍌滃仯鐠囷附鍎?
    std::string HandleGetNodeDetails(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        return Moon::SceneSerializer::GetNodeDetails(scene, nodeId);
    }

    // 闁鑵戦懞鍌滃仯
    std::string HandleSelectNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        MOON_LOG_INFO("MoonEngineMessage", "Selected node: %s (ID=%u)", 
                     node->GetName().c_str(), nodeId);
        
        // 閺囧瓨鏌婇崗銊ョ湰闁鑵戦悩鑸碘偓渚婄礄鏉╂瑦鐗?Gizmo 娴兼碍妯夌粈鍝勬躬鏉╂瑤閲滈悧鈺€缍嬫稉濠忕礆
        SetSelectedObject(node);
        
        return CreateSuccessResponse();
    }

    // 鐠佸墽鐤嗘担宥囩枂
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

    // 鐠佸墽鐤嗛弮瀣祮
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

    // 鐠佸墽鐤嗙紓鈺傛杹
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

    // 鐠佸墽鐤?Gizmo 閹垮秳缍斿Ο鈥崇础閿涘澅ranslate/rotate/scale閿?
    std::string HandleSetGizmoMode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string mode = req["mode"];
        
        SetGizmoOperation(mode);
        MOON_LOG_INFO("MoonEngineMessage", "Gizmo operation set to %s", mode.c_str());
        
        return CreateSuccessResponse();
    }

    // ========================================================================
    // Light 缂佸嫪娆㈢仦鐐粹偓褑顔曠純?
    // ========================================================================

    // 鐠佸墽鐤?Light 妫版粏澹?
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

    // 鐠佸墽鐤?Light 瀵搫瀹?
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

    // 鐠佸墽鐤?Light 閼煎啫娲块敍鍦int/Spot閿?
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

    // 鐠佸墽鐤?Light 缁鐎?
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
    // Skybox 缂佸嫪娆㈢仦鐐粹偓褑顔曠純?
    // ========================================================================

    // 鐠佸墽鐤?Skybox 瀵搫瀹?
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

    // 鐠佸墽鐤?Skybox 閺冨娴?
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

    // 鐠佸墽鐤?Skybox 閼硅尪鐨?
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

    // 鐠佸墽鐤?Skybox IBL
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

    // 鐠佸墽鐤?Skybox 閻滎垰顣ㄧ拹鏉戞禈鐠侯垰绶?
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
    // Material 缂佸嫪娆㈤崨鎴掓姢婢跺嫮鎮婇崳?
    // ========================================================================

    // 鐠佸墽鐤?Material 闁叉垵鐫樻惔?
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

    // 鐠佸墽鐤?Material 缁纭绘惔?
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

    // 鐠佸墽鐤?Material 閸╄櫣顢呮０婊嗗
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

    // 鐠佸墽鐤?Material 鐠愭潙娴?
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

    // 鐠佸墽鐤?Material 妫板嫯顔?
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

        // 鐏忓棗鐡х粭锔胯鏉烆剚宕叉稉鐑樼亣娑?
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

    // 棣冨箚 鐠佸墽鐤?Gizmo 閸ф劖鐖ｇ化缁樐佸蹇ョ礄world/local閿?
    std::string HandleSetGizmoCoordinateMode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string mode = req["mode"];
        
        SetGizmoMode(mode);
        MOON_LOG_INFO("MoonEngineMessage", "Gizmo coordinate mode set to %s", mode.c_str());
        
        return CreateSuccessResponse();
    }

    // 閸掓稑缂撻懞鍌滃仯
    std::string HandleCreateNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string type = req["type"];
        
        MOON_LOG_INFO("MoonEngineMessage", "Creating node of type: %s", type.c_str());
        
        // 閼惧嘲褰囧鏇熸惛閺嶇绺?
        EngineCore* engine = handler->GetEngineCore();
        if (!engine) {
            return CreateErrorResponse("Engine core not available");
        }
        
        // 閸掓稑缂撻崷鐑樻珯閼哄倻鍋?
        Moon::SceneNode* newNode = nullptr;
        
        if (type == "empty") {
            newNode = scene->CreateNode("Empty Node");
        }
        // === 閺咁噣鈧艾鍤戞担鏇氱秼閿涘牓鈧俺绻?MeshGenerator 閸掓稑缂撻敍灞炬暜閹镐箒V閸滃本娼楃拹顭掔礆 ===
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
        // === CSG閸戠姳缍嶆担鎿勭礄閸╄桨绨琈anifold鎼存搫绱濋弨顖涘瘮鐢啫鐨垫潻鎰暬閿涘本娈忛弮鐕疺閸ф劖鐖ｉ敍?===
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
            
            // 鐠佸墽鐤嗛崗澶嬬爱姒涙顓婚弬鐟版倻閿涙矮绮犻崣鍏呯瑐閸撳秵鏌熼悡褍鐨犻敍鍫㈣娴煎吋顒滈崡鍫濄亰闂冪绱?
            // Rotation: (45鎺? -30鎺? 0鎺? 鐞涖劎銇氶崥鎴滅瑓 45鎺?楠炶泛鎮滃锕佹祮 30鎺?
            newNode->GetTransform()->SetLocalRotation(Moon::Vector3(45.0f, -30.0f, 0.0f));
            
            // 濞ｈ濮?Light 缂佸嫪娆?
            Moon::Light* light = newNode->AddComponent<Moon::Light>();
            light->SetType(Moon::Light::Type::Directional);
            light->SetColor(Moon::Vector3(1.0f, 0.95f, 0.9f));  // 閺嗘牜娅ч懝璇х礄婢额亪妲奸崗澶涚礆
            light->SetIntensity(1.5f);  // 瀵搫瀹?
            
            MOON_LOG_INFO("MoonEngineMessage", "Created directional light (Intensity: %.1f)", 
                         light->GetIntensity());
        }
        else if (type == "skybox") {
            newNode = scene->CreateNode("Skybox");

            // 濞ｈ濮?Skybox 缂佸嫪娆?
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
        
        // 棣冨箚 閺€顖涘瘮閻栨儼濡悙纭咁啎缂?
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

    // 閸掔娀娅庨懞鍌滃仯
    std::string HandleDeleteNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }
        
        MOON_LOG_INFO("MoonEngineMessage", "Deleting node: %s (ID=%u)", 
                     node->GetName().c_str(), nodeId);
        
        // 婵″倹鐏夎ぐ鎾冲閸掔娀娅庨惃鍕Ν閻愯妲搁柅澶夎厬閻ㄥ嫯濡悙鐧哥礉濞撳懘娅庨柅澶嬪
        if (GetSelectedObject() == node) {
            SetSelectedObject(nullptr);
        }
        
        // 闁库偓濮ｄ浇濡悙鐧哥礄瀵ゆ儼绻滈崚鐘绘珟閿涘苯婀敮褏绮ㄩ弶鐔告閸掔娀娅庨敍?
        scene->DestroyNode(node);
        
        return CreateSuccessResponse();
    }

    // 鐠佸墽鐤嗛懞鍌滃仯閻栧墎楠?
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
            
            // 濡偓閺屻儱鎯婇悳顖欑贩鐠ф牭绱版稉宥堝厴鐏忓棛鍩楅懞鍌滃仯鐠佸彞璐熼懛顏勭箒閻ㄥ嫬鐡欑€涙瑨濡悙?
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

    // 闁插秴鎳￠崥宥堝Ν閻?
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

    // 鐠佸墽鐤嗛懞鍌滃仯濠碘偓濞茶崵濮搁幀?
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
    // 棣冨箚 Undo/Redo 娑撴挾鏁?API閿涘牆鍞撮柈銊ゅ▏閻㈩煉绱?
    // ========================================================================
    
    /**
     * 閼惧嘲褰囬懞鍌滃仯閻ㄥ嫬鐣弫鏉戠碍閸掓瀵查弫鐗堝祦閿涘牏鏁ゆ禍?Delete Undo閿?
     * 
     * 鐠囬攱鐪伴弽鐓庣础閿?
     * {
     *   "command": "serializeNode",
     *   "nodeId": 123
     * }
     * 
     * 閸濆秴绨查弽鐓庣础閿?
     * {
     *   "success": true,
     *   "data": "{ ... 鐎瑰本鏆ｉ惃鍕Ν閻?JSON 閺佺増宓?... }"
     * }
     * 
     * 閳跨媴绗?閸愬懘鍎?API閿涙矮绮庢笟?WebUI Undo/Redo 缁崵绮烘担璺ㄦ暏
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
     * 娴犲骸绨崚妤€瀵查弫鐗堝祦闁插秴缂撻懞鍌滃仯閿涘牏鏁ゆ禍?Delete Undo閿?
     * 
     * 鐠囬攱鐪伴弽鐓庣础閿?
     * {
     *   "command": "deserializeNode",
     *   "data": "{ ... 鐎瑰本鏆ｉ惃鍕Ν閻?JSON 閺佺増宓?... }"
     * }
     * 
     * 閳跨媴绗?閸愬懘鍎?API閿涙矮绮庢笟?WebUI Undo/Redo 缁崵绮烘担璺ㄦ暏
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
     * 閹靛綊鍣虹拋鍓х枂 Transform閿涘潷osition + rotation + scale閿?
     * 閻劋绨?Undo/Redo 韫囶偊鈧喐浠径宥堝Ν閻愬湱濮搁幀?
     * 
     * 鐠囬攱鐪伴弽鐓庣础閿?
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
     * 閳跨媴绗?閸愬懘鍎?API閿涙矮绮庢笟?WebUI Undo/Redo 缁崵绮烘担璺ㄦ暏
     */
    std::string HandleSetNodeTransform(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }
        
        const json& transform = req["transform"];
        
        // 閹靛綊鍣虹拋鍓х枂 Transform閿涘牓浼╅崗宥咁樋濞?API 鐠嬪啰鏁ら敍?
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
     * 閸掓稑缂撻懞鍌滃仯楠炶埖瀵氱€?ID閿涘牏鏁ゆ禍?Undo 閹垹顦茬悮顐㈠灩闂勩倗娈戦懞鍌滃仯閿?
     * 
     * 鐠囬攱鐪伴弽鐓庣础閿?
     * {
     *   "command": "createNodeWithId",
     *   "nodeId": 123,  // 閸樼喎顫愰懞鍌滃仯 ID
     *   "name": "MyNode",
     *   "type": "empty",  // "empty", "cube", "sphere", etc.
     *   "parentId": 456,  // 閸欘垶鈧?
     *   "transform": {    // 閸欘垶鈧?
     *     "position": {...},
     *     "rotation": {...},
     *     "scale": {...}
     *   }
     * }
     * 
     * 閳跨媴绗?閸愬懘鍎?API閿涙矮绮庢笟?WebUI Undo/Redo 缁崵绮烘担璺ㄦ暏
     * 閳跨媴绗?濞夈劍鍓伴敍姘洤閺嬫粍瀵氱€规氨娈?ID 瀹告彃鐡ㄩ崷顭掔礉娴兼俺绻戦崶鐐烘晩鐠?
     */
    std::string HandleCreateNodeWithId(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t targetId = req["nodeId"];
        std::string name = req["name"];
        std::string type = req.contains("type") ? req["type"].get<std::string>() : "empty";
        
        // 棣冩瘍 濡偓閺?ID 閺勵垰鎯佸鎻掔摠閸?
        if (scene->FindNodeByID(targetId)) {
            return CreateErrorResponse("Node with ID already exists: " + std::to_string(targetId));
        }
        
        MOON_LOG_INFO("MoonEngineMessage", "[Undo] Creating node with ID=%u, name=%s, type=%s", 
                     targetId, name.c_str(), type.c_str());
        
        // 閼惧嘲褰囧鏇熸惛閺嶇绺?
        EngineCore* engine = handler->GetEngineCore();
        if (!engine) {
            return CreateErrorResponse("Engine core not available");
        }
        
        // 閸掓稑缂撻崷鐑樻珯閼哄倻鍋ｉ敍鍫滃▏閻劍瀵氱€?ID閿?
        Moon::SceneNode* newNode = scene->CreateNodeWithID(targetId, name);
        
        if (!newNode) {
            return CreateErrorResponse("Failed to create node with specified ID");
        }
        
        // 閺嶈宓佺猾璇茬€峰ǎ璇插缂佸嫪娆?
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
        
        // 鐠佸墽鐤嗛悥鎯板Ν閻?
        if (req.contains("parentId") && !req["parentId"].is_null()) {
            uint32_t parentId = req["parentId"];
            Moon::SceneNode* parent = scene->FindNodeByID(parentId);
            if (parent) {
                newNode->SetParent(parent);
            } else {
                MOON_LOG_WARN("MoonEngineMessage", "Parent node %u not found", parentId);
            }
        }
        
        // 鐠佸墽鐤?Transform
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

    // 閸愭瑦妫╄箛妤€鍩岄弬鍥︽
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

        const bool focusCamera = req.value("focusCamera", false);
        if (focusCamera) {
            FrameCameraToBounds(handler, ComputePreviewBounds(buildResult));
        }

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

    std::string HandleListMassingPresets(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        (void)handler;
        (void)req;
        (void)scene;

        const std::filesystem::path presetDirectory = GetMassingPresetDirectory();
        if (!std::filesystem::exists(presetDirectory)) {
            return CreateErrorResponse("Massing preset directory not found: " + presetDirectory.string());
        }

        std::vector<std::filesystem::path> presetPaths;
        for (const auto& entry : std::filesystem::directory_iterator(presetDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                presetPaths.push_back(entry.path());
            }
        }

        std::sort(presetPaths.begin(), presetPaths.end());

        json response;
        response["success"] = true;
        response["presets"] = json::array();

        for (const std::filesystem::path& path : presetPaths) {
            json preset;
            preset["id"] = MakePresetId(path);
            preset["name"] = path.stem().string();
            preset["file"] = path.filename().string();
            response["presets"].push_back(preset);
        }

        return response.dump();
    }

    std::string HandleLoadMassingPreset(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        (void)handler;
        (void)scene;

        if (!req.contains("presetFile")) {
            return CreateErrorResponse("Missing 'presetFile' field");
        }

        const std::filesystem::path presetPath = GetMassingPresetDirectory() / req["presetFile"].get<std::string>();
        if (!std::filesystem::exists(presetPath) || presetPath.extension() != ".json") {
            return CreateErrorResponse("Massing preset not found: " + presetPath.string());
        }

        const std::string ruleJson = ReadTextFile(presetPath);
        if (ruleJson.empty()) {
            return CreateErrorResponse("Failed to read massing preset: " + presetPath.string());
        }

        json response;
        response["success"] = true;
        response["presetFile"] = presetPath.filename().string();
        response["ruleJson"] = ruleJson;
        return response.dump();
    }

    std::string HandleWriteLog(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        if (!req.contains("logContent")) {
            return CreateErrorResponse("Missing 'logContent' field");
        }

        std::string logContent = req["logContent"];
        
        // 閼惧嘲褰囬弮銉ョ箶閻╊喖缍嶇捄顖氱窞
        std::string logDir = "E:\\game_engine\\Moon\\bin\\x64\\Debug\\logs";
        std::string logFilePath = logDir + "\\frontend.log";
        
        try {
            // 閹垫挸绱戦弬鍥︽閿涘牐鎷烽崝鐘衬佸蹇ョ礆
            std::ofstream logFile(logFilePath, std::ios::app);
            if (!logFile.is_open()) {
                return CreateErrorResponse("Failed to open log file: " + logFilePath);
            }
            
            // 閸愭瑥鍙嗛弮銉ョ箶閸愬懎顔?
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
// 閸涙垝鎶ら弰鐘茬殸鐞涱煉绱欓棃娆愨偓浣稿灥婵瀵查敍?
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
    {"listMassingPresets",       CommandHandlers::HandleListMassingPresets},
    {"loadMassingPreset",        CommandHandlers::HandleLoadMassingPreset},
    {"writeLog",                 CommandHandlers::HandleWriteLog}
};

MoonEngineMessageHandler::MoonEngineMessageHandler()
    : m_engine(nullptr) {
}

void MoonEngineMessageHandler::SetEngineCore(EngineCore* engine) {
    m_engine = engine;
}

// ============================================================================
// 婢跺嫮鎮婇弶銉ㄥ殰 JavaScript 閻ㄥ嫭鐓＄拠?
// ============================================================================
bool MoonEngineMessageHandler::OnQuery(CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        int64_t query_id,
                                        const CefString& request,
                                        bool persistent,
                                        CefRefPtr<Callback> callback) {
    
    std::string requestStr = request.ToString();
    
    MOON_LOG_INFO("MoonEngineMessage", "OnQuery called with request: %s", requestStr.c_str());
    
    // 婢跺嫮鎮婄拠閿嬬湴楠炴儼骞忛崣鏍ф惙鎼?
    std::string response = ProcessRequest(requestStr);
    
    MOON_LOG_INFO("MoonEngineMessage", "Response: %s", response.c_str());
    
    // 鏉╂柨娲栭崫宥呯安缂?JavaScript
    callback->Success(response);
    
    return true;
}

void MoonEngineMessageHandler::OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                                                CefRefPtr<CefFrame> frame,
                                                int64_t query_id) {
    // 閺屻儴顕楃悮顐㈠絿濞戝牞绱濋弮鐘绘付婢跺嫮鎮?
}

// ============================================================================
// 婢跺嫮鎮婇崗铚傜秼鐠囬攱鐪伴敍鍫ュ櫢閺嬪嫬鎮楅敍姘▏閻劌鎳℃禒銈嗘Ё鐏忓嫯銆冮敍?
// ============================================================================
std::string MoonEngineMessageHandler::ProcessRequest(const std::string& request) {
    if (!m_engine) {
        return CreateErrorResponse("Engine not initialized");
    }

    try {
        // 鐟欙絾鐎?JSON 鐠囬攱鐪?
        json req = json::parse(request);
        
        if (!req.contains("command")) {
            return CreateErrorResponse("Missing 'command' field");
        }

        std::string command = req["command"];
        Moon::Scene* scene = m_engine->GetScene();

        // 閺屻儲澹橀崨鎴掓姢婢跺嫮鎮婇崳?
        auto it = s_commandHandlers.find(command);
        if (it != s_commandHandlers.end()) {
            // 鐠嬪啰鏁ょ€电懓绨查惃鍕槱閻炲棗鍤遍弫?
            return it->second(this, req, scene);
        }

        // 閺堫亞鐓￠崨鎴掓姢
        return CreateErrorResponse("Unknown command: " + command);
    }
    catch (const json::exception& e) {
        return CreateErrorResponse(std::string("JSON parse error: ") + e.what());
    }
    catch (const std::exception& e) {
        return CreateErrorResponse(std::string("Exception: ") + e.what());
    }
}

