#include "ObjectCopilotPreview.h"

#include "../../engine/core/Assets/AssetPaths.h"
#include "../../engine/core/CSG/CSGBuilder.h"
#include "../../engine/core/EngineCore.h"
#include "../../engine/core/Geometry/MeshGenerator.h"
#include "../../engine/core/Mesh/Mesh.h"
#include "../../engine/core/Object/BlueprintLoader.h"
#include "../../engine/core/Scene/Light.h"
#include "../../engine/core/Scene/Material.h"
#include "../../engine/core/Scene/MeshRenderer.h"
#include "../../engine/core/Scene/Scene.h"
#include "../../engine/core/Scene/SceneNode.h"
#include "../../engine/environment/EnvironmentComponent.h"
#include "../../engine/environment/EnvironmentTypes.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>

namespace Moon {
namespace Tooling {

namespace {

Moon::MaterialPreset ParseMaterialPreset(const std::string& materialName) {
    if (materialName == "wood" || materialName == "wood_floor" || materialName == "wood_polished") {
        return Moon::MaterialPreset::Wood;
    }
    if (materialName == "metal" || materialName == "steel" || materialName == "chrome") {
        return Moon::MaterialPreset::Metal;
    }
    if (materialName == "glass" || materialName == "glass_tinted" || materialName == "glass_frosted") {
        return Moon::MaterialPreset::Glass;
    }
    if (materialName == "concrete") {
        return Moon::MaterialPreset::Concrete;
    }
    if (materialName == "plastic" || materialName == "rubber") {
        return Moon::MaterialPreset::Plastic;
    }
    if (materialName == "fabric" || materialName == "leather") {
        return Moon::MaterialPreset::Fabric;
    }
    return Moon::MaterialPreset::None;
}

} // namespace

ObjectCopilotPreview::ObjectCopilotPreview(EngineCore* engine)
    : m_engine(engine) {
}

bool ObjectCopilotPreview::RebuildFromObjectJson(const std::string& objectJson, std::string& outError) {
    if (!m_engine || !m_engine->GetScene()) {
        outError = "Engine or scene is not initialized";
        return false;
    }

    Moon::Object::BlueprintDatabase database;
    if (!database.LoadIndex(Moon::Assets::BuildObjectPath("index.json"), outError)) {
        return false;
    }

    auto blueprint = Moon::Object::BlueprintLoader::ParseFromString(objectJson, outError);
    if (!blueprint) {
        return false;
    }

    Moon::CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(&database);
    std::unordered_map<std::string, float> params;
    Moon::CSG::BuildResult buildResult = builder.Build(blueprint.get(), params, outError);
    if (buildResult.meshes.empty() && buildResult.lights.empty()) {
        if (outError.empty()) {
            outError = "Object preview produced no meshes or lights";
        }
        return false;
    }

    EnsurePreviewEnvironment();
    Clear();

    Moon::Scene* scene = m_engine->GetScene();
    m_previewRoot = scene->CreateNode("__ObjectCopilotPreview");

    for (size_t i = 0; i < buildResult.meshes.size(); ++i) {
        const auto& item = buildResult.meshes[i];
        Moon::SceneNode* node = scene->CreateNode("PreviewMesh_" + std::to_string(i));
        node->SetParent(m_previewRoot, false);
        node->GetTransform()->SetLocalPosition(item.worldTransform.position);
        node->GetTransform()->SetLocalRotation(item.worldTransform.rotation);
        node->GetTransform()->SetLocalScale(item.worldTransform.scale);

        Moon::MeshRenderer* renderer = node->AddComponent<Moon::MeshRenderer>();
        renderer->SetMesh(item.mesh);
        AddPreviewMaterial(node, item.material);
    }

    for (size_t i = 0; i < buildResult.lights.size(); ++i) {
        const auto& item = buildResult.lights[i];
        Moon::SceneNode* node = scene->CreateNode("PreviewLight_" + std::to_string(i));
        node->SetParent(m_previewRoot, false);
        node->GetTransform()->SetLocalPosition(item.worldTransform.position);
        node->GetTransform()->SetLocalRotation(item.worldTransform.rotation);
        node->GetTransform()->SetLocalScale(item.worldTransform.scale);

        Moon::Light* light = node->AddComponent<Moon::Light>();
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
        light->SetRange(item.range);
        light->SetAttenuation(item.attenuation.x, item.attenuation.y, item.attenuation.z);
        light->SetSpotAngles(item.spotInnerConeAngle, item.spotOuterConeAngle);
        light->SetCastShadows(item.castShadows);

        std::shared_ptr<Moon::Mesh> markerMesh(Moon::MeshGenerator::CreateSphere(0.08f, 12, 8, item.color));
        if (markerMesh && markerMesh->IsValid()) {
            Moon::MeshRenderer* renderer = node->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(markerMesh);
            Moon::Material* material = node->AddComponent<Moon::Material>();
            material->SetBaseColor(item.color);
            material->SetMetallic(0.0f);
            material->SetRoughness(0.2f);
        }
    }

    const Bounds3 bounds = ComputeBounds(buildResult);
    AddPreviewHelpers(bounds);
    UpdatePreviewStats(bounds);
    FocusCamera(bounds);
    return true;
}

void ObjectCopilotPreview::Clear() {
    if (!m_engine || !m_engine->GetScene() || !m_previewRoot) {
        m_previewRoot = nullptr;
        return;
    }

    m_engine->GetScene()->DestroyNodeImmediate(m_previewRoot);
    m_previewRoot = nullptr;
    m_previewStats = PreviewStats{};
}

void ObjectCopilotPreview::ExpandBounds(Bounds3& bounds, const Moon::Vector3& point) {
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

ObjectCopilotPreview::Bounds3 ObjectCopilotPreview::ComputeBounds(const Moon::CSG::BuildResult& buildResult) {
    Bounds3 bounds;
    for (const auto& item : buildResult.meshes) {
        if (!item.mesh) {
            continue;
        }

        for (const Moon::Vertex& vertex : item.mesh->GetVertices()) {
            const Moon::Vector3 scaled(
                vertex.position.x * item.worldTransform.scale.x,
                vertex.position.y * item.worldTransform.scale.y,
                vertex.position.z * item.worldTransform.scale.z);
            const Moon::Vector3 transformed = item.worldTransform.rotation * scaled + item.worldTransform.position;
            ExpandBounds(bounds, transformed);
        }
    }

    for (const auto& item : buildResult.lights) {
        ExpandBounds(bounds, item.worldTransform.position + Moon::Vector3(-0.1f, -0.1f, -0.1f));
        ExpandBounds(bounds, item.worldTransform.position + Moon::Vector3(0.1f, 0.1f, 0.1f));
    }

    return bounds;
}

Moon::Material* ObjectCopilotPreview::AddPreviewMaterial(Moon::SceneNode* node, const std::string& materialName) {
    Moon::Material* material = node->AddComponent<Moon::Material>();
    material->SetMetallic(0.0f);
    material->SetRoughness(0.72f);
    material->SetOpacity(1.0f);
    material->SetMappingMode(Moon::MappingMode::Triplanar);
    material->SetMaterialPreset(ParseMaterialPreset(materialName));
    if (material->GetMaterialPreset() == Moon::MaterialPreset::None) {
        material->SetBaseColor(Moon::Vector3(0.76f, 0.76f, 0.76f));
    }
    return material;
}

Moon::Material* ObjectCopilotPreview::AddHelperMaterial(Moon::SceneNode* node,
                                                        const Moon::Vector3& color,
                                                        float opacity,
                                                        float roughness) {
    Moon::Material* material = node->AddComponent<Moon::Material>();
    material->SetBaseColor(color);
    material->SetMetallic(0.0f);
    material->SetRoughness(roughness);
    material->SetOpacity(opacity);
    material->SetMappingMode(Moon::MappingMode::UV);
    return material;
}

void ObjectCopilotPreview::FocusCamera(const Bounds3& bounds) {
    if (!m_engine || !m_engine->GetCamera() || !bounds.valid) {
        return;
    }

    Moon::PerspectiveCamera* camera = m_engine->GetCamera();
    const Moon::Vector3 center = (bounds.min + bounds.max) * 0.5f;
    const Moon::Vector3 extents = (bounds.max - bounds.min) * 0.5f;
    const float radius = std::max(0.5f, std::sqrt(extents.x * extents.x + extents.y * extents.y + extents.z * extents.z));
    const Moon::Vector3 eye = center + Moon::Vector3(radius * 1.2f, radius * 0.9f, -radius * 2.6f);

    camera->SetPosition(eye);
    camera->SetTarget(center);
}

void ObjectCopilotPreview::EnsurePreviewEnvironment() {
    Moon::Scene* scene = m_engine->GetScene();
    if (!scene) {
        return;
    }

    if (!scene->FindNodeByName("ObjectCopilotEnvironment")) {
        Moon::SceneNode* environmentNode = scene->CreateNode("ObjectCopilotEnvironment");
        Moon::EnvironmentComponent* environment = environmentNode->AddComponent<Moon::EnvironmentComponent>();

        Moon::EnvironmentProfile profile;
        profile.name = "ObjectCopilotEnvironment";
        profile.enableDayNightCycle = false;
        profile.enableWeather = false;
        profile.enableWind = false;
        profile.enableClouds = false;
        profile.enableFog = false;
        profile.syncPrimaryDirectionalLight = true;
        profile.lockToFixedTime = true;
        profile.fixedTimeHours = 11.0f;
        profile.minSunIntensity = 1.2f;
        profile.maxSunIntensity = 1.2f;
        profile.clearFogDensity = 0.0f;
        profile.fogWeatherDensity = 0.0f;
        profile.cloudyCloudCoverage = 0.0f;
        profile.rainCloudCoverage = 0.0f;

        environment->SetProfile(profile);
        environment->SetTimeOfDay(11.0f);
        environment->SetWeather(Moon::WeatherType::Clear, 0.0f);

        Moon::EnvironmentState state = environment->GetState();
        state.timeOfDay.dayLengthMinutes = 60.0f;
        state.timeOfDay.timeScale = 0.0f;
        state.timeOfDay.paused = true;
        state.atmosphere.cloudCoverage = 0.0f;
        state.atmosphere.fogDensity = 0.0f;
        state.atmosphere.skyZenithColor = Moon::Vector3(0.94f, 0.95f, 0.97f);
        state.atmosphere.skyHorizonColor = Moon::Vector3(0.98f, 0.98f, 0.99f);
        state.atmosphere.sunColor = Moon::Vector3(1.0f, 0.98f, 0.95f);
        state.atmosphere.sunIntensity = 1.2f;
        environment->GetSystem().SetState(state);
    }

    if (!scene->FindNodeByName("ObjectCopilotSun")) {
        Moon::SceneNode* sunNode = scene->CreateNode("ObjectCopilotSun");
        sunNode->GetTransform()->SetLocalPosition(Moon::Vector3(10.0f, 14.0f, -10.0f));
        sunNode->GetTransform()->LookAt(Moon::Vector3(0.0f, 0.0f, 0.0f));
        Moon::Light* light = sunNode->AddComponent<Moon::Light>();
        light->SetType(Moon::Light::Type::Directional);
        light->SetColor(Moon::Vector3(1.0f, 0.98f, 0.95f));
        light->SetIntensity(2.2f);
        light->SetCastShadows(true);
    }
}

void ObjectCopilotPreview::AddPreviewHelpers(const Bounds3& bounds) {
    if (!m_engine || !m_engine->GetScene() || !m_previewRoot) {
        return;
    }

    Moon::Scene* scene = m_engine->GetScene();
    const Moon::Vector3 size = bounds.valid ? (bounds.max - bounds.min) : Moon::Vector3(1.0f, 1.0f, 1.0f);
    const float footprint = std::max(2.5f, std::max(size.x, size.z) * 1.8f);

    if (std::shared_ptr<Moon::Mesh> planeMesh(Moon::MeshGenerator::CreatePlane(
            footprint, footprint, 1, 1, Moon::Vector3(0.65f, 0.67f, 0.70f)));
        planeMesh && planeMesh->IsValid()) {
        Moon::SceneNode* planeNode = scene->CreateNode("__ObjectCopilotPreviewGround");
        planeNode->SetParent(m_previewRoot, false);
        planeNode->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, -0.002f, 0.0f));
        Moon::MeshRenderer* renderer = planeNode->AddComponent<Moon::MeshRenderer>();
        renderer->SetMesh(planeMesh);
        AddHelperMaterial(planeNode, Moon::Vector3(0.82f, 0.82f, 0.80f), 1.0f, 0.98f);
    }

