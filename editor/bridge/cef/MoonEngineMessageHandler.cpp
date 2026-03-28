#include "MoonEngineMessageHandler.h"
#include "../../app/SceneSerializer.h"
#include "../../../engine/core/EngineCore.h"
#include "../../../engine/core/Logging/Logger.h"
#include "../../../engine/core/Assets/AssetPaths.h"
#include "../../../engine/core/Scene/MeshRenderer.h"
#include "../../../engine/core/Scene/Material.h"
#include "../../../engine/core/Scene/Light.h"
#include "../../../engine/core/Scene/Skybox.h"
#include "../../../engine/environment/EnvironmentComponent.h"
#include "../../../engine/core/CSG/CSGComponent.h"
#include "../../../engine/massing/BuildingMassingPlanner.h"
#include "../../../engine/massing/MassingPromptGenerator.h"
#include "../../../engine/massing/MassRuleParser.h"
#include "../../../engine/massing/MassMeshBuilder.h"
#include "../../../engine/building/BuildingPipeline.h"
#include "../../../engine/building/BuildingToObjectBlueprintConverter.h"
#include "../../../engine/core/Object/BlueprintLoader.h"
#include "../../../engine/core/CSG/CSGBuilder.h"
#include "../../../engine/core/Object/Blueprint.h"
#include "../../../engine/core/Mesh/Mesh.h"
#include "../../../engine/core/Geometry/MeshGenerator.h"
#include "../../../external/nlohmann/json.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <fstream>

using json = nlohmann::json;

// ?EditorApp.cpp ?
extern void SetSelectedObject(Moon::SceneNode* node);
extern Moon::SceneNode* GetSelectedObject();
extern void SetGizmoOperation(const std::string& mode);
extern void SetGizmoMode(const std::string& mode);  //  World/Local ?

// ============================================================================
// JSON ?
// ============================================================================
namespace {
    // ?
    std::string CreateSuccessResponse() {
        json result;
        result["success"] = true;
        return result.dump();
    }

    // ?
    std::string CreateErrorResponse(const std::string& errorMessage) {
        json error;
        error["error"] = errorMessage;
        return error.dump();
    }

    // ?Vector3 ?
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

    Moon::EnvironmentComponent* FindEditorEnvironmentComponent(Moon::Scene* scene) {
        if (!scene) {
            return nullptr;
        }

        Moon::SceneNode* environmentNode = scene->FindNodeByName("Editor Environment");
        if (!environmentNode) {
            return nullptr;
        }

        return environmentNode->GetComponent<Moon::EnvironmentComponent>();
    }

    std::string WeatherTypeToString(Moon::WeatherType weather) {
        switch (weather) {
        case Moon::WeatherType::Clear:  return "Clear";
        case Moon::WeatherType::Cloudy: return "Cloudy";
        case Moon::WeatherType::Rain:   return "Rain";
        case Moon::WeatherType::Fog:    return "Fog";
        case Moon::WeatherType::Storm:  return "Storm";
        }

        return "Clear";
    }

    bool TryParseWeatherType(const std::string& name, Moon::WeatherType& weather) {
        if (name == "Clear") {
            weather = Moon::WeatherType::Clear;
            return true;
        }
        if (name == "Cloudy") {
            weather = Moon::WeatherType::Cloudy;
            return true;
        }
        if (name == "Rain") {
            weather = Moon::WeatherType::Rain;
            return true;
        }
        if (name == "Fog") {
            weather = Moon::WeatherType::Fog;
            return true;
        }
        if (name == "Storm") {
            weather = Moon::WeatherType::Storm;
            return true;
        }

        return false;
    }

    // ?PBR ?
    void AddDefaultMaterial(Moon::SceneNode* node, const Moon::Vector3& baseColor = Moon::Vector3(1.0f, 1.0f, 1.0f)) {
        Moon::Material* material = node->AddComponent<Moon::Material>();
        material->SetMetallic(0.0f);
        material->SetRoughness(0.5f);
        material->SetBaseColor(baseColor);
        // ?eshV?
        material->SetMappingMode(Moon::MappingMode::UV);
    }

