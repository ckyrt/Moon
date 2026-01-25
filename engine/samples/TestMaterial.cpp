#include "TestScenes.h"
#include <Logging/Logger.h>
#include <CSG/CSGOperations.h>
#include "CSGUnitTest.h"
#include <EngineCore.h>
#include <Scene/MeshRenderer.h>
#include <Scene/Material.h>

namespace TestScenes {

    void TestMaterial(EngineCore* engine)
    {
        Moon::Scene* scene = engine->GetScene();
        Moon::MeshManager* meshManager = engine->GetMeshManager();
        Moon::TextureManager* textureManager = engine->GetTextureManager();

        MOON_LOG_INFO("Sample", "Creating Material Showcase Scene...");

        // ============================================================================
        // 1. 地面 - rock_terrain (大平面)
        // ============================================================================
        Moon::SceneNode* groundNode = scene->CreateNode("Ground_RockTerrain");
        groundNode->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, 0.0f, 0.0f));
        groundNode->GetTransform()->SetLocalScale(Moon::Vector3(2.0f, 1.0f, 2.0f));  // 放大地面

        Moon::MeshRenderer* groundRenderer = groundNode->AddComponent<Moon::MeshRenderer>();
        groundRenderer->SetMesh(meshManager->CreatePlane(25.0f, 25.0f, 1, 1, Moon::Vector3(0.5f, 0.5f, 0.5f)));

        Moon::Material* groundMaterial = groundNode->AddComponent<Moon::Material>();
        groundMaterial->SetMaterialPreset(Moon::MaterialPreset::Concrete);
        groundMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 普通mesh使用UV

        // ============================================================================
        // 2. 石膏墙立方体 - painted_plaster_wall
        // ============================================================================
        Moon::SceneNode* plasterCubeNode = scene->CreateNode("PlasterWall_Cube");
        plasterCubeNode->GetTransform()->SetLocalPosition(Moon::Vector3(-5.0f, 2.0f, 0.0f));
        plasterCubeNode->GetTransform()->SetLocalScale(Moon::Vector3(2.0f, 4.0f, 0.5f));  // 墙的形状

        Moon::MeshRenderer* plasterRenderer = plasterCubeNode->AddComponent<Moon::MeshRenderer>();
        plasterRenderer->SetMesh(meshManager->CreateCube(1.0f, Moon::Vector3(0.95f, 0.95f, 0.92f)));

        Moon::Material* plasterMaterial = plasterCubeNode->AddComponent<Moon::Material>();
        plasterMaterial->SetMaterialPreset(Moon::MaterialPreset::Plastic);
        plasterMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 普通mesh使用UV

        // ============================================================================
        // 3. 红砖墙立方体 - red_brick
        // ============================================================================
        Moon::SceneNode* brickWallNode = scene->CreateNode("BrickWall_Cube");
        brickWallNode->GetTransform()->SetLocalPosition(Moon::Vector3(5.0f, 2.0f, 0.0f));
        brickWallNode->GetTransform()->SetLocalScale(Moon::Vector3(2.0f, 4.0f, 0.5f));  // 墙的形状

        Moon::MeshRenderer* brickRenderer = brickWallNode->AddComponent<Moon::MeshRenderer>();
        brickRenderer->SetMesh(meshManager->CreateCube(1.0f, Moon::Vector3(0.6f, 0.3f, 0.2f)));

        Moon::Material* brickMaterial = brickWallNode->AddComponent<Moon::Material>();
        brickMaterial->SetMaterialPreset(Moon::MaterialPreset::Rock);
        brickMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 普通mesh使用UV

        // ============================================================================
        // 4. 橡胶跑道矩形 - rubberized_track
        // ============================================================================
        Moon::SceneNode* trackNode = scene->CreateNode("RubberTrack_Rectangle");
        trackNode->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, 0.05f, -5.0f));  // 稍微抬高避免 z-fighting
        trackNode->GetTransform()->SetLocalScale(Moon::Vector3(8.0f, 1.0f, 3.0f));  // 跑道形状

        Moon::MeshRenderer* trackRenderer = trackNode->AddComponent<Moon::MeshRenderer>();
        trackRenderer->SetMesh(meshManager->CreatePlane(1.0f, 1.0f, 1, 1, Moon::Vector3(0.2f, 0.2f, 0.2f)));

        Moon::Material* trackMaterial = trackNode->AddComponent<Moon::Material>();
        trackMaterial->SetMaterialPreset(Moon::MaterialPreset::Fabric);
        trackMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 普通mesh使用UV

        // ============================================================================
        // 5. 生锈金属球 - rusty_metal
        // ============================================================================
        Moon::SceneNode* metalSphereNode = scene->CreateNode("RustyMetal_Sphere");
        metalSphereNode->GetTransform()->SetLocalPosition(Moon::Vector3(-2.0f, 1.5f, 3.0f));

        Moon::MeshRenderer* metalRenderer = metalSphereNode->AddComponent<Moon::MeshRenderer>();
        metalRenderer->SetMesh(meshManager->CreateSphere(1.0f, 64, 32, Moon::Vector3(1.0f, 1.0f, 1.0f)));

        Moon::Material* metalMaterial = metalSphereNode->AddComponent<Moon::Material>();
        metalMaterial->SetMaterialPreset(Moon::MaterialPreset::Metal);
        metalMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 普通mesh使用UV

        // ============================================================================
        // 6. 木头球 - wood_floor
        // ============================================================================
        Moon::SceneNode* woodSphereNode = scene->CreateNode("Wood_Sphere");
        woodSphereNode->GetTransform()->SetLocalPosition(Moon::Vector3(2.0f, 1.5f, 3.0f));

        Moon::MeshRenderer* woodRenderer = woodSphereNode->AddComponent<Moon::MeshRenderer>();
        woodRenderer->SetMesh(meshManager->CreateSphere(1.0f, 64, 32, Moon::Vector3(1.0f, 1.0f, 1.0f)));

        Moon::Material* woodMaterial = woodSphereNode->AddComponent<Moon::Material>();
        woodMaterial->SetMaterialPreset(Moon::MaterialPreset::Wood);
        woodMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 普通mesh使用UV
    }

}
