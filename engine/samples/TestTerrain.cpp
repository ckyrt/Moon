#include "TestScenes.h"

#include <EngineCore.h>
#include <Logging/Logger.h>
#include <Scene/Light.h>
#include <Scene/Material.h>
#include <Scene/MeshRenderer.h>
#include <Scene/Scene.h>
#include <Scene/SceneNode.h>

#include "../environment/EnvironmentComponent.h"
#include "../terrain/ProceduralTerrainGenerator.h"
#include "../terrain/TerrainComponent.h"
#include "../terrain/TerrainVisualBuilder.h"

namespace TestScenes {

void TestTerrain(EngineCore* engine)
{
    if (!engine) {
        return;
    }

    Moon::Scene* scene = engine->GetScene();
    Moon::PerspectiveCamera* camera = engine->GetCamera();
    if (!scene || !camera) {
        return;
    }

    MOON_LOG_INFO("TestTerrain", "Creating open-world terrain showcase scene...");

    camera->SetPosition(Moon::Vector3(-210.0f, 120.0f, -360.0f));
    camera->LookAt(Moon::Vector3(40.0f, 35.0f, 10.0f));
    camera->SetFarPlane(3000.0f);

    Moon::SceneNode* environmentNode = scene->CreateNode("Terrain Environment");
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

    Moon::SceneNode* sunNode = scene->CreateNode("Environment Sun");
    Moon::Light* sun = sunNode->AddComponent<Moon::Light>();
    sun->SetType(Moon::Light::Type::Directional);
    sun->SetCastShadows(true);
    sun->SetIntensity(1.25f);

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

    Moon::SceneNode* terrainNode = scene->CreateNode("Terrain Runtime");
    Moon::TerrainComponent* terrain = terrainNode->AddComponent<Moon::TerrainComponent>();
    Moon::TerrainProfile terrainProfile;
    terrainProfile.name = "OpenWorldTerrain";
    terrainProfile.chunkResolutionQuads = 32;
    terrainProfile.chunkWorldSize = 175.0f;
    terrainProfile.heightScale = generationSettings.heightScale;
    terrain->SetProfile(terrainProfile);
    terrain->SetData(generation.terrainData);

    Moon::SceneNode* terrainMeshNode = scene->CreateNode("Terrain Mesh");
    Moon::MeshRenderer* terrainRenderer = terrainMeshNode->AddComponent<Moon::MeshRenderer>();
    Moon::Material* terrainMaterial = terrainMeshNode->AddComponent<Moon::Material>();
    terrainRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildTerrainMesh(generation.terrainData, generationSettings));
    terrainMaterial->SetBaseColor(Moon::Vector3(0.47f, 0.58f, 0.34f));
    terrainMaterial->SetMetallic(0.01f);
    terrainMaterial->SetRoughness(0.96f);

    Moon::SceneNode* riverNode = scene->CreateNode("Terrain River");
    Moon::MeshRenderer* riverRenderer = riverNode->AddComponent<Moon::MeshRenderer>();
    Moon::Material* riverMaterial = riverNode->AddComponent<Moon::Material>();
    riverRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildRiverMesh(generation, generationSettings));
    riverMaterial->SetBaseColor(Moon::Vector3(0.17f, 0.39f, 0.58f));
    riverMaterial->SetMetallic(0.08f);
    riverMaterial->SetRoughness(0.10f);
    riverMaterial->SetOpacity(0.78f);

    Moon::SceneNode* grassNode = scene->CreateNode("Terrain Grass");
    Moon::MeshRenderer* grassRenderer = grassNode->AddComponent<Moon::MeshRenderer>();
    Moon::Material* grassMaterial = grassNode->AddComponent<Moon::Material>();
    grassRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildGrassMesh(generation.terrainData, generation, generationSettings));
    grassMaterial->SetBaseColor(Moon::Vector3(0.33f, 0.55f, 0.21f));
    grassMaterial->SetMetallic(0.0f);
    grassMaterial->SetRoughness(0.88f);

    MOON_LOG_INFO(
        "TestTerrain",
        "Open-world terrain ready: %u x %u samples, %u x %u chunks, river points=%zu",
        terrain->GetData().heightmap.GetWidth(),
        terrain->GetData().heightmap.GetHeight(),
        terrain->GetRuntimeState().chunkCountX,
        terrain->GetRuntimeState().chunkCountZ,
        generation.riverPolyline.size() / 3);
}

} // namespace TestScenes