    // ?CSG  CSGiplanar?
    void AddCSGMaterial(Moon::SceneNode* node) {
        Moon::Material* material = node->AddComponent<Moon::Material>();
        material->SetMetallic(0.0f);
        material->SetRoughness(0.5f);
        material->SetBaseColor(Moon::Vector3(0.8f, 0.8f, 0.8f));
        // ?CSGiplanar?,0?
        material->SetMappingMode(Moon::MappingMode::Triplanar);
        material->SetTriplanarTiling(0.5f);  // ??
        material->SetTriplanarBlend(4.0f);   // 
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

    void AddEnvelopeMaterial(Moon::SceneNode* node) {
        Moon::Material* material = node->AddComponent<Moon::Material>();
        material->SetBaseColor(Moon::Vector3(0.64f, 0.69f, 0.76f));
        material->SetMetallic(0.0f);
        material->SetRoughness(0.88f);
        material->SetOpacity(0.28f);
        material->SetMappingMode(Moon::MappingMode::UV);
    }

    void AddUsablePlateMaterial(Moon::SceneNode* node) {
        Moon::Material* material = node->AddComponent<Moon::Material>();
        material->SetBaseColor(Moon::Vector3(0.74f, 0.76f, 0.8f));
        material->SetMetallic(0.0f);
        material->SetRoughness(0.9f);
        material->SetOpacity(1.0f);
        material->SetMappingMode(Moon::MappingMode::UV);
    }

    std::filesystem::path GetMassingPresetDirectory() {
        return std::filesystem::path(Moon::Assets::BuildAssetPath("massing"));
    }

    std::filesystem::path GetBuildingPresetDirectory() {
        return std::filesystem::path(Moon::Assets::BuildAssetPath("building"));
    }

    std::filesystem::path GetObjectPresetDirectory() {
        return std::filesystem::path(Moon::Assets::BuildObjectPath(""));
    }

    std::string ToForwardSlashPath(const std::filesystem::path& path) {
        return path.generic_string();
    }

    std::string BuildObjectPresetName(const std::filesystem::path& relativePath) {
        std::string name = relativePath.stem().string();
        const std::string parent = relativePath.parent_path().generic_string();
        if (!parent.empty()) {
            name += " (" + parent + ")";
        }
        return name;
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

    bool AppendMassingEnvelopePreview(Moon::Scene* scene,
                                      Moon::SceneNode* previewRoot,
                                      const std::string& massingRuleAsset,
                                      std::vector<std::string>& outWarnings,
                                      std::string& outError) {
        if (massingRuleAsset.empty()) {
            return true;
        }

        const std::filesystem::path rulePath = GetMassingPresetDirectory() / massingRuleAsset;
        const std::string ruleJson = ReadTextFile(rulePath);
        if (ruleJson.empty()) {
            outError = "Failed to read massing envelope rule: " + rulePath.string();
            return false;
        }

        Moon::Massing::RuleSet ruleSet;
        if (!Moon::Massing::MassRuleParser::ParseFromString(ruleJson, ruleSet, outError)) {
            outError = "Failed to parse massing envelope rule: " + outError;
            return false;
        }

        Moon::Massing::MassBuildResult massBuildResult;
        if (!Moon::Massing::MassMeshBuilder::Build(ruleSet, massBuildResult, outError)) {
            outError = "Failed to build massing envelope preview: " + outError;
            return false;
        }

        for (size_t i = 0; i < massBuildResult.items.size(); ++i) {
            const Moon::Massing::MassBuildItem& item = massBuildResult.items[i];
            Moon::SceneNode* childNode = scene->CreateNode("EnvelopePart_" + std::to_string(i));
            childNode->SetParent(previewRoot, false);
            Moon::MeshRenderer* renderer = childNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(item.mesh);
            AddEnvelopeMaterial(childNode);
        }

        outWarnings.insert(outWarnings.end(), massBuildResult.warnings.begin(), massBuildResult.warnings.end());
        return true;
    }

    std::shared_ptr<Moon::Mesh> CreateFloorPlateMesh(const Moon::Building::FloorPlate& plate,
                                                     const Moon::Building::BuildingDefinition& definition,
                                                     float slabThickness) {
        if (plate.outline.size() < 3) {
            return nullptr;
        }

        auto mesh = std::make_shared<Moon::Mesh>();
        std::vector<Moon::Vertex> vertices;
        std::vector<uint32_t> indices;

        const float baseY = Moon::Building::GetFloorBaseHeight(definition, plate.floorLevel);
        const float topY = baseY + slabThickness;

        float centerX = 0.0f;
        float centerZ = 0.0f;
        for (const auto& point : plate.outline) {
            centerX += point[0];
            centerZ += point[1];
        }
        centerX /= static_cast<float>(plate.outline.size());
        centerZ /= static_cast<float>(plate.outline.size());

        auto addVertex = [&](float x, float y, float z, const Moon::Vector3& normal, float u, float v) {
            vertices.emplace_back(Moon::Vector3(x, y, z), normal, Moon::Vector3(1.0f, 1.0f, 1.0f), 1.0f, Moon::Vector2(u, v));
        };

        const uint32_t topCenter = static_cast<uint32_t>(vertices.size());
        addVertex(centerX, topY, centerZ, Moon::Vector3(0.0f, 1.0f, 0.0f), 0.5f, 0.5f);
        for (const auto& point : plate.outline) {
            addVertex(point[0], topY, point[1], Moon::Vector3(0.0f, 1.0f, 0.0f), 0.0f, 0.0f);
        }

        const uint32_t bottomCenter = static_cast<uint32_t>(vertices.size());
        addVertex(centerX, baseY, centerZ, Moon::Vector3(0.0f, -1.0f, 0.0f), 0.5f, 0.5f);
        for (const auto& point : plate.outline) {
            addVertex(point[0], baseY, point[1], Moon::Vector3(0.0f, -1.0f, 0.0f), 0.0f, 0.0f);
        }

        const size_t count = plate.outline.size();
        for (size_t i = 0; i < count; ++i) {
            const size_t next = (i + 1) % count;
            indices.push_back(topCenter);
            indices.push_back(topCenter + 1 + static_cast<uint32_t>(i));
            indices.push_back(topCenter + 1 + static_cast<uint32_t>(next));

            indices.push_back(bottomCenter);
            indices.push_back(bottomCenter + 1 + static_cast<uint32_t>(next));
            indices.push_back(bottomCenter + 1 + static_cast<uint32_t>(i));
        }

        for (size_t i = 0; i < count; ++i) {
            const size_t next = (i + 1) % count;
            const auto& a = plate.outline[i];
            const auto& b = plate.outline[next];
            Moon::Vector3 edge(b[0] - a[0], 0.0f, b[1] - a[1]);
            Moon::Vector3 normal(edge.z, 0.0f, -edge.x);
            const float length = normal.Length();
            if (length > 0.0001f) {
                normal = normal * (1.0f / length);
            } else {
                normal = Moon::Vector3(1.0f, 0.0f, 0.0f);
            }

            const uint32_t sideStart = static_cast<uint32_t>(vertices.size());
            addVertex(a[0], topY, a[1], normal, 0.0f, 0.0f);
            addVertex(b[0], topY, b[1], normal, 1.0f, 0.0f);
            addVertex(b[0], baseY, b[1], normal, 1.0f, 1.0f);
            addVertex(a[0], baseY, a[1], normal, 0.0f, 1.0f);
            indices.push_back(sideStart + 0);
            indices.push_back(sideStart + 1);
            indices.push_back(sideStart + 2);
            indices.push_back(sideStart + 0);
            indices.push_back(sideStart + 2);
            indices.push_back(sideStart + 3);
        }

        mesh->SetVertices(std::move(vertices));
        mesh->SetIndices(std::move(indices));
        return mesh;
    }

    size_t AppendFloorPlateContourPreview(Moon::Scene* scene,
                                          Moon::SceneNode* previewRoot,
                                          const Moon::Building::GeneratedBuilding& building) {
        size_t meshCount = 0;
        for (const auto& plate : building.floorPlates) {
            if (plate.outline.size() < 3) {
                continue;
            }
            std::shared_ptr<Moon::Mesh> mesh = CreateFloorPlateMesh(plate, building.definition, 0.18f);
            if (!mesh || !mesh->IsValid()) {
                continue;
            }

            Moon::SceneNode* node = scene->CreateNode("UsablePlate_" + std::to_string(plate.floorLevel));
            node->SetParent(previewRoot, false);
            Moon::MeshRenderer* renderer = node->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(mesh);
            AddUsablePlateMaterial(node);
            ++meshCount;
        }
        return meshCount;
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

    Bounds3 ComputePreviewBounds(const Moon::CSG::BuildResult& buildResult) {
        Bounds3 bounds;

        for (const auto& item : buildResult.meshes) {
            if (!item.mesh) {
                continue;
            }

            for (const Moon::Vertex& vertex : item.mesh->GetVertices()) {
                const Moon::Vector3 position = vertex.position + item.worldTransform.position;
                ExpandBounds(bounds, position);
            }
        }

        return bounds;
    }

    Bounds3 ComputeObjectPreviewBounds(const Moon::CSG::BuildResult& buildResult) {
        Bounds3 bounds = ComputePreviewBounds(buildResult);
        for (const auto& light : buildResult.lights) {
            ExpandBounds(bounds, light.worldTransform.position + Moon::Vector3(-0.25f, -0.25f, -0.25f));
            ExpandBounds(bounds, light.worldTransform.position + Moon::Vector3(0.25f, 0.25f, 0.25f));
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

    void AddPreviewMaterial(Moon::SceneNode* node, const std::string& materialName) {
        Moon::Material* material = node->AddComponent<Moon::Material>();
        material->SetMetallic(0.0f);
        material->SetRoughness(0.72f);
        material->SetOpacity(1.0f);
        material->SetMappingMode(Moon::MappingMode::Triplanar);

        const Moon::MaterialPreset preset = ParseMaterialPreset(materialName);
        material->SetMaterialPreset(preset);
        if (preset == Moon::MaterialPreset::None) {
            material->SetBaseColor(Moon::Vector3(0.76f, 0.76f, 0.76f));
        }
    }
}

// ============================================================================
// ?
// ============================================================================
using CommandHandler = std::function<std::string(MoonEngineMessageHandler*, const json&, Moon::Scene*)>;

// ============================================================================
// ?
// ============================================================================
namespace CommandHandlers {
    // ?
    std::string HandleGetScene(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        return Moon::SceneSerializer::GetSceneHierarchy(scene);
    }

    // ?
    std::string HandleGetNodeDetails(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        return Moon::SceneSerializer::GetNodeDetails(scene, nodeId);
    }

    // 
    std::string HandleSelectNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        
        if (!node) {
            return CreateErrorResponse("Node not found");
        }

        MOON_LOG_INFO("MoonEngineMessage", "Selected node: %s (ID=%u)", 
                     node->GetName().c_str(), nodeId);
        
        // ?Gizmo 
        SetSelectedObject(node);
        
        return CreateSuccessResponse();
    }

    // 
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

    // 
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

    // 
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

    // ?Gizmo ranslate/rotate/scale?
    std::string HandleSetGizmoMode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string mode = req["mode"];
        
        SetGizmoOperation(mode);
        MOON_LOG_INFO("MoonEngineMessage", "Gizmo operation set to %s", mode.c_str());
        
        return CreateSuccessResponse();
    }

    // ========================================================================
    // Light ?
    // ========================================================================

    // ?Light ?
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

    // ?Light ?
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

    // ?Light int/Spot?
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

    // ?Light ?
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
    // Skybox ?
    // ========================================================================

    // ?Skybox ?
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

    // ?Skybox ?
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

    // ?Skybox ?
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

    // ?Skybox IBL
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

    // ?Skybox ?
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

    std::string HandleGetEnvironmentSettings(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        (void)handler;
        (void)req;

        Moon::EnvironmentComponent* environment = FindEditorEnvironmentComponent(scene);
        if (!environment) {
            return CreateErrorResponse("Editor environment component not found");
        }

        json result;
        result["success"] = true;
        result["timeOfDayHours"] = environment->GetState().timeOfDay.timeOfDayHours;
        result["weatherType"] = WeatherTypeToString(environment->GetState().weather.target);
        return result.dump();
    }

    std::string HandleSetEnvironmentTime(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        (void)handler;

        Moon::EnvironmentComponent* environment = FindEditorEnvironmentComponent(scene);
        if (!environment) {
            return CreateErrorResponse("Editor environment component not found");
        }

        const float hours = req.value("hours", environment->GetState().timeOfDay.timeOfDayHours);
        environment->SetTimeOfDay(hours);
        Moon::EnvironmentState state = environment->GetState();
        state.timeOfDay.paused = true;
        environment->GetSystem().SetState(state);

        MOON_LOG_INFO("MoonEngineMessage", "Set environment time to %.2f hours", hours);
        return CreateSuccessResponse();
    }

    std::string HandleSetEnvironmentWeather(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        (void)handler;

        Moon::EnvironmentComponent* environment = FindEditorEnvironmentComponent(scene);
        if (!environment) {
            return CreateErrorResponse("Editor environment component not found");
        }

        const std::string weatherTypeName = req.value("weatherType", std::string("Clear"));
        Moon::WeatherType weather = Moon::WeatherType::Clear;
        if (!TryParseWeatherType(weatherTypeName, weather)) {
            return CreateErrorResponse("Unknown weather type: " + weatherTypeName);
        }

        environment->SetWeather(weather, 1.5f);
        MOON_LOG_INFO("MoonEngineMessage", "Set environment weather to %s", weatherTypeName.c_str());
        return CreateSuccessResponse();
    }

    // ========================================================================
    // Material ?
    // ========================================================================

    // ?Material ?
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

    // ?Material ?
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

    // ?Material 
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

    // ?Material ?
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

    // ?Material ?
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

        // ?
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

    //  ?Gizmo world/local?
    std::string HandleSetGizmoCoordinateMode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string mode = req["mode"];
        
        SetGizmoMode(mode);
        MOON_LOG_INFO("MoonEngineMessage", "Gizmo coordinate mode set to %s", mode.c_str());
        
        return CreateSuccessResponse();
    }

    // 
    std::string HandleCreateNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        std::string type = req["type"];
        
        MOON_LOG_INFO("MoonEngineMessage", "Creating node of type: %s", type.c_str());
        
        // ?
        EngineCore* engine = handler->GetEngineCore();
        if (!engine) {
            return CreateErrorResponse("Engine core not available");
        }
        
        // ?
        Moon::SceneNode* newNode = nullptr;
        
        if (type == "empty") {
            newNode = scene->CreateNode("Empty Node");
        }
        // === ?MeshGenerator V ===
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
        // === CSGanifold?===
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
            
            // ?
            // Rotation: (45? -30? 0?  45? 30?
            newNode->GetTransform()->SetLocalRotation(Moon::Vector3(45.0f, -30.0f, 0.0f));
            
            // ?Light ?
            Moon::Light* light = newNode->AddComponent<Moon::Light>();
            light->SetType(Moon::Light::Type::Directional);
            light->SetColor(Moon::Vector3(1.0f, 0.95f, 0.9f));  // 
            light->SetIntensity(1.5f);  // ?
            
            MOON_LOG_INFO("MoonEngineMessage", "Created directional light (Intensity: %.1f)", 
                         light->GetIntensity());
        }
        else if (type == "skybox") {
            newNode = scene->CreateNode("Skybox");

            // ?Skybox ?
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
        
        //  ?
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

    // 
    std::string HandleDeleteNode(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }
        
        MOON_LOG_INFO("MoonEngineMessage", "Deleting node: %s (ID=%u)", 
                     node->GetName().c_str(), nodeId);
        
        // 
        if (GetSelectedObject() == node) {
            SetSelectedObject(nullptr);
        }
        
        // ?
        scene->DestroyNode(node);
        
        return CreateSuccessResponse();
    }

    // ?
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
            
            // ?
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

    // ?
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

    // ?
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
    //  Undo/Redo ?API?
    // ========================================================================
    
    /**
     * ?Delete Undo?
     * 
     * ?
     * {
     *   "command": "serializeNode",
     *   "nodeId": 123
     * }
     * 
     * ?
     * {
     *   "success": true,
     *   "data": "{ ... ?JSON ?... }"
     * }
     * 
     * ??API?WebUI Undo/Redo 
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
     * ?Delete Undo?
     * 
     * ?
     * {
     *   "command": "deserializeNode",
     *   "data": "{ ... ?JSON ?... }"
     * }
     * 
     * ??API?WebUI Undo/Redo 
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
     *  Transformosition + rotation + scale?
     * ?Undo/Redo ?
     * 
     * ?
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
     * ??API?WebUI Undo/Redo 
     */
    std::string HandleSetNodeTransform(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t nodeId = req["nodeId"];
        
        Moon::SceneNode* node = scene->FindNodeByID(nodeId);
        if (!node) {
            return CreateErrorResponse("Node not found");
        }
        
        const json& transform = req["transform"];
        
        //  Transform?API ?
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
     * ?ID?Undo ?
     * 
     * ?
     * {
     *   "command": "createNodeWithId",
     *   "nodeId": 123,  //  ID
     *   "name": "MyNode",
     *   "type": "empty",  // "empty", "cube", "sphere", etc.
     *   "parentId": 456,  // ?
     *   "transform": {    // ?
     *     "position": {...},
     *     "rotation": {...},
     *     "scale": {...}
     *   }
     * }
     * 
     * ??API?WebUI Undo/Redo 
     * ??ID ?
     */
    std::string HandleCreateNodeWithId(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        uint32_t targetId = req["nodeId"];
        std::string name = req["name"];
        std::string type = req.contains("type") ? req["type"].get<std::string>() : "empty";
        
        //  ?ID ?
        if (scene->FindNodeByID(targetId)) {
            return CreateErrorResponse("Node with ID already exists: " + std::to_string(targetId));
        }
        
        MOON_LOG_INFO("MoonEngineMessage", "[Undo] Creating node with ID=%u, name=%s, type=%s", 
                     targetId, name.c_str(), type.c_str());
        
        // ?
        EngineCore* engine = handler->GetEngineCore();
        if (!engine) {
            return CreateErrorResponse("Engine core not available");
        }
        
        // ?ID?
        Moon::SceneNode* newNode = scene->CreateNodeWithID(targetId, name);
        
        if (!newNode) {
            return CreateErrorResponse("Failed to create node with specified ID");
        }
        
        // ?
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
        
        // ?
        if (req.contains("parentId") && !req["parentId"].is_null()) {
            uint32_t parentId = req["parentId"];
            Moon::SceneNode* parent = scene->FindNodeByID(parentId);
            if (parent) {
                newNode->SetParent(parent);
            } else {
                MOON_LOG_WARN("MoonEngineMessage", "Parent node %u not found", parentId);
            }
        }
        
        // ?Transform
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

    // 
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

    std::string HandlePreviewBuilding(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        if (!req.contains("buildingJson")) {
            return CreateErrorResponse("Missing 'buildingJson' field");
        }

        Moon::Building::GeneratedBuilding building;
        std::string pipelineError;
        Moon::Building::BuildingPipeline pipeline;
        if (!pipeline.ProcessBuilding(req["buildingJson"].get<std::string>(), building, pipelineError)) {
            return CreateErrorResponse("Failed to process building preview: " + pipelineError);
        }

        building.walls.clear();
        building.doors.clear();
        building.windows.clear();
        building.stairs.clear();
        building.supportColumns.clear();
        const bool hasMassDrivenEnvelope = std::any_of(
            building.definition.masses.begin(),
            building.definition.masses.end(),
            [](const Moon::Building::Mass& mass) { return !mass.massingRuleAsset.empty(); });

        if (hasMassDrivenEnvelope) {
            building.verticalCores.clear();
            building.programBlocks.clear();
        }

        std::string blueprintJson = Moon::Building::BuildingToObjectBlueprintConverter::Convert(building);
        std::string loadError;
        Moon::Object::BlueprintDatabase database;
        const std::string csgIndexPath = Moon::Assets::BuildObjectPath("index.json");
        if (!database.LoadIndex(csgIndexPath, loadError)) {
            return CreateErrorResponse("Failed to load CSG index: " + loadError);
        }

        auto generatedBlueprint = Moon::Object::BlueprintLoader::ParseFromString(blueprintJson, loadError);
        if (!generatedBlueprint) {
            return CreateErrorResponse("Failed to parse building blueprint: " + loadError);
        }

        Moon::CSG::CSGBuilder builder;
        builder.SetBlueprintDatabase(&database);
        std::unordered_map<std::string, float> params;
        Moon::CSG::BuildResult buildResult = builder.Build(generatedBlueprint.get(), params, loadError);
        if (buildResult.meshes.empty()) {
            return CreateErrorResponse("Failed to build building preview: " + loadError);
        }

        ClearMassingPreviewNodes(scene);
        Moon::SceneNode* previewRoot = scene->CreateNode("__MassingPreview");

        for (size_t i = 0; i < buildResult.meshes.size(); ++i) {
            const auto& item = buildResult.meshes[i];
            const std::string childName = "BuildingPart_" + std::to_string(i);
            Moon::SceneNode* childNode = scene->CreateNode(childName);
            childNode->SetParent(previewRoot, false);
            childNode->GetTransform()->SetLocalPosition(item.worldTransform.position);
            childNode->GetTransform()->SetLocalRotation(item.worldTransform.rotation);
            childNode->GetTransform()->SetLocalScale(item.worldTransform.scale);

            Moon::MeshRenderer* renderer = childNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(item.mesh);
            AddPreviewMaterial(childNode, item.material);
        }

        const size_t contourPlateMeshCount = AppendFloorPlateContourPreview(scene, previewRoot, building);

        std::vector<std::string> previewWarnings;
        if (hasMassDrivenEnvelope) {
            for (const auto& mass : building.definition.masses) {
                if (mass.massingRuleAsset.empty()) {
                    continue;
                }
                if (!AppendMassingEnvelopePreview(scene, previewRoot, mass.massingRuleAsset, previewWarnings, loadError)) {
                    return CreateErrorResponse(loadError);
                }
            }
        }

        const bool focusCamera = req.value("focusCamera", false);
        if (focusCamera) {
            FrameCameraToBounds(handler, ComputePreviewBounds(buildResult));
        }

        json response;
        response["success"] = true;
        response["rootNodeId"] = previewRoot->GetID();
        response["meshCount"] = buildResult.meshes.size() + contourPlateMeshCount;
        response["programBlockCount"] = building.programBlocks.size();
        response["floorPlateCount"] = building.floorPlates.size();
        response["coreCount"] = building.verticalCores.size();
        response["resolvedLayoutJson"] = building.resolvedLayoutJson;
        response["warnings"] = previewWarnings;
        return response.dump();
    }

    std::string HandlePreviewObject(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        if (!req.contains("objectJson")) {
            return CreateErrorResponse("Missing 'objectJson' field");
        }

        std::string loadError;
        Moon::Object::BlueprintDatabase database;
        const std::string objectIndexPath = Moon::Assets::BuildObjectPath("index.json");
        if (!database.LoadIndex(objectIndexPath, loadError)) {
            return CreateErrorResponse("Failed to load object index: " + loadError);
        }

        auto blueprint = Moon::Object::BlueprintLoader::ParseFromString(req["objectJson"].get<std::string>(), loadError);
        if (!blueprint) {
            return CreateErrorResponse("Failed to parse object preview: " + loadError);
        }

        Moon::CSG::CSGBuilder builder;
        builder.SetBlueprintDatabase(&database);
        std::unordered_map<std::string, float> params;
        Moon::CSG::BuildResult buildResult = builder.Build(blueprint.get(), params, loadError);
        if (buildResult.meshes.empty() && buildResult.lights.empty()) {
            return CreateErrorResponse("Failed to build object preview: " + loadError);
        }

        ClearMassingPreviewNodes(scene);
        Moon::SceneNode* previewRoot = scene->CreateNode("__MassingPreview");

        for (size_t i = 0; i < buildResult.meshes.size(); ++i) {
            const auto& item = buildResult.meshes[i];
            const std::string childName = "ObjectPart_" + std::to_string(i);
            Moon::SceneNode* childNode = scene->CreateNode(childName);
            childNode->SetParent(previewRoot, false);
            childNode->GetTransform()->SetLocalPosition(item.worldTransform.position);
            childNode->GetTransform()->SetLocalRotation(item.worldTransform.rotation);
            childNode->GetTransform()->SetLocalScale(item.worldTransform.scale);

            Moon::MeshRenderer* renderer = childNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(item.mesh);
            AddPreviewMaterial(childNode, item.material);
        }

        for (size_t i = 0; i < buildResult.lights.size(); ++i) {
            const auto& item = buildResult.lights[i];
            const std::string childName = "ObjectLight_" + std::to_string(i);
            Moon::SceneNode* childNode = scene->CreateNode(childName);
            childNode->SetParent(previewRoot, false);
            childNode->GetTransform()->SetLocalPosition(item.worldTransform.position);
            childNode->GetTransform()->SetLocalRotation(item.worldTransform.rotation);
            childNode->GetTransform()->SetLocalScale(item.worldTransform.scale);

            Moon::Light* light = childNode->AddComponent<Moon::Light>();
            switch (item.type) {
            case Moon::CSG::LightItem::Type::Directional:
                light->SetType(Moon::Light::Type::Directional);
                break;
            case Moon::CSG::LightItem::Type::Point:
                light->SetType(Moon::Light::Type::Point);
                break;
            case Moon::CSG::LightItem::Type::Spot:
                light->SetType(Moon::Light::Type::Spot);
                break;
            }
            light->SetColor(item.color);
            light->SetIntensity(item.intensity);
            if (item.type == Moon::CSG::LightItem::Type::Point ||
                item.type == Moon::CSG::LightItem::Type::Spot) {
                light->SetRange(item.range);
                light->SetAttenuation(item.attenuation.x, item.attenuation.y, item.attenuation.z);
            }
            if (item.type == Moon::CSG::LightItem::Type::Spot) {
                light->SetSpotAngles(item.spotInnerConeAngle, item.spotOuterConeAngle);
            }
            light->SetCastShadows(item.castShadows);

            std::shared_ptr<Moon::Mesh> markerMesh(Moon::MeshGenerator::CreateSphere(0.08f, 12, 8, item.color));
            if (markerMesh && markerMesh->IsValid()) {
                Moon::MeshRenderer* renderer = childNode->AddComponent<Moon::MeshRenderer>();
                renderer->SetMesh(markerMesh);
                AddDefaultMaterial(childNode, item.color);
            }
        }

        const bool focusCamera = req.value("focusCamera", false);
        if (focusCamera) {
            FrameCameraToBounds(handler, ComputeObjectPreviewBounds(buildResult));
        }

        json response;
        response["success"] = true;
        response["rootNodeId"] = previewRoot->GetID();
        response["meshCount"] = buildResult.meshes.size();
        response["lightCount"] = buildResult.lights.size();
        response["warnings"] = json::array();
        return response.dump();
    }

    std::string HandlePlanBuildingMassing(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        (void)handler;
        (void)scene;

        if (!req.contains("intentJson")) {
            return CreateErrorResponse("Missing 'intentJson' field");
        }

        Moon::Massing::BuildingMassingIntent intent;
        std::string parseError;
        if (!Moon::Massing::BuildingMassingPlanner::ParseIntentFromString(
                req["intentJson"].get<std::string>(), intent, parseError)) {
            return CreateErrorResponse("Failed to parse building massing intent: " + parseError);
        }

        std::string ruleJson;
        std::string planError;
        if (!Moon::Massing::BuildingMassingPlanner::PlanToJsonString(intent, ruleJson, planError)) {
            return CreateErrorResponse("Failed to plan building massing: " + planError);
        }

        json response;
        response["success"] = true;
        response["ruleJson"] = ruleJson;
        response["archetype"] = intent.archetype;
        response["buildingType"] = intent.buildingType;
        return response.dump();
    }

    std::string HandleGenerateMassingFromPrompt(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        (void)handler;
        (void)scene;

        if (!req.contains("prompt")) {
            return CreateErrorResponse("Missing 'prompt' field");
        }

        Moon::Massing::MassingPromptGenerationResult result;
        std::string generationError;
        if (!Moon::Massing::MassingPromptGenerator::GenerateFromPrompt(
                req["prompt"].get<std::string>(),
                req.value("currentRuleJson", std::string()),
                result,
                generationError)) {
            return CreateErrorResponse("Failed to generate massing from prompt: " + generationError);
        }

        json response;
        response["success"] = true;
        response["ruleJson"] = result.ruleJson;
        response["strategy"] = result.strategy;
        response["hiddenContextSummary"] = result.hiddenContextSummary;
        response["notes"] = result.notes;
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

        std::vector<std::filesystem::path> presetPaths;
        const std::filesystem::path massingDirectory = GetMassingPresetDirectory();
        if (std::filesystem::exists(massingDirectory)) {
            for (const auto& entry : std::filesystem::directory_iterator(massingDirectory)) {
                const std::filesystem::path path = entry.path();
                const std::string stem = path.stem().string();
                if (entry.is_regular_file() && path.extension() == ".json" &&
                    stem.rfind("planned_", 0) == 0) {
                    presetPaths.push_back(path);
                }
            }
        }

        const std::filesystem::path buildingDirectory = GetBuildingPresetDirectory();
        if (std::filesystem::exists(buildingDirectory)) {
            for (const auto& entry : std::filesystem::directory_iterator(buildingDirectory)) {
                const std::filesystem::path path = entry.path();
                if (entry.is_regular_file() && path.extension() == ".json") {
                    presetPaths.push_back(path);
                }
            }
        }

        std::sort(presetPaths.begin(), presetPaths.end());

        json response;
        response["success"] = true;
        response["presets"] = json::array();

        for (const std::filesystem::path& path : presetPaths) {
            json preset;
            preset["id"] = MakePresetId(path);
            const bool isBuildingPreset = path.parent_path().filename() == "building";
            preset["name"] = isBuildingPreset
                ? "[Building] " + path.stem().string()
                : path.stem().string();
            preset["file"] = isBuildingPreset
                ? std::string("building/") + path.filename().string()
                : std::string("massing/") + path.filename().string();
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

        const std::string presetFile = req["presetFile"].get<std::string>();
        std::filesystem::path presetPath;
        if (presetFile.rfind("building/", 0) == 0) {
            presetPath = GetBuildingPresetDirectory() / presetFile.substr(9);
        } else if (presetFile.rfind("massing/", 0) == 0) {
            presetPath = GetMassingPresetDirectory() / presetFile.substr(8);
        } else {
            presetPath = GetMassingPresetDirectory() / presetFile;
        }
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

    std::string HandleListObjectPresets(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        (void)handler;
        (void)req;
        (void)scene;

        const std::filesystem::path objectDirectory = GetObjectPresetDirectory();
        if (!std::filesystem::exists(objectDirectory)) {
            return CreateErrorResponse("Object preset directory not found: " + objectDirectory.string());
        }

        std::vector<std::filesystem::path> presetPaths;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(objectDirectory)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                continue;
            }

            const std::filesystem::path relativePath = std::filesystem::relative(entry.path(), objectDirectory);
            const std::string relativeString = ToForwardSlashPath(relativePath);
            if (relativeString == "catalog.json" ||
                relativeString == "index.json" ||
                relativeString == "generated_building.json") {
                continue;
            }

            presetPaths.push_back(relativePath);
        }

        std::sort(presetPaths.begin(), presetPaths.end());

        json response;
        response["success"] = true;
        response["presets"] = json::array();

        for (const std::filesystem::path& relativePath : presetPaths) {
            json preset;
            preset["id"] = "object:" + ToForwardSlashPath(relativePath);
            preset["name"] = BuildObjectPresetName(relativePath);
            preset["file"] = "objects/" + ToForwardSlashPath(relativePath);
            response["presets"].push_back(preset);
        }

        return response.dump();
    }

    std::string HandleLoadObjectPreset(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        (void)handler;
        (void)scene;

        if (!req.contains("presetFile")) {
            return CreateErrorResponse("Missing 'presetFile' field");
        }

        const std::string presetFile = req["presetFile"].get<std::string>();
        std::filesystem::path presetPath;
        if (presetFile.rfind("objects/", 0) == 0) {
            presetPath = GetObjectPresetDirectory() / presetFile.substr(8);
        } else {
            presetPath = GetObjectPresetDirectory() / presetFile;
        }

        if (!std::filesystem::exists(presetPath) || presetPath.extension() != ".json") {
            return CreateErrorResponse("Object preset not found: " + presetPath.string());
        }

        const std::string objectJson = ReadTextFile(presetPath);
        if (objectJson.empty()) {
            return CreateErrorResponse("Failed to read object preset: " + presetPath.string());
        }

        json response;
        response["success"] = true;
        response["presetFile"] = ToForwardSlashPath(std::filesystem::relative(presetPath, GetObjectPresetDirectory()));
        response["objectJson"] = objectJson;
        return response.dump();
    }

    std::string HandleWriteLog(MoonEngineMessageHandler* handler, const json& req, Moon::Scene* scene) {
        if (!req.contains("logContent")) {
            return CreateErrorResponse("Missing 'logContent' field");
        }

        std::string logContent = req["logContent"];
        
        // 
        std::string logDir = "E:\\game_engine\\Moon\\bin\\x64\\Debug\\logs";
        std::string logFilePath = logDir + "\\frontend.log";
        
        try {
            // 
            std::ofstream logFile(logFilePath, std::ios::app);
            if (!logFile.is_open()) {
                return CreateErrorResponse("Failed to open log file: " + logFilePath);
            }
            
            // ?
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
// ?
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
    {"getEnvironmentSettings",   CommandHandlers::HandleGetEnvironmentSettings},
    {"setEnvironmentTime",       CommandHandlers::HandleSetEnvironmentTime},
    {"setEnvironmentWeather",    CommandHandlers::HandleSetEnvironmentWeather},
    {"listObjectPresets",        CommandHandlers::HandleListObjectPresets},
    {"loadObjectPreset",         CommandHandlers::HandleLoadObjectPreset},
    {"previewObject",            CommandHandlers::HandlePreviewObject},
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
    {"planBuildingMassing",      CommandHandlers::HandlePlanBuildingMassing},
    {"generateMassingFromPrompt", CommandHandlers::HandleGenerateMassingFromPrompt},
    {"previewMassing",           CommandHandlers::HandlePreviewMassing},
    {"previewBuilding",          CommandHandlers::HandlePreviewBuilding},
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
//  JavaScript ?
// ============================================================================
bool MoonEngineMessageHandler::OnQuery(CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        int64_t query_id,
                                        const CefString& request,
                                        bool persistent,
                                        CefRefPtr<Callback> callback) {
    
    std::string requestStr = request.ToString();
    
    MOON_LOG_INFO("MoonEngineMessage", "OnQuery called with request: %s", requestStr.c_str());
    
    // ?
    std::string response = ProcessRequest(requestStr);
    
    MOON_LOG_INFO("MoonEngineMessage", "Response: %s", response.c_str());
    
    // ?JavaScript
    callback->Success(response);
    
    return true;
}

void MoonEngineMessageHandler::OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                                                CefRefPtr<CefFrame> frame,
                                                int64_t query_id) {
    // ?
}

// ============================================================================
// ?
// ============================================================================
std::string MoonEngineMessageHandler::ProcessRequest(const std::string& request) {
    if (!m_engine) {
        return CreateErrorResponse("Engine not initialized");
    }

    try {
        // ?JSON ?
        json req = json::parse(request);
        
        if (!req.contains("command")) {
            return CreateErrorResponse("Missing 'command' field");
        }

        std::string command = req["command"];
        Moon::Scene* scene = m_engine->GetScene();

        // ?
        auto it = s_commandHandlers.find(command);
        if (it != s_commandHandlers.end()) {
            // ?
            return it->second(this, req, scene);
        }

        // 
        return CreateErrorResponse("Unknown command: " + command);
    }
    catch (const json::exception& e) {
        return CreateErrorResponse(std::string("JSON parse error: ") + e.what());
    }
    catch (const std::exception& e) {
        return CreateErrorResponse(std::string("Exception: ") + e.what());
    }
}

