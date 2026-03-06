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
#include <fstream>
#include <sstream>

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

void TestBuildingV8(EngineCore* engine)
{
    MOON_LOG_INFO("BuildingV8", "=== Testing Building System V8 with CSG ===");
    
    // Read Building System V8 JSON input
    std::string jsonContent = ReadFileToString("assets/building/residential/luxury_villa.json");
    if (jsonContent.empty()) {
        MOON_LOG_ERROR("BuildingV8", "Failed to read luxury_villa.json");
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
    std::string blueprintJsonStr = Moon::Building::BuildingToCSGConverter::Convert(result);

    // Save CSG Blueprint to file
    std::string outputPath = "assets/csg/generated_building_v8.json";
    std::ofstream outFile(outputPath);
    if (outFile.is_open()) {
        outFile << blueprintJsonStr;
        outFile.close();
        MOON_LOG_INFO("BuildingV8", "Saved CSG Blueprint to: %s", outputPath.c_str());
        MOON_LOG_INFO("BuildingV8", "CSG Blueprint size: %zu bytes", blueprintJsonStr.size());
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