    if (!bounds.valid) {
        return;
    }

    std::shared_ptr<Moon::Mesh> unitCube(Moon::MeshGenerator::CreateCube(1.0f, Moon::Vector3(1.0f, 1.0f, 1.0f)));
    if (!unitCube || !unitCube->IsValid()) {
        return;
    }

    const Moon::Vector3 boundsSize = bounds.max - bounds.min;
    const float maxSize = std::max({boundsSize.x, boundsSize.y, boundsSize.z, 0.001f});
    const float lineThickness = std::max(0.006f, maxSize * 0.01f);
    const float axisThickness = std::max(0.008f, maxSize * 0.012f);
    const float axisLength = std::max(0.35f, maxSize * 0.3f);

    auto addHelperCube = [&](const std::string& name,
                             const Moon::Vector3& center,
                             const Moon::Vector3& scale,
                             const Moon::Vector3& color,
                             float opacity,
                             float roughness) {
        Moon::SceneNode* node = scene->CreateNode(name);
        node->SetParent(m_previewRoot, false);
        node->GetTransform()->SetLocalPosition(center);
        node->GetTransform()->SetLocalScale(scale);
        Moon::MeshRenderer* renderer = node->AddComponent<Moon::MeshRenderer>();
        renderer->SetMesh(unitCube);
        AddHelperMaterial(node, color, opacity, roughness);
    };

