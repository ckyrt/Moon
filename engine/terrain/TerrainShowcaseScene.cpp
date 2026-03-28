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
constexpr const char* kShrubNodeName = "Terrain Shrubs";

float ResolveTimeOfDayHours(const std::string& timeOfDay)
{
    if (timeOfDay == "dawn") {
        return 5.5f;
    }
    if (timeOfDay == "sunrise") {
        return 6.2f;
    }
    if (timeOfDay == "morning") {
        return 8.8f;
    }
    if (timeOfDay == "noon") {
        return 12.0f;
    }
    if (timeOfDay == "afternoon") {
        return 15.5f;
    }
    if (timeOfDay == "sunset") {
        return 18.2f;
    }
    if (timeOfDay == "dusk") {
        return 19.1f;
    }
    if (timeOfDay == "night") {
        return 22.0f;
    }
    return 10.2f;
}

Moon::WeatherType ResolveWeatherType(const std::string& weather)
{
    if (weather == "cloudy" || weather == "overcast") {
        return Moon::WeatherType::Cloudy;
    }
    if (weather == "rain" || weather == "drizzle") {
        return Moon::WeatherType::Rain;
    }
    if (weather == "snow" || weather == "sleet" || weather == "blizzard") {
        return Moon::WeatherType::Snow;
    }
    if (weather == "fog" || weather == "mist") {
        return Moon::WeatherType::Fog;
    }
    if (weather == "storm" || weather == "thunderstorm") {
        return Moon::WeatherType::Storm;
    }
    return Moon::WeatherType::Clear;
}

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
        camera->SetPosition(Moon::Vector3(-140.0f, 88.0f, -235.0f));
        camera->LookAt(Moon::Vector3(55.0f, 28.0f, 70.0f));
        camera->SetFarPlane(6000.0f);
    }

    if (options.createEnvironment && !scene->FindNodeByName(kEnvironmentNodeName)) {
        Moon::SceneNode* environmentNode = scene->CreateNode(kEnvironmentNodeName);
        Moon::EnvironmentComponent* environment = environmentNode->AddComponent<Moon::EnvironmentComponent>();

        Moon::EnvironmentProfile environmentProfile;
        environmentProfile.name = "OpenWorldTerrain";
        environmentProfile.enableClouds = true;
        environmentProfile.enableFog = true;
        environmentProfile.enableWind = true;
        environmentProfile.lockToFixedTime = true;
        environmentProfile.enableDayNightCycle = false;
        environmentProfile.fixedTimeHours = ResolveTimeOfDayHours(buildSpec.atmosphere.timeOfDay);
        environmentProfile.maxSunIntensity = 1.85f;
        environmentProfile.clearFogDensity = 0.00008f + buildSpec.atmosphere.fog * 0.00038f;
        environmentProfile.fogWeatherDensity = 0.0010f + buildSpec.atmosphere.fog * 0.0065f;
        environmentProfile.cloudyCloudCoverage = 0.42f;
        environmentProfile.baseWindSpeed = 1.4f + buildSpec.atmosphere.wind * 5.2f;
        environmentProfile.baseWindGustStrength = 0.08f + buildSpec.atmosphere.wind * 0.24f;
        environmentProfile.baseWindTurbulence = 0.03f + buildSpec.atmosphere.wind * 0.11f;
        environment->SetProfile(environmentProfile);
        environment->SetTimeOfDay(ResolveTimeOfDayHours(buildSpec.atmosphere.timeOfDay));
        environment->SetWeather(ResolveWeatherType(buildSpec.atmosphere.weather), 0.0f);

        Moon::EnvironmentState state = environment->GetState();
        state.timeOfDay.paused = true;
        state.timeOfDay.timeScale = 0.0f;
        state.wind.direction = Moon::Vector3(0.85f, 0.0f, 0.35f).Normalized();
        environment->GetSystem().SetState(state);
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
    terrainMaterial->SetTriplanarTiling(0.075f);
    terrainMaterial->SetTriplanarBlend(5.2f);
    terrainMaterial->SetUseVertexColorTint(true);

    Moon::SceneNode* riverNode = scene->CreateNode(kRiverNodeName);
    Moon::MeshRenderer* riverRenderer = riverNode->AddComponent<Moon::MeshRenderer>();
    Moon::Material* riverMaterial = riverNode->AddComponent<Moon::Material>();
    riverRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildRiverMesh(generation, generationSettings));
    riverMaterial->SetBaseColor(Moon::Vector3(0.17f, 0.39f, 0.58f));
    riverMaterial->SetMetallic(0.08f);
    riverMaterial->SetRoughness(0.10f);
    riverMaterial->SetOpacity(0.78f);
    riverMaterial->SetShadingModel(Moon::ShadingModel::Water);
    riverMaterial->SetTransmissionColor(Moon::Vector3(0.72f, 0.88f, 0.96f));

    if (generationSettings.hasOcean) {
        Moon::SceneNode* oceanNode = scene->CreateNode(kOceanNodeName);
        Moon::MeshRenderer* oceanRenderer = oceanNode->AddComponent<Moon::MeshRenderer>();
        Moon::Material* oceanMaterial = oceanNode->AddComponent<Moon::Material>();
        oceanRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildOceanMesh(generation, generationSettings));
        oceanMaterial->SetBaseColor(Moon::Vector3(0.18f, 0.46f, 0.68f));
        oceanMaterial->SetMetallic(0.02f);
        oceanMaterial->SetRoughness(0.06f);
        oceanMaterial->SetOpacity(0.84f);
        oceanMaterial->SetShadingModel(Moon::ShadingModel::Water);
        oceanMaterial->SetTransmissionColor(Moon::Vector3(0.74f, 0.90f, 0.98f));
    }

    Moon::SceneNode* grassNode = scene->CreateNode(kGrassNodeName);
    Moon::MeshRenderer* grassRenderer = grassNode->AddComponent<Moon::MeshRenderer>();
    Moon::Material* grassMaterial = grassNode->AddComponent<Moon::Material>();
    grassRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildGrassMesh(generation.terrainData, generation, generationSettings));
    grassMaterial->SetBaseColor(Moon::Vector3(0.33f, 0.55f, 0.21f));
    grassMaterial->SetMetallic(0.0f);
    grassMaterial->SetRoughness(0.88f);
    grassMaterial->SetUseVertexColorTint(true);

    Moon::SceneNode* shrubNode = scene->CreateNode(kShrubNodeName);
    Moon::MeshRenderer* shrubRenderer = shrubNode->AddComponent<Moon::MeshRenderer>();
    Moon::Material* shrubMaterial = shrubNode->AddComponent<Moon::Material>();
    shrubRenderer->SetMesh(Moon::TerrainVisualBuilder::BuildShrubMesh(generation.terrainData, generation, generationSettings));
    shrubMaterial->SetBaseColor(Moon::Vector3(0.30f, 0.46f, 0.20f));
    shrubMaterial->SetMetallic(0.0f);
    shrubMaterial->SetRoughness(0.92f);
    shrubMaterial->SetUseVertexColorTint(true);

}

} // namespace Moon
