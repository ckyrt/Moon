#pragma once

#include "../core/Assets/MeshManager.h"
#include "../core/Camera/Camera.h"
#include "../core/EngineCore.h"
#include "../core/Geometry/MeshGenerator.h"
#include "../core/Math/Quaternion.h"
#include "../core/Math/Vector3.h"
#include "../core/Mesh/Mesh.h"
#include "../core/Scene/Light.h"
#include "../core/Scene/Material.h"
#include "../core/Scene/MeshRenderer.h"
#include "../core/Scene/Scene.h"
#include "../core/Scene/SceneNode.h"
#include "../core/Scene/Skybox.h"
#include "../environment/EnvironmentComponent.h"
#include "../render/diligent/DiligentRenderer.h"
#include "../render/SceneRenderer.h"
#include "../terrain/ProceduralTerrainGenerator.h"
#include "../terrain/TerrainComponent.h"
#include "../terrain/TerrainVisualBuilder.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Moon {
namespace Api {

enum class PrimitiveKind {
    Cube,
    Sphere,
    Plane,
    Cylinder,
    Cone,
    Torus,
    Capsule,
    Quad
};

struct TransformDesc {
    Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 rotationEulerDeg = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);
};

struct MaterialDesc {
    MaterialPreset preset = MaterialPreset::None;
    Vector3 baseColor = Vector3(1.0f, 1.0f, 1.0f);
    float metallic = 0.0f;
    float roughness = 0.5f;
    float opacity = 1.0f;
    MappingMode mappingMode = MappingMode::UV;
    float triplanarTiling = 0.5f;
    float triplanarBlend = 4.0f;
    ShadingModel shadingModel = ShadingModel::DefaultLit;
    bool useVertexColorTint = false;
};

struct PrimitiveDesc {
    std::string name = "Primitive";
    PrimitiveKind kind = PrimitiveKind::Cube;
    TransformDesc transform;
    MaterialDesc material;
    Vector3 vertexColor = Vector3(1.0f, 1.0f, 1.0f);
    float width = 1.0f;
    float height = 1.0f;
    float depth = 1.0f;
    float radius = 0.5f;
    float radiusTop = 0.5f;
    float radiusBottom = 0.5f;
    float majorRadius = 0.5f;
    float minorRadius = 0.2f;
    int segments = 24;
    int rings = 16;
    int subdivisionsX = 1;
    int subdivisionsZ = 1;
};

struct WallDesc {
    std::string name = "Wall";
    Vector3 start = Vector3(-1.0f, 0.0f, 0.0f);
    Vector3 end = Vector3(1.0f, 0.0f, 0.0f);
    float height = 3.0f;
    float thickness = 0.2f;
    MaterialDesc material;
};

struct FloorDesc {
    std::string name = "Floor";
    Vector3 center = Vector3(0.0f, 0.0f, 0.0f);
    float width = 10.0f;
    float depth = 10.0f;
    int subdivisionsX = 1;
    int subdivisionsZ = 1;
    MaterialDesc material;
};

struct WaterPlaneDesc {
    std::string name = "Water";
    Vector3 center = Vector3(0.0f, 0.0f, 0.0f);
    float width = 100.0f;
    float depth = 100.0f;
    float opacity = 0.58f;
    Vector3 color = Vector3(0.12f, 0.38f, 0.62f);
};

struct RiverDesc {
    std::string name = "River";
    std::vector<float> controlPoints;
    float width = 8.0f;
    float waterDepth = 0.5f;
    MaterialDesc material;
};

struct TerrainNodes {
    SceneNode* root = nullptr;
    SceneNode* terrain = nullptr;
    SceneNode* rivers = nullptr;
    SceneNode* ocean = nullptr;
    SceneNode* grass = nullptr;
    TerrainComponent* terrainComponent = nullptr;
};

class RenderWorld {
public:
    explicit RenderWorld(EngineCore& engine)
        : m_engine(&engine)
        , m_scene(engine.GetScene())
        , m_meshManager(engine.GetMeshManager())
    {
    }

    RenderWorld(Scene& scene, MeshManager& meshManager)
        : m_scene(&scene)
        , m_meshManager(&meshManager)
    {
    }

    Scene* GetScene() const { return m_scene; }
    MeshManager* GetMeshManager() const { return m_meshManager; }

    SceneNode* CreateNode(const std::string& name, const TransformDesc& transform = TransformDesc())
    {
        if (!m_scene) {
            return nullptr;
        }

        SceneNode* node = m_scene->CreateNode(name);
        ApplyTransform(node, transform);
        return node;
    }

    Material* AddMaterial(SceneNode* node, const MaterialDesc& desc)
    {
        if (!node) {
            return nullptr;
        }

        Material* material = node->GetComponent<Material>();
        if (!material) {
            material = node->AddComponent<Material>();
        }
        ApplyMaterial(material, desc);
        return material;
    }

