#include "TestScenes.h"
#include <Logging/Logger.h>
#include <EngineCore.h>
#include <Scene/Scene.h>
#include <Scene/SceneNode.h>
#include <Scene/MeshRenderer.h>
#include <Scene/Material.h>
#include <Scene/Light.h>
#include <Mesh/Mesh.h>
#include <Assets/AssetPaths.h>
#include <Building.h>
#include <CSG/Blueprint.h>
#include <CSG/BlueprintLoader.h>
#include <CSG/CSGBuilder.h>
#include <fstream>
#include <unordered_map>

namespace TestScenes {

void TestBuildingSample(EngineCore* engine)
{
    MOON_LOG_INFO("BuildingSample", "=== Testing semantic building sample with CSG ===");
    MOON_LOG_INFO("BuildingSample", "Using fixed asset root: %s", Moon::Assets::kAssetRootPath);

    const std::string semanticFilePath = Moon::Assets::BuildBuildingPath("test_building.json");
    std::ifstream semanticCheck(semanticFilePath);
    if (!semanticCheck.good()) {
        MOON_LOG_ERROR("BuildingSample", "No semantic building input available. Expected test_building.json.");
        return;
    }
    semanticCheck.close();

    Moon::Building::GeneratedBuilding result;
    Moon::Building::BestEffortGenerationReport report;
    std::string error;

    std::ifstream semanticFile(semanticFilePath);
    if (!semanticFile.is_open()) {
        MOON_LOG_ERROR("BuildingSample", "Failed to open semantic JSON: %s", semanticFilePath.c_str());
        return;
    }

    std::string semanticJson((std::istreambuf_iterator<char>(semanticFile)),
                             std::istreambuf_iterator<char>());

    Moon::Building::BuildingPipeline pipeline;
    if (!pipeline.ProcessBuildingBestEffort(semanticJson, result, report, error)) {
        MOON_LOG_ERROR("BuildingSample", "%s", error.c_str());
        return;
    }

    if (report.usedBestEffort) {
        MOON_LOG_WARN("BuildingSample", "Building generated in best-effort mode. Skipped %zu spaces.",
                      report.skippedSpaces.size());
        for (const auto& note : report.repairNotes) {
            MOON_LOG_WARN("BuildingSample", "  Repair: %s", note.c_str());
        }
        for (const auto& adjustedSpace : report.adjustedSpaces) {
            MOON_LOG_WARN("BuildingSample",
                          "  Floor %d space '%s' (%s) repaired: %s",
                          adjustedSpace.floorLevel,
                          adjustedSpace.spaceId.c_str(),
                          adjustedSpace.spaceType.c_str(),
                          adjustedSpace.reason.c_str());
        }
        for (const auto& skippedSpace : report.skippedSpaces) {
            MOON_LOG_ERROR("BuildingSample",
                           "  Floor %d space '%s' (%s) skipped: %s",
                           skippedSpace.floorLevel,
                           skippedSpace.spaceId.c_str(),
                           skippedSpace.spaceType.c_str(),
                           skippedSpace.reason.c_str());
        }
    }

    MOON_LOG_INFO("BuildingSample", "Building generated successfully:");
    MOON_LOG_INFO("BuildingSample", "  - Walls: %zu", result.walls.size());
    MOON_LOG_INFO("BuildingSample", "  - Doors: %zu", result.doors.size());
    MOON_LOG_INFO("BuildingSample", "  - Windows: %zu", result.windows.size());
    MOON_LOG_INFO("BuildingSample", "  - Connections: %zu", result.connections.size());

    MOON_LOG_INFO("BuildingSample", "Converting to CSG Blueprint...");
    std::string blueprintJsonStr = Moon::Building::BuildingToCSGConverter::Convert(result);

    const std::string outputPath = Moon::Assets::BuildCsgPath("generated_building.json");
    std::ofstream outFile(outputPath);
    if (outFile.is_open()) {
        outFile << blueprintJsonStr;
        outFile.close();
        MOON_LOG_INFO("BuildingSample", "Saved CSG Blueprint to: %s", outputPath.c_str());
        MOON_LOG_INFO("BuildingSample", "CSG Blueprint size: %zu bytes", blueprintJsonStr.size());
    } else {
        MOON_LOG_ERROR("BuildingSample", "Failed to save CSG Blueprint to %s", outputPath.c_str());
        return;
    }

    MOON_LOG_INFO("BuildingSample", "Loading generated CSG Blueprint...");

    Moon::Scene* scene = engine->GetScene();
    Moon::CSG::BlueprintDatabase database;
    std::string loadError;

    const std::string csgIndexPath = Moon::Assets::BuildCsgPath("index.json");
    if (!database.LoadIndex(csgIndexPath, loadError)) {
        MOON_LOG_ERROR("BuildingSample", "Failed to load CSG index: %s", loadError.c_str());
        return;
    }

    auto generatedBlueprint = Moon::CSG::BlueprintLoader::ParseFromString(blueprintJsonStr, loadError);
    if (!generatedBlueprint) {
        MOON_LOG_ERROR("BuildingSample", "Failed to parse generated blueprint: %s", loadError.c_str());
        return;
    }

    Moon::CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(&database);

    std::unordered_map<std::string, float> params;
    Moon::CSG::BuildResult buildResult = builder.Build(generatedBlueprint.get(), params, loadError);

    if (buildResult.meshes.empty()) {
        MOON_LOG_ERROR("BuildingSample", "Failed to build CSG scene: %s", loadError.c_str());
        return;
    }

    MOON_LOG_INFO("BuildingSample", "CSG scene built successfully: %d meshes", (int)buildResult.meshes.size());

    for (size_t i = 0; i < buildResult.meshes.size(); ++i) {
        const auto& item = buildResult.meshes[i];
        if (item.mesh && item.mesh->IsValid()) {
            auto node = scene->CreateNode("building_part_" + std::to_string(i));

            node->GetTransform()->SetLocalPosition(item.worldTransform.position);
            node->GetTransform()->SetLocalRotation(item.worldTransform.rotation);
            node->GetTransform()->SetLocalScale(item.worldTransform.scale);

            auto renderer = node->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(item.mesh);

            auto mat = node->AddComponent<Moon::Material>();
            const std::string& matName = item.material;

            if (matName == "concrete")              { mat->SetMaterialPreset(Moon::MaterialPreset::Concrete); }
            else if (matName == "concrete_floor")   { mat->SetMaterialPreset(Moon::MaterialPreset::ConcreteFloor); }
            else if (matName == "concrete_polished"){ mat->SetMaterialPreset(Moon::MaterialPreset::ConcretePolished); }
            else if (matName == "rock" || matName == "stone") { mat->SetMaterialPreset(Moon::MaterialPreset::Rock); }
            else if (matName == "brick")            { mat->SetMaterialPreset(Moon::MaterialPreset::Brick); }
            else if (matName == "plaster")          { mat->SetMaterialPreset(Moon::MaterialPreset::Plaster); }
            else if (matName == "tile_ceramic")     { mat->SetMaterialPreset(Moon::MaterialPreset::TileCeramic); }
            else if (matName == "wood")             { mat->SetMaterialPreset(Moon::MaterialPreset::Wood); }
            else if (matName == "wood_floor")       { mat->SetMaterialPreset(Moon::MaterialPreset::WoodFloor); }
            else if (matName == "wood_polished")    { mat->SetMaterialPreset(Moon::MaterialPreset::WoodPolished); }
            else if (matName == "wood_painted")     { mat->SetMaterialPreset(Moon::MaterialPreset::WoodPainted); }
            else if (matName == "metal")            { mat->SetMaterialPreset(Moon::MaterialPreset::Metal); }
            else if (matName == "steel")            { mat->SetMaterialPreset(Moon::MaterialPreset::Steel); }
            else if (matName == "aluminum")         { mat->SetMaterialPreset(Moon::MaterialPreset::Aluminum); }
            else if (matName == "chrome")           { mat->SetMaterialPreset(Moon::MaterialPreset::Chrome); }
            else if (matName == "copper")           { mat->SetMaterialPreset(Moon::MaterialPreset::Copper); }
            else if (matName == "glass")            { mat->SetMaterialPreset(Moon::MaterialPreset::Glass); }
            else if (matName == "glass_frosted")    { mat->SetMaterialPreset(Moon::MaterialPreset::GlassFrosted); }
            else if (matName == "glass_tinted")     { mat->SetMaterialPreset(Moon::MaterialPreset::GlassTinted); }
            else if (matName == "fabric")           { mat->SetMaterialPreset(Moon::MaterialPreset::Fabric); }
            else if (matName == "leather")          { mat->SetMaterialPreset(Moon::MaterialPreset::Leather); }
            else if (matName == "carpet")           { mat->SetMaterialPreset(Moon::MaterialPreset::Carpet); }
            else if (matName == "plastic")          { mat->SetMaterialPreset(Moon::MaterialPreset::Plastic); }
            else if (matName == "rubber")           { mat->SetMaterialPreset(Moon::MaterialPreset::Rubber); }
            else if (!matName.empty()) {
                MOON_LOG_WARN("BuildingSample", "Unknown material '%s'", matName.c_str());
            }

            mat->SetMappingMode(Moon::MappingMode::Triplanar);
        }
    }

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

    MOON_LOG_INFO("BuildingSample", "=== Semantic building sample test complete ===");
    MOON_LOG_INFO("BuildingSample", "Building is now visible in the scene!");
}

} // namespace TestScenes