    addHelperCube("__PreviewBBox_B0", Moon::Vector3((bounds.min.x + bounds.max.x) * 0.5f, bounds.min.y, bounds.min.z), Moon::Vector3(boundsSize.x, lineThickness, lineThickness), Moon::Vector3(0.95f, 0.76f, 0.22f), 0.72f, 0.28f);
    addHelperCube("__PreviewBBox_B1", Moon::Vector3((bounds.min.x + bounds.max.x) * 0.5f, bounds.min.y, bounds.max.z), Moon::Vector3(boundsSize.x, lineThickness, lineThickness), Moon::Vector3(0.95f, 0.76f, 0.22f), 0.72f, 0.28f);
    addHelperCube("__PreviewBBox_B2", Moon::Vector3(bounds.min.x, bounds.min.y, (bounds.min.z + bounds.max.z) * 0.5f), Moon::Vector3(lineThickness, lineThickness, boundsSize.z), Moon::Vector3(0.95f, 0.76f, 0.22f), 0.72f, 0.28f);
    addHelperCube("__PreviewBBox_B3", Moon::Vector3(bounds.max.x, bounds.min.y, (bounds.min.z + bounds.max.z) * 0.5f), Moon::Vector3(lineThickness, lineThickness, boundsSize.z), Moon::Vector3(0.95f, 0.76f, 0.22f), 0.72f, 0.28f);
    addHelperCube("__PreviewBBox_T0", Moon::Vector3((bounds.min.x + bounds.max.x) * 0.5f, bounds.max.y, bounds.min.z), Moon::Vector3(boundsSize.x, lineThickness, lineThickness), Moon::Vector3(0.95f, 0.76f, 0.22f), 0.72f, 0.28f);
    addHelperCube("__PreviewBBox_T1", Moon::Vector3((bounds.min.x + bounds.max.x) * 0.5f, bounds.max.y, bounds.max.z), Moon::Vector3(boundsSize.x, lineThickness, lineThickness), Moon::Vector3(0.95f, 0.76f, 0.22f), 0.72f, 0.28f);
    addHelperCube("__PreviewBBox_T2", Moon::Vector3(bounds.min.x, bounds.max.y, (bounds.min.z + bounds.max.z) * 0.5f), Moon::Vector3(lineThickness, lineThickness, boundsSize.z), Moon::Vector3(0.95f, 0.76f, 0.22f), 0.72f, 0.28f);
    addHelperCube("__PreviewBBox_T3", Moon::Vector3(bounds.max.x, bounds.max.y, (bounds.min.z + bounds.max.z) * 0.5f), Moon::Vector3(lineThickness, lineThickness, boundsSize.z), Moon::Vector3(0.95f, 0.76f, 0.22f), 0.72f, 0.28f);
    addHelperCube("__PreviewBBox_V0", Moon::Vector3(bounds.min.x, (bounds.min.y + bounds.max.y) * 0.5f, bounds.min.z), Moon::Vector3(lineThickness, boundsSize.y, lineThickness), Moon::Vector3(0.95f, 0.76f, 0.22f), 0.72f, 0.28f);
    addHelperCube("__PreviewBBox_V1", Moon::Vector3(bounds.max.x, (bounds.min.y + bounds.max.y) * 0.5f, bounds.min.z), Moon::Vector3(lineThickness, boundsSize.y, lineThickness), Moon::Vector3(0.95f, 0.76f, 0.22f), 0.72f, 0.28f);
    addHelperCube("__PreviewBBox_V2", Moon::Vector3(bounds.min.x, (bounds.min.y + bounds.max.y) * 0.5f, bounds.max.z), Moon::Vector3(lineThickness, boundsSize.y, lineThickness), Moon::Vector3(0.95f, 0.76f, 0.22f), 0.72f, 0.28f);
    addHelperCube("__PreviewBBox_V3", Moon::Vector3(bounds.max.x, (bounds.min.y + bounds.max.y) * 0.5f, bounds.max.z), Moon::Vector3(lineThickness, boundsSize.y, lineThickness), Moon::Vector3(0.95f, 0.76f, 0.22f), 0.72f, 0.28f);