    MeshRenderer* AddMesh(SceneNode* node, std::shared_ptr<Mesh> mesh)
    {
        if (!node || !mesh) {
            return nullptr;
        }

        MeshRenderer* renderer = node->GetComponent<MeshRenderer>();
        if (!renderer) {
            renderer = node->AddComponent<MeshRenderer>();
        }
        renderer->SetMesh(std::move(mesh));
        return renderer;
    }

    SceneNode* CreatePrimitive(const PrimitiveDesc& desc)
    {
        SceneNode* node = CreateNode(desc.name, desc.transform);
        AddMesh(node, BuildPrimitiveMesh(desc));
        AddMaterial(node, desc.material);
        return node;
    }

    SceneNode* CreateGround(const FloorDesc& desc)
    {
        FloorDesc copy = desc;
        copy.name = copy.name.empty() ? "Ground" : copy.name;
        MaterialDesc material = copy.material;
        if (material.preset == MaterialPreset::None) {
            material.preset = MaterialPreset::ConcreteFloor;
            material.mappingMode = MappingMode::Triplanar;
        }
        return CreateFloor(copy, material);
    }

    SceneNode* CreateFloor(const FloorDesc& desc)
    {
        return CreateFloor(desc, desc.material);
    }

    SceneNode* CreateWall(const WallDesc& desc)
    {
        const Vector3 delta = desc.end - desc.start;
        const float length = std::max(0.001f, std::sqrt(delta.x * delta.x + delta.z * delta.z));
        const float yawDeg = std::atan2(delta.z, delta.x) * 180.0f / 3.14159265358979323846f;

        PrimitiveDesc primitive;
        primitive.name = desc.name;
        primitive.kind = PrimitiveKind::Cube;
        primitive.width = 1.0f;
        primitive.height = 1.0f;
        primitive.depth = 1.0f;
        primitive.transform.position = Vector3(
            (desc.start.x + desc.end.x) * 0.5f,
            std::min(desc.start.y, desc.end.y) + desc.height * 0.5f,
            (desc.start.z + desc.end.z) * 0.5f);
        primitive.transform.rotationEulerDeg = Vector3(0.0f, -yawDeg, 0.0f);
        primitive.transform.scale = Vector3(length, desc.height, desc.thickness);
        primitive.material = desc.material;
        if (primitive.material.preset == MaterialPreset::None) {
            primitive.material.preset = MaterialPreset::Brick;
            primitive.material.mappingMode = MappingMode::Triplanar;
        }
        return CreatePrimitive(primitive);
    }

    SceneNode* CreateWaterPlane(const WaterPlaneDesc& desc)
    {
        PrimitiveDesc primitive;
        primitive.name = desc.name;
        primitive.kind = PrimitiveKind::Plane;
        primitive.width = desc.width;
        primitive.depth = desc.depth;
        primitive.subdivisionsX = 8;
        primitive.subdivisionsZ = 8;
        primitive.transform.position = desc.center;
        primitive.material.baseColor = desc.color;
        primitive.material.opacity = desc.opacity;
        primitive.material.roughness = 0.06f;
        primitive.material.shadingModel = ShadingModel::Water;
        return CreatePrimitive(primitive);
    }

    SceneNode* CreateRiver(const RiverDesc& desc)
    {
        if (!m_scene || desc.controlPoints.size() < 6) {
            return nullptr;
        }

        std::shared_ptr<Mesh> mesh(
            MeshGenerator::CreateRiverFromPolyline(desc.controlPoints, desc.width, desc.waterDepth));
        SceneNode* node = CreateNode(desc.name);
        AddMesh(node, mesh);

        MaterialDesc material = desc.material;
        material.shadingModel = ShadingModel::Water;
        material.opacity = material.opacity < 1.0f ? material.opacity : 0.62f;
        material.baseColor = Vector3(0.10f, 0.36f, 0.58f);
        AddMaterial(node, material);
        return node;
    }

