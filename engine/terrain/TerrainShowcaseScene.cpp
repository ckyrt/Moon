#include "TerrainShowcaseScene.h"

#include "../core/EngineCore.h"
#include "../core/Logging/Logger.h"
#include "../core/Scene/Light.h"
#include "../core/Scene/Material.h"
#include "../core/Scene/MeshRenderer.h"
#include "../core/Scene/Scene.h"
#include "../core/Scene/SceneNode.h"

#include "../environment/EnvironmentComponent.h"
#include "ProceduralTerrainGenerator.h"
#include "TerrainComponent.h"
#include "TerrainVisualBuilder.h"
#include "WorldSpec.h"

namespace Moon {

namespace {

constexpr const char* kEnvironmentNodeName = "Terrain Environment";
constexpr const char* kSunNodeName = "Environment Sun";
constexpr const char* kRuntimeNodeName = "Terrain Runtime";
constexpr const char* kTerrainMeshNodeName = "Terrain Mesh";
constexpr const char* kRiverNodeName = "Terrain River";
constexpr const char* kOceanNodeName = "Terrain Ocean";
constexpr const char* kGrassNodeName = "Terrain Grass";

} // namespace

void TerrainShowcaseScene::BuildOpenWorldScene(EngineCore* engine, const TerrainShowcaseOptions& options)
{
    BuildOpenWorldScene(engine, WorldSpecIO::BuildFromPrompt(WorldSpecIO::CreateDefaultPromptSpec()), options);
}

void TerrainShowcaseScene::BuildOpenWorldScene(EngineCore* engine, const WorldBuildSpec& buildSpec, const TerrainShowcaseOptions& options)
{
    if (!engine) {
        return;
    }

    Moon::Scene* scene = engine->GetScene();
    Moon::PerspectiveCamera* camera = engine->GetCamera();
    if (!scene || !camera) {
        return;
    }

    if (scene->FindNodeByName(kRuntimeNodeName)) {
        MOON_LOG_INFO("TerrainShowcaseScene", "Open-world terrain scene already exists, skipping rebuild.");
        return;
    }

    MOON_LOG_INFO("TerrainShowcaseScene", "Building open-world terrain showcase scene...");

    if (options.configureCamera) {
        camera->SetPosition(Moon::Vector3(-210.0f, 120.0f, -360.0f));
        camera->LookAt(Moon::Vector3(40.0f, 35.0f, 10.0f));
        camera->SetFarPlane(3000.0f);
    }

    if (options.createEnvironment && !scene->FindNodeByName(kEnvironmentNodeName)) {
        Moon::SceneNode* environmentNode = scene->CreateNode(kEnvironmentNodeName);
        Moon::EnvironmentComponent* environment = environmentNode->AddComponent<Moon::EnvironmentComponent>();

        Moon::EnvironmentProfile environmentProfile;
        environmentProfile.name = "OpenWorldTerrain";
        environmentProfile.enableClouds = true;
        environmentProfile.enableFog = false;
        environmentProfile.enableWind = true;
        environmentProfile.maxSunIntensity = 1.85f;
        environmentProfile.clearFogDensity = 0.00015f;
        environmentProfile.fogWeatherDensity = 0.0025f;
        environmentProfile.cloudyCloudCoverage = 0.10f;
        environment->SetProfile(environmentProfile);
        environment->SetTimeOfDay(10.5f);
        environment->SetWeather(Moon::WeatherType::Clear, 0.0f);
    }

    if (options.createSunLight && !scene->FindNodeByName(kSunNodeName)) {
        Moon::SceneNode* sunNode = scene->CreateNode(kSunNodeName);
        Moon::Light* sun = sunNode->AddComponent<Moon::Light>();
        sun->SetType(Moon::Light::Type::Directional);
        sun->SetCastShadows(true);
        sun->SetIntensity(1.25f);
    }

    const Moon::TerrainGenerationSettings generationSettings =
        Moon::ProceduralTerrainGenerator::CreateSettingsFromWorldBuildSpec(buildSpec);

    const Moon::TerrainGenerationResult generation =
        Moon::ProceduralTerrainGenerator::CreateFromWorldBuildSpec(buildSpec);

    if (options.createTerrainRuntime) {
        Moon::SceneNode* terrainNode = scene->CreateNode(kRuntimeNodeName);
        Moon::TerrainComponent* terrain = terrainNode->AddComponent<Moon::TerrainComponent>();
        Moon::TerrainProfile terrainProfile;
        terrainProfile.name = "OpenWorldTerrain";
        terrainProfile.chunkResolutionQuads = 32;
        terrainProfile.chunkWorldSize = 175.0f;
        terrainProfile.heightScale = generationSettings.heightScale;
        terrain->SetProfile(terrainProfile);
        terrain->SetData(generation.terrainData);

        MOON_LOG_INFO(
            "TerrainShowcaseScene",
            "Open-world terrain ready: %u x %u samples, %u x %u chunks, river points=%zu",
            terrain->GetData().heightmap.GetWidth(),
            terrain->GetData().heightmap.GetHeight(),
            terrain->GetRuntimeState().chunkCountX,
            terrain->GetRuntimeState().chunkCountZ,
            generation.riverPolylines.empty() ? 0 : generation.riverPolylines.front().size() / 3);
    }

    Moon::SceneNode* terrainMeshNode = scene->CreateNode(kTerrainMeshNodeName);
    Moon::MeshRenderer* terrainRenderer = terrainMeshNode->AddComponent<Moon::MeshRenderer>();
    Moon::Material* terrainMaterial = terrainMeshNode->AddComponent<Moon::Material>();
    terrainRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildTerrainMesh(generation, generationSettings));
    terrainMaterial->SetMaterialPreset(Moon::MaterialPreset::Rock);
    terrainMaterial->SetBaseColor(Moon::Vector3(1.0f, 1.0f, 1.0f));
    terrainMaterial->SetMappingMode(Moon::MappingMode::Triplanar);
    terrainMaterial->SetTriplanarTiling(0.12f);
    terrainMaterial->SetTriplanarBlend(6.0f);
    terrainMaterial->SetUseVertexColorTint(true);