    addHelperCube("__PreviewAxisX", Moon::Vector3(axisLength * 0.5f, 0.0f, 0.0f), Moon::Vector3(axisLength, axisThickness, axisThickness), Moon::Vector3(0.92f, 0.28f, 0.28f), 0.90f, 0.18f);
    addHelperCube("__PreviewAxisY", Moon::Vector3(0.0f, axisLength * 0.5f, 0.0f), Moon::Vector3(axisThickness, axisLength, axisThickness), Moon::Vector3(0.26f, 0.82f, 0.36f), 0.90f, 0.18f);
    addHelperCube("__PreviewAxisZ", Moon::Vector3(0.0f, 0.0f, axisLength * 0.5f), Moon::Vector3(axisThickness, axisThickness, axisLength), Moon::Vector3(0.30f, 0.52f, 0.96f), 0.90f, 0.18f);
}

void ObjectCopilotPreview::UpdatePreviewStats(const Bounds3& bounds) {
    m_previewStats = PreviewStats{};
    if (!bounds.valid) {
        return;
    }

    const Moon::Vector3 size = bounds.max - bounds.min;
    m_previewStats.valid = true;
    m_previewStats.min = bounds.min;
    m_previewStats.max = bounds.max;
    m_previewStats.size = size;
    m_previewStats.axisLength = std::max(0.35f, std::max({size.x, size.y, size.z}) * 0.3f);
}

} // namespace Tooling
} // namespace Moon