    TerrainNodes CreateProceduralTerrain(
        const TerrainGenerationSettings& settings,
        bool createRivers = true,
        bool createOcean = true,
        bool createGrass = true)
    {
        TerrainNodes out;
        if (!m_scene) {
            return out;
        }

        TerrainGenerationResult generation = ProceduralTerrainGenerator::CreateOpenWorldLandscape(settings);
        out.root = m_scene->CreateNode("Procedural Terrain");

        out.terrain = CreateNode("Terrain Mesh");
        out.terrain->SetParent(out.root);
        AddMesh(out.terrain, TerrainVisualBuilder::BuildTerrainMesh(generation, settings));
        MaterialDesc terrainMaterial;
        terrainMaterial.preset = MaterialPreset::Rock;
        terrainMaterial.mappingMode = MappingMode::Triplanar;
        terrainMaterial.triplanarTiling = 0.18f;
        AddMaterial(out.terrain, terrainMaterial);

        out.terrainComponent = out.terrain->AddComponent<TerrainComponent>();
        TerrainProfile profile;
        profile.worldWidth = settings.worldWidth;
        profile.worldDepth = settings.worldDepth;
        profile.heightScale = settings.heightScale;
        out.terrainComponent->SetProfile(profile);
        out.terrainComponent->SetData(generation.terrainData);

        if (createRivers) {
            out.rivers = CreateNode("Rivers");
            out.rivers->SetParent(out.root);
            AddMesh(out.rivers, TerrainVisualBuilder::BuildRiverMesh(generation, settings));
            MaterialDesc riverMaterial;
            riverMaterial.shadingModel = ShadingModel::Water;
            riverMaterial.baseColor = Vector3(0.10f, 0.32f, 0.54f);
            riverMaterial.opacity = 0.62f;
            AddMaterial(out.rivers, riverMaterial);
        }

        if (createOcean && settings.hasOcean) {
            out.ocean = CreateNode("Ocean");
            out.ocean->SetParent(out.root);
            AddMesh(out.ocean, TerrainVisualBuilder::BuildOceanMesh(generation, settings));
            MaterialDesc oceanMaterial;
            oceanMaterial.shadingModel = ShadingModel::Water;
            oceanMaterial.baseColor = Vector3(0.06f, 0.24f, 0.44f);
            oceanMaterial.opacity = 0.68f;
            AddMaterial(out.ocean, oceanMaterial);
        }

        if (createGrass) {
            out.grass = CreateNode("Grass");
            out.grass->SetParent(out.root);
            AddMesh(out.grass, TerrainVisualBuilder::BuildGrassMesh(generation.terrainData, generation, settings));
            MaterialDesc grassMaterial;
            grassMaterial.baseColor = Vector3(0.18f, 0.42f, 0.18f);
            grassMaterial.roughness = 0.9f;
            grassMaterial.useVertexColorTint = true;
            AddMaterial(out.grass, grassMaterial);
        }

        return out;
    }

    EnvironmentComponent* CreateEnvironment(
        const EnvironmentProfile& profile = EnvironmentProfile(),
        WeatherType weather = WeatherType::Clear,
        float timeOfDayHours = 12.0f)
    {
        SceneNode* node = CreateNode("Environment");
        if (!node) {
            return nullptr;
        }

        EnvironmentComponent* environment = node->AddComponent<EnvironmentComponent>();
        environment->SetProfile(profile);
        environment->SetWeather(weather, 0.0f);
        environment->SetTimeOfDay(timeOfDayHours);

        Light* sun = CreateDirectionalLight(
            "Sun",
            Vector3(50.0f, 70.0f, -35.0f),
            Vector3(50.0f, -70.0f, 35.0f),
            Vector3(1.0f, 0.96f, 0.84f),
            profile.maxSunIntensity);
        if (sun) {
            sun->SetCastShadows(true);
        }

        return environment;
    }

    Skybox* CreateProceduralSky(float intensity = 1.0f)
    {
        SceneNode* node = CreateNode("Skybox");
        if (!node) {
            return nullptr;
        }

        Skybox* skybox = node->AddComponent<Skybox>();
        skybox->SetType(Skybox::Type::Procedural);
        skybox->SetIntensity(intensity);
        skybox->SetEnableIBL(true);
        return skybox;
    }

    Skybox* CreateHdrSkybox(const std::string& hdrPath, float intensity = 1.0f)
    {
        SceneNode* node = CreateNode("Skybox");
        if (!node) {
            return nullptr;
        }

        Skybox* skybox = node->AddComponent<Skybox>();
        skybox->SetIntensity(intensity);
        skybox->SetEnableIBL(true);
        skybox->LoadEnvironmentMap(hdrPath);
        return skybox;
    }

    Light* CreateDirectionalLight(
        const std::string& name,
        const Vector3& position,
        const Vector3& lookAt,
        const Vector3& color,
        float intensity)
    {
        SceneNode* node = CreateNode(name);
        if (!node) {
            return nullptr;
        }

        node->GetTransform()->SetWorldPosition(position);
        node->GetTransform()->LookAt(lookAt);
        Light* light = node->AddComponent<Light>();
        light->SetType(Light::Type::Directional);
        light->SetColor(color);
        light->SetIntensity(intensity);
        light->SetCastShadows(true);
        return light;
    }

    Light* CreatePointLight(
        const std::string& name,
        const Vector3& position,
        const Vector3& color,
        float intensity,
        float range,
        bool castShadows = false)
    {
        SceneNode* node = CreateNode(name);
        if (!node) {
            return nullptr;
        }

        node->GetTransform()->SetWorldPosition(position);
        Light* light = node->AddComponent<Light>();
        light->SetType(Light::Type::Point);
        light->SetColor(color);
        light->SetIntensity(intensity);
        light->SetRange(range);
        light->SetCastShadows(castShadows);
        return light;
    }

