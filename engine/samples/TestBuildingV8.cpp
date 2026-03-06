#include "TestScenes.h"
#include <Logging/Logger.h>
#include <EngineCore.h>
#include <Scene/Scene.h>
#include <Scene/SceneNode.h>
#include <Scene/MeshRenderer.h>
#include <Scene/Material.h>
#include <Scene/Light.h>
#include <Mesh/Mesh.h>
#include <Building.h>
#include <CSG/Blueprint.h>
#include <CSG/BlueprintLoader.h>
#include <CSG/CSGBuilder.h>
#include "../../external/nlohmann/json.hpp"
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace TestScenes {

// Helper function to read file contents
static std::string ReadFileToString(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Convert Building System V8 output to CSG Blueprint JSON
static json ConvertToCSGBlueprint(const Moon::Building::GeneratedBuilding& building) {
    json blueprint;
    blueprint["schema_version"] = 1;
    blueprint["name"] = "generated_building_v8";
    blueprint["description"] = "Auto-generated from Building System V8";
    blueprint["version"] = 1;
    
    json children = json::array();
    const float wallThickness = 0.2f; // 20cm walls
    
    // Generate floors
    int floorIdx = 0;
    for (const auto& floor : building.definition.floors) {
        for (const auto& space : floor.spaces) {
            for (const auto& rect : space.rects) {
                json floorNode;
                floorNode["name"] = "floor_" + std::to_string(space.spaceId) + "_" + rect.rectId;
                floorNode["type"] = "primitive";
                floorNode["primitive"] = "cube";
                
                // rect.origin is [X, Y], rect.size is [width, depth]
                // Convert meters to centimeters (x100)
                float centerX = (rect.origin[0] + rect.size[0] * 0.5f) * 100.0f;
                float centerY = (floor.level * floor.floorHeight + 0.025f) * 100.0f; // 5cm thick floor
                float centerZ = (rect.origin[1] + rect.size[1] * 0.5f) * 100.0f;
                
                floorNode["params"] = {
                    {"size_x", rect.size[0] * 100.0f},
                    {"size_y", 5.0f}, // 5cm thick
                    {"size_z", rect.size[1] * 100.0f}
                };
                floorNode["transform"] = {
                    {"position", json::array({centerX, centerY, centerZ})}
                };
                
                // Material based on space usage
                if (space.properties.usageHint == "living") {
                    floorNode["material"] = "wood_floor";
                } else if (space.properties.usageHint == "kitchen") {
                    floorNode["material"] = "tile_ceramic";
                } else if (space.properties.usageHint == "bedroom") {
                    floorNode["material"] = "carpet";
                } else {
                    floorNode["material"] = "concrete_floor";
                }
                
                children.push_back(floorNode);
                floorIdx++;
            }
        }
    }
    
    // Generate walls
    int wallIdx = 0;
    for (const auto& wall : building.walls) {
        // wall.start and wall.end are GridPos2D [X, Y]
        float dx = wall.end[0] - wall.start[0];
        float dz = wall.end[1] - wall.start[1];
        float length = std::sqrt(dx * dx + dz * dz);
        
        if (length < 0.01f) continue;
        
        float centerX = (wall.start[0] + wall.end[0]) * 0.5f * 100.0f;
        float centerY = wall.height * 0.5f * 100.0f; // Assume wall starts at Y=0
        float centerZ = (wall.start[1] + wall.end[1]) * 0.5f * 100.0f;
        
        // Wall is along X or Z axis
        bool alongX = std::abs(dz) < 0.01f;
        
        json wallNode;
        wallNode["name"] = "wall_" + std::to_string(wallIdx);
        wallNode["type"] = "primitive";
        wallNode["primitive"] = "cube";
        wallNode["params"] = {
            {"size_x", (alongX ? length : wall.thickness) * 100.0f},
            {"size_y", wall.height * 100.0f},
            {"size_z", (alongX ? wall.thickness : length) * 100.0f}
        };
        wallNode["transform"] = {
            {"position", json::array({centerX, centerY, centerZ})}
        };
        wallNode["material"] = (wall.type == Moon::Building::WallType::Exterior) ? "brick" : "plaster";
        
        children.push_back(wallNode);
        wallIdx++;
    }
    
    // Add doors
    int doorIdx = 0;
    for (const auto& door : building.doors) {
        json doorNode;
        doorNode["name"] = "door_" + std::to_string(doorIdx++);
        doorNode["type"] = "reference";
        doorNode["ref"] = "complete_door_v1";
        doorNode["overrides"] = {
            {"door_width", door.width * 100.0f},
            {"door_height", door.height * 100.0f},
            {"frame_depth", wallThickness * 100.0f}
        };
        doorNode["transform"] = {
            {"position", json::array({door.position[0] * 100.0f, 0.0f, door.position[1] * 100.0f})},
            {"rotation", json::array({0.0f, door.rotation, 0.0f})}
        };
        
        children.push_back(doorNode);
    }
    
    // Add windows
    int windowIdx = 0;
    for (const auto& window : building.windows) {
        json windowNode;
        windowNode["name"] = "window_" + std::to_string(windowIdx++);
        windowNode["type"] = "reference";
        windowNode["ref"] = "window_v1";
        windowNode["overrides"] = {
            {"w", window.width * 100.0f},
            {"h", window.height * 100.0f},
            {"t", wallThickness * 100.0f}
        };
        windowNode["transform"] = {
            {"position", json::array({window.position[0] * 100.0f, window.sillHeight * 100.0f, window.position[1] * 100.0f})},
            {"rotation", json::array({0.0f, window.rotation, 0.0f})}
        };
        
        children.push_back(windowNode);
    }
    
    blueprint["root"] = {
        {"type", "group"},
        {"children", children},
        {"output", {{"mode", "separate"}}}
    };
    
    return blueprint;
}

void TestBuildingV8(EngineCore* engine)
{
    MOON_LOG_INFO("BuildingV8", "=== Testing Building System V8 with CSG ===");
    
    // Read Building System V8 JSON input
    std::string jsonContent = ReadFileToString("assets/csg/test_building.json");
    if (jsonContent.empty()) {
        MOON_LOG_ERROR("BuildingV8", "Failed to read test_building.json");
        return;
    }
    
    MOON_LOG_INFO("BuildingV8", "Loaded Building V8 JSON: %zu bytes", jsonContent.size());
    
    // Process building through Building System V8 pipeline
    Moon::Building::BuildingPipeline pipeline;
    Moon::Building::GeneratedBuilding result;
    std::string error;
    
    if (!pipeline.ProcessBuilding(jsonContent, result, error)) {
        MOON_LOG_ERROR("BuildingV8", "Building processing failed: %s", error.c_str());
        return;
    }
    
    MOON_LOG_INFO("BuildingV8", "Building generated successfully:");
    MOON_LOG_INFO("BuildingV8", "  - Walls: %zu", result.walls.size());
    MOON_LOG_INFO("BuildingV8", "  - Doors: %zu", result.doors.size());
    MOON_LOG_INFO("BuildingV8", "  - Windows: %zu", result.windows.size());
    MOON_LOG_INFO("BuildingV8", "  - Connections: %zu", result.connections.size());
    
    // Convert to CSG Blueprint
    MOON_LOG_INFO("BuildingV8", "Converting to CSG Blueprint...");
    json csgBlueprint = ConvertToCSGBlueprint(result);
    
    // Save CSG Blueprint to file
    std::string outputPath = "assets/csg/generated_building_v8.json";
    std::ofstream outFile(outputPath);
    if (outFile.is_open()) {
        outFile << csgBlueprint.dump(2);
        outFile.close();
        MOON_LOG_INFO("BuildingV8", "Saved CSG Blueprint to: %s", outputPath.c_str());
        MOON_LOG_INFO("BuildingV8", "CSG Blueprint size: %zu bytes", csgBlueprint.dump(2).size());
    } else {
        MOON_LOG_ERROR("BuildingV8", "Failed to save CSG Blueprint to %s", outputPath.c_str());
        return;
    }
    
    // ============================================================================
    // Load and render the generated CSG Blueprint
    // ============================================================================
    MOON_LOG_INFO("BuildingV8", "Loading generated CSG Blueprint...");
    
    Moon::Scene* scene = engine->GetScene();
    Moon::CSG::BlueprintDatabase database;
    std::string loadError;
    
    // Load index.json for component references
    if (!database.LoadIndex("assets/csg/index.json", loadError)) {
        MOON_LOG_ERROR("BuildingV8", "Failed to load CSG index: %s", loadError.c_str());
        return;
    }
    
    // Parse the generated blueprint from JSON string
    std::string blueprintJsonStr = csgBlueprint.dump();
    auto generatedBlueprint = Moon::CSG::BlueprintLoader::ParseFromString(blueprintJsonStr, loadError);
    if (!generatedBlueprint) {
        MOON_LOG_ERROR("BuildingV8", "Failed to parse generated blueprint: %s", loadError.c_str());
        return;
    }
    
    // Build the CSG scene
    Moon::CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(&database);
    
    std::unordered_map<std::string, float> params;
    Moon::CSG::BuildResult buildResult = builder.Build(generatedBlueprint.get(), params, loadError);
    
    if (buildResult.meshes.empty()) {
        MOON_LOG_ERROR("BuildingV8", "Failed to build CSG scene: %s", loadError.c_str());
        return;
    }
    
    MOON_LOG_INFO("BuildingV8", "CSG scene built successfully: %d meshes", (int)buildResult.meshes.size());
    
    // Create scene nodes for all meshes
    for (size_t i = 0; i < buildResult.meshes.size(); ++i) {
        const auto& item = buildResult.meshes[i];
        if (item.mesh && item.mesh->IsValid()) {
            auto node = scene->CreateNode("building_part_" + std::to_string(i));
            
            node->GetTransform()->SetLocalPosition(item.worldTransform.position);
            node->GetTransform()->SetLocalRotation(item.worldTransform.rotation);
            node->GetTransform()->SetLocalScale(item.worldTransform.scale);
            
            auto renderer = node->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(item.mesh);
            
            // Apply material based on material name
            auto mat = node->AddComponent<Moon::Material>();
            const std::string& matName = item.material;
            
            bool materialApplied = false;
            
            // === 混凝土系列 ===
            if (matName == "concrete")              { mat->SetMaterialPreset(Moon::MaterialPreset::Concrete); materialApplied = true; }
            else if (matName == "concrete_floor")   { mat->SetMaterialPreset(Moon::MaterialPreset::ConcreteFloor); materialApplied = true; }
            else if (matName == "concrete_polished"){ mat->SetMaterialPreset(Moon::MaterialPreset::ConcretePolished); materialApplied = true; }
            
            // === 岩石/砖石系列 ===
            else if (matName == "rock" || matName == "stone") { mat->SetMaterialPreset(Moon::MaterialPreset::Rock); materialApplied = true; }
            else if (matName == "brick")            { mat->SetMaterialPreset(Moon::MaterialPreset::Brick); materialApplied = true; }
            else if (matName == "plaster")          { mat->SetMaterialPreset(Moon::MaterialPreset::Plaster); materialApplied = true; }
            else if (matName == "tile_ceramic")     { mat->SetMaterialPreset(Moon::MaterialPreset::TileCeramic); materialApplied = true; }
            
            // === 木材系列 ===
            else if (matName == "wood")             { mat->SetMaterialPreset(Moon::MaterialPreset::Wood); materialApplied = true; }
            else if (matName == "wood_floor")       { mat->SetMaterialPreset(Moon::MaterialPreset::WoodFloor); materialApplied = true; }
            else if (matName == "wood_polished")    { mat->SetMaterialPreset(Moon::MaterialPreset::WoodPolished); materialApplied = true; }
            else if (matName == "wood_painted")     { mat->SetMaterialPreset(Moon::MaterialPreset::WoodPainted); materialApplied = true; }
            
            // === 金属系列 ===
            else if (matName == "metal")            { mat->SetMaterialPreset(Moon::MaterialPreset::Metal); materialApplied = true; }
            else if (matName == "steel")            { mat->SetMaterialPreset(Moon::MaterialPreset::Steel); materialApplied = true; }
            else if (matName == "aluminum")         { mat->SetMaterialPreset(Moon::MaterialPreset::Aluminum); materialApplied = true; }
            else if (matName == "chrome")           { mat->SetMaterialPreset(Moon::MaterialPreset::Chrome); materialApplied = true; }
            else if (matName == "copper")           { mat->SetMaterialPreset(Moon::MaterialPreset::Copper); materialApplied = true; }
            
            // === 玻璃系列 ===
            else if (matName == "glass")            { mat->SetMaterialPreset(Moon::MaterialPreset::Glass); materialApplied = true; }
            else if (matName == "glass_frosted")    { mat->SetMaterialPreset(Moon::MaterialPreset::GlassFrosted); materialApplied = true; }
            else if (matName == "glass_tinted")     { mat->SetMaterialPreset(Moon::MaterialPreset::GlassTinted); materialApplied = true; }
            
            // === 软装系列 ===
            else if (matName == "fabric")           { mat->SetMaterialPreset(Moon::MaterialPreset::Fabric); materialApplied = true; }
            else if (matName == "leather")          { mat->SetMaterialPreset(Moon::MaterialPreset::Leather); materialApplied = true; }
            else if (matName == "carpet")           { mat->SetMaterialPreset(Moon::MaterialPreset::Carpet); materialApplied = true; }
            
            // === 塑料/橡胶系列 ===
            else if (matName == "plastic")          { mat->SetMaterialPreset(Moon::MaterialPreset::Plastic); materialApplied = true; }
            else if (matName == "rubber")           { mat->SetMaterialPreset(Moon::MaterialPreset::Rubber); materialApplied = true; }
            
            else if (!matName.empty()) {
                MOON_LOG_WARN("BuildingV8", "Unknown material '%s'", matName.c_str());
            }
            
            mat->SetMappingMode(Moon::MappingMode::Triplanar);
        }
    }
    
    // Create lights
    for (size_t i = 0; i < buildResult.lights.size(); ++i) {
        const auto& item = buildResult.lights[i];
        auto node = scene->CreateNode("building_light_" + std::to_string(i));

        node->GetTransform()->SetLocalPosition(item.worldTransform.position);
        node->GetTransform()->SetLocalRotation(item.worldTransform.rotation);
        node->GetTransform()->SetLocalScale(item.worldTransform.scale);

        auto light = node->AddComponent<Moon::Light>();
        switch (item.type) {
            case Moon::CSG::LightItem::Type::Directional: light->SetType(Moon::Light::Type::Directional); break;
            case Moon::CSG::LightItem::Type::Point:       light->SetType(Moon::Light::Type::Point); break;
            case Moon::CSG::LightItem::Type::Spot:        light->SetType(Moon::Light::Type::Spot); break;
        }
        light->SetColor(item.color);
        light->SetIntensity(item.intensity);

        if (light->GetType() == Moon::Light::Type::Point || light->GetType() == Moon::Light::Type::Spot) {
            light->SetRange(item.range);
            light->SetAttenuation(item.attenuation.x, item.attenuation.y, item.attenuation.z);
        }
        if (light->GetType() == Moon::Light::Type::Spot) {
            light->SetSpotAngles(item.spotInnerConeAngle, item.spotOuterConeAngle);
        }

        light->SetCastShadows(item.castShadows);
    }
    
    MOON_LOG_INFO("BuildingV8", "=== Building System V8 test complete ===");
    MOON_LOG_INFO("BuildingV8", "Building is now visible in the scene!");
}

} // namespace TestScenes