    Moon::SceneNode* riverNode = scene->CreateNode(kRiverNodeName);
    Moon::MeshRenderer* riverRenderer = riverNode->AddComponent<Moon::MeshRenderer>();
    Moon::Material* riverMaterial = riverNode->AddComponent<Moon::Material>();
    riverRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildRiverMesh(generation, generationSettings));
    riverMaterial->SetBaseColor(Moon::Vector3(0.17f, 0.39f, 0.58f));
    riverMaterial->SetMetallic(0.08f);
    riverMaterial->SetRoughness(0.10f);
    riverMaterial->SetOpacity(0.78f);

    if (generationSettings.hasOcean) {
        Moon::SceneNode* oceanNode = scene->CreateNode(kOceanNodeName);
        Moon::MeshRenderer* oceanRenderer = oceanNode->AddComponent<Moon::MeshRenderer>();
        Moon::Material* oceanMaterial = oceanNode->AddComponent<Moon::Material>();
        oceanRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildOceanMesh(generation, generationSettings));
        oceanMaterial->SetBaseColor(Moon::Vector3(0.18f, 0.46f, 0.68f));
        oceanMaterial->SetMetallic(0.02f);
        oceanMaterial->SetRoughness(0.06f);
        oceanMaterial->SetOpacity(0.84f);
    }

    Moon::SceneNode* grassNode = scene->CreateNode(kGrassNodeName);
    Moon::MeshRenderer* grassRenderer = grassNode->AddComponent<Moon::MeshRenderer>();
    Moon::Material* grassMaterial = grassNode->AddComponent<Moon::Material>();
    grassRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildGrassMesh(generation.terrainData, generation, generationSettings));
    grassMaterial->SetBaseColor(Moon::Vector3(0.33f, 0.55f, 0.21f));
    grassMaterial->SetMetallic(0.0f);
    grassMaterial->SetRoughness(0.88f);
}

} // namespace Moon