    Light* CreateSpotLight(
        const std::string& name,
        const Vector3& position,
        const Vector3& lookAt,
        const Vector3& color,
        float intensity,
        float range,
        float innerConeDeg,
        float outerConeDeg,
        bool castShadows = false)
    {
        SceneNode* node = CreateNode(name);
        if (!node) {
            return nullptr;
        }

        node->GetTransform()->SetWorldPosition(position);
        node->GetTransform()->LookAt(lookAt);
        Light* light = node->AddComponent<Light>();
        light->SetType(Light::Type::Spot);
        light->SetColor(color);
        light->SetIntensity(intensity);
        light->SetRange(range);
        light->SetSpotAngles(innerConeDeg, outerConeDeg);
        light->SetCastShadows(castShadows);
        return light;
    }

    void Tick(float deltaTimeSeconds)
    {
        if (m_engine) {
            m_engine->Tick(deltaTimeSeconds);
        } else if (m_scene) {
            m_scene->Update(deltaTimeSeconds);
        }
    }

    void Render(DiligentRenderer& renderer, Camera& camera, const EnvironmentState* environmentState = nullptr)
    {
        if (m_scene) {
            SceneRendererUtils::RenderScene(&renderer, m_scene, &camera, environmentState);
        }
    }

private:
    EngineCore* m_engine = nullptr;
    Scene* m_scene = nullptr;
    MeshManager* m_meshManager = nullptr;

    static void ApplyTransform(SceneNode* node, const TransformDesc& desc)
    {
        if (!node) {
            return;
        }

        Transform* transform = node->GetTransform();
        transform->SetLocalPosition(desc.position);
        transform->SetLocalRotation(desc.rotationEulerDeg);
        transform->SetLocalScale(desc.scale);
    }

    static void ApplyMaterial(Material* material, const MaterialDesc& desc)
    {
        if (!material) {
            return;
        }

        material->SetMaterialPreset(desc.preset);
        material->SetBaseColor(desc.baseColor);
        material->SetMetallic(desc.metallic);
        material->SetRoughness(desc.roughness);
        material->SetOpacity(desc.opacity);
        material->SetMappingMode(desc.mappingMode);
        material->SetTriplanarTiling(desc.triplanarTiling);
        material->SetTriplanarBlend(desc.triplanarBlend);
        material->SetShadingModel(desc.shadingModel);
        material->SetUseVertexColorTint(desc.useVertexColorTint);
    }

    std::shared_ptr<Mesh> BuildPrimitiveMesh(const PrimitiveDesc& desc)
    {
        if (!m_meshManager) {
            return nullptr;
        }

        switch (desc.kind) {
        case PrimitiveKind::Cube:
            return m_meshManager->CreateCube(1.0f, desc.vertexColor);
        case PrimitiveKind::Sphere:
            return m_meshManager->CreateSphere(desc.radius, desc.segments, desc.rings, desc.vertexColor);
        case PrimitiveKind::Plane:
            return m_meshManager->CreatePlane(
                desc.width,
                desc.depth,
                desc.subdivisionsX,
                desc.subdivisionsZ,
                desc.vertexColor);
        case PrimitiveKind::Cylinder:
            return m_meshManager->CreateCylinder(
                desc.radiusTop,
                desc.radiusBottom,
                desc.height,
                desc.segments,
                desc.vertexColor);
        case PrimitiveKind::Cone:
            return m_meshManager->CreateCone(desc.radius, desc.height, desc.segments, desc.vertexColor);
        case PrimitiveKind::Torus:
            return m_meshManager->CreateTorus(
                desc.majorRadius,
                desc.minorRadius,
                desc.segments,
                std::max(3, desc.rings),
                desc.vertexColor);
        case PrimitiveKind::Capsule:
            return m_meshManager->CreateCapsule(desc.radius, desc.height, desc.segments, desc.rings, desc.vertexColor);
        case PrimitiveKind::Quad:
            return m_meshManager->CreateQuad(desc.width, desc.height, desc.vertexColor);
        }

        return nullptr;
    }

    SceneNode* CreateFloor(const FloorDesc& desc, const MaterialDesc& material)
    {
        PrimitiveDesc primitive;
        primitive.name = desc.name;
        primitive.kind = PrimitiveKind::Plane;
        primitive.width = desc.width;
        primitive.depth = desc.depth;
        primitive.subdivisionsX = desc.subdivisionsX;
        primitive.subdivisionsZ = desc.subdivisionsZ;
        primitive.transform.position = desc.center;
        primitive.material = material;
        return CreatePrimitive(primitive);
    }
};

} // namespace Api
} // namespace Moon
