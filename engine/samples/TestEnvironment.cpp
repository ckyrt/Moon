#include "TestScenes.h"

#include <EngineCore.h>
#include <Logging/Logger.h>
#include <Scene/Component.h>
#include <Scene/Light.h>
#include <Scene/Material.h>
#include <Scene/MeshRenderer.h>
#include <Scene/Scene.h>
#include <Scene/SceneNode.h>

#include "../environment/EnvironmentComponent.h"

namespace {

class DemoEnvironmentDriver final : public Moon::Component {
public:
    explicit DemoEnvironmentDriver(Moon::SceneNode* owner)
        : Component(owner) {
    }

    void SetEnvironment(Moon::EnvironmentComponent* environment) {
        m_environment = environment;
        ApplyCurrentWeather(0.0f);
    }

    void Update(float deltaTime) override {
        if (!m_environment) {
            return;
        }

        m_elapsedSeconds += deltaTime;
        if (m_elapsedSeconds < m_holdDurationSeconds) {
            return;
        }

        m_elapsedSeconds = 0.0f;
        m_index = (m_index + 1) % kWeatherCount;
        ApplyCurrentWeather(8.0f);
    }

private:
    void ApplyCurrentWeather(float transitionSeconds) {
        m_environment->SetWeather(kWeatherSequence[m_index], transitionSeconds);
        MOON_LOG_INFO("TestEnvironment", "Weather switched to index %d", m_index);
    }

    Moon::EnvironmentComponent* m_environment = nullptr;
    float m_elapsedSeconds = 0.0f;
    float m_holdDurationSeconds = 18.0f;
    int m_index = 0;

    static constexpr Moon::WeatherType kWeatherSequence[] = {
        Moon::WeatherType::Clear,
        Moon::WeatherType::Cloudy,
        Moon::WeatherType::Rain,
        Moon::WeatherType::Fog,
        Moon::WeatherType::Storm
    };
    static constexpr int kWeatherCount = 5;
};

constexpr Moon::WeatherType DemoEnvironmentDriver::kWeatherSequence[];

void CreateProp(
    Moon::Scene* scene,
    Moon::MeshManager* meshManager,
    const char* name,
    const std::shared_ptr<Moon::Mesh>& mesh,
    const Moon::Vector3& position,
    const Moon::Vector3& scale,
    Moon::MaterialPreset preset)
{
    Moon::SceneNode* node = scene->CreateNode(name);
    node->GetTransform()->SetLocalPosition(position);
    node->GetTransform()->SetLocalScale(scale);

    Moon::MeshRenderer* renderer = node->AddComponent<Moon::MeshRenderer>();
    renderer->SetMesh(mesh);

    Moon::Material* material = node->AddComponent<Moon::Material>();
    material->SetMaterialPreset(preset);
    material->SetMappingMode(Moon::MappingMode::UV);
}

} // namespace

namespace TestScenes {

void TestEnvironment(EngineCore* engine)
{
    if (!engine) {
        return;
    }

    Moon::Scene* scene = engine->GetScene();
    Moon::MeshManager* meshManager = engine->GetMeshManager();
    Moon::PerspectiveCamera* camera = engine->GetCamera();
    if (!scene || !meshManager || !camera) {
        return;
    }

    MOON_LOG_INFO("TestEnvironment", "Creating dynamic environment showcase scene...");

    camera->SetPosition(Moon::Vector3(0.0f, 4.5f, -12.0f));
    camera->LookAt(Moon::Vector3(0.0f, 2.0f, 4.0f));

    Moon::SceneNode* environmentNode = scene->CreateNode("Environment Controller");
    Moon::EnvironmentComponent* environment = environmentNode->AddComponent<Moon::EnvironmentComponent>();

    Moon::EnvironmentProfile profile;
    profile.name = "EnvironmentShowcase";
    profile.enableDayNightCycle = true;
    profile.enableWeather = true;
    profile.enableWind = true;
    profile.enableClouds = true;
    profile.enableFog = true;
    profile.syncPrimaryDirectionalLight = true;
    profile.minSunIntensity = 0.04f;
    profile.maxSunIntensity = 1.8f;
    profile.clearFogDensity = 0.0012f;
    profile.fogWeatherDensity = 0.018f;
    environment->SetProfile(profile);
    environment->SetTimeOfDay(8.5f);
    environment->SetWeather(Moon::WeatherType::Clear, 0.0f);

    DemoEnvironmentDriver* driver = environmentNode->AddComponent<DemoEnvironmentDriver>();
    driver->SetEnvironment(environment);

    Moon::SceneNode* sunNode = scene->CreateNode("Environment Sun");
    Moon::Light* sun = sunNode->AddComponent<Moon::Light>();
    sun->SetType(Moon::Light::Type::Directional);
    sun->SetCastShadows(true);
    sun->SetIntensity(1.2f);

    Moon::SceneNode* groundNode = scene->CreateNode("Environment Ground");
    Moon::MeshRenderer* groundRenderer = groundNode->AddComponent<Moon::MeshRenderer>();
    groundRenderer->SetMesh(meshManager->CreatePlane(80.0f, 80.0f, 1, 1, Moon::Vector3(0.5f, 0.5f, 0.5f)));
    Moon::Material* groundMaterial = groundNode->AddComponent<Moon::Material>();
    groundMaterial->SetMaterialPreset(Moon::MaterialPreset::Concrete);
    groundMaterial->SetMappingMode(Moon::MappingMode::UV);

    CreateProp(scene, meshManager, "Weather Sphere", meshManager->CreateSphere(1.0f, 32, 16), Moon::Vector3(-4.0f, 1.0f, 4.0f), Moon::Vector3(1.0f, 1.0f, 1.0f), Moon::MaterialPreset::Metal);
    CreateProp(scene, meshManager, "Weather Cube", meshManager->CreateCube(1.0f), Moon::Vector3(0.0f, 1.0f, 6.0f), Moon::Vector3(2.0f, 2.0f, 2.0f), Moon::MaterialPreset::Wood);
    CreateProp(scene, meshManager, "Weather Tower", meshManager->CreateCube(1.0f), Moon::Vector3(5.0f, 2.5f, 5.0f), Moon::Vector3(1.5f, 5.0f, 1.5f), Moon::MaterialPreset::Plastic);

    MOON_LOG_INFO("TestEnvironment", "Environment showcase scene ready");
}

} // namespace TestScenes
