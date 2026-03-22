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

namespace Moon {

namespace {

constexpr const char* kEnvironmentNodeName = "Terrain Environment";
constexpr const char* kSunNodeName = "Environment Sun";
constexpr const char* kRuntimeNodeName = "Terrain Runtime";
constexpr const char* kTerrainMeshNodeName = "Terrain Mesh";
constexpr const char* kRiverNodeName = "Terrain River";
constexpr const char* kGrassNodeName = "Terrain Grass";

} // namespace

void TerrainShowcaseScene::BuildOpenWorldScene(EngineCore* engine, const TerrainShowcaseOptions& options)
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
        environmentProfile.enableFog = true;
        environmentProfile.enableWind = true;
        environmentProfile.maxSunIntensity = 1.55f;
        environmentProfile.clearFogDensity = 0.0013f;
        environmentProfile.fogWeatherDensity = 0.010f;
        environmentProfile.cloudyCloudCoverage = 0.52f;
        environment->SetProfile(environmentProfile);
        environment->SetTimeOfDay(8.3f);
        environment->SetWeather(Moon::WeatherType::Clear, 0.0f);
    }

    if (options.createSunLight && !scene->FindNodeByName(kSunNodeName)) {
        Moon::SceneNode* sunNode = scene->CreateNode(kSunNodeName);
        Moon::Light* sun = sunNode->AddComponent<Moon::Light>();
        sun->SetType(Moon::Light::Type::Directional);
        sun->SetCastShadows(true);
        sun->SetIntensity(1.25f);
    }

    Moon::TerrainGenerationSettings generationSettings;
    generationSettings.resolution = 257;
    generationSettings.worldWidth = 1400.0f;
    generationSettings.worldDepth = 1400.0f;
    generationSettings.heightScale = 180.0f;
    generationSettings.riverWidth = 44.0f;
    generationSettings.riverDepth = 4.5f;
    generationSettings.grassClusterBudget = 2600;

    const Moon::TerrainGenerationResult generation =
        Moon::ProceduralTerrainGenerator::CreateOpenWorldLandscape(generationSettings);

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
            generation.riverPolyline.size() / 3);
    }

    Moon::SceneNode* terrainMeshNode = scene->CreateNode(kTerrainMeshNodeName);
    Moon::MeshRenderer* terrainRenderer = terrainMeshNode->AddComponent<Moon::MeshRenderer>();
    Moon::Material* terrainMaterial = terrainMeshNode->AddComponent<Moon::Material>();
    terrainRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildTerrainMesh(generation.terrainData, generationSettings));
    terrainMaterial->SetBaseColor(Moon::Vector3(0.47f, 0.58f, 0.34f));
    terrainMaterial->SetMetallic(0.01f);
    terrainMaterial->SetRoughness(0.96f);

    Moon::SceneNode* riverNode = scene->CreateNode(kRiverNodeName);
    Moon::MeshRenderer* riverRenderer = riverNode->AddComponent<Moon::MeshRenderer>();
    Moon::Material* riverMaterial = riverNode->AddComponent<Moon::Material>();
    riverRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildRiverMesh(generation, generationSettings));
    riverMaterial->SetBaseColor(Moon::Vector3(0.17f, 0.39f, 0.58f));
    riverMaterial->SetMetallic(0.08f);
    riverMaterial->SetRoughness(0.10f);
    riverMaterial->SetOpacity(0.78f);

    Moon::SceneNode* grassNode = scene->CreateNode(kGrassNodeName);
    Moon::MeshRenderer* grassRenderer = grassNode->AddComponent<Moon::MeshRenderer>();
    Moon::Material* grassMaterial = grassNode->AddComponent<Moon::Material>();
    grassRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildGrassMesh(generation.terrainData, generation, generationSettings));
    grassMaterial->SetBaseColor(Moon::Vector3(0.33f, 0.55f, 0.21f));
    grassMaterial->SetMetallic(0.0f);
    grassMaterial->SetRoughness(0.88f);
}

} // namespace Moon
