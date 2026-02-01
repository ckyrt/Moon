#include "TestScenes.h"
#include <Logging/Logger.h>
#include <CSG/CSGOperations.h>
#include "CSGUnitTest.h"
#include <EngineCore.h>
#include <Scene/MeshRenderer.h>
#include <Scene/Material.h>
#include "SceneZipLoader.h"
#include <Geometry/MeshGenerator.h>

namespace TestScenes {

    void TestZipLoad(EngineCore* engine)
    {
        MOON_LOG_INFO("Test", "Running ZIP scene loading test");

        SceneData sceneData;
        if (!SceneZipLoader::LoadSceneFromZip("C:/Users/Administrator/Downloads/0201.zip", sceneData)) {
            MOON_LOG_ERROR("Test", "Failed to load scene from zip");
            return;
        }

        Moon::Scene* scene = engine->GetScene();
        Moon::MeshManager* meshManager = engine->GetMeshManager();
        Moon::TextureManager* textureManager = engine->GetTextureManager();

        {
            // terrain
            Moon::SceneNode* terrainNode = scene->CreateNode("Terrain");
            auto* mr = terrainNode->AddComponent<Moon::MeshRenderer>();
            auto* mat = terrainNode->AddComponent<Moon::Material>();

            Moon::Mesh* rawMesh = Moon::MeshGenerator::CreateTerrainFromHeightmap(
                sceneData.terrain.resolution,  // 采样分辨率（如 129×129）
                sceneData.terrain.resolution,
                sceneData.terrain.heightMap.data(),
                100.0f,  // 实际地形宽度（世界坐标单位）
                100.0f,  // 实际地形深度（世界坐标单位）
                1.0f,    // 高度缩放
                true
            );
            std::shared_ptr<Moon::Mesh> terrainMesh(rawMesh);
            mr->SetMesh(terrainMesh);

            mat->SetBaseColor({ 0.3f, 0.7f, 0.3f });
            mat->SetRoughness(1.0f);
            mat->SetMetallic(0.0f);
        }

        {
            // river
            for (const RiverSpline& river : sceneData.rivers)
            {
                Moon::Mesh* rawMesh = Moon::MeshGenerator::CreateRiverFromPolyline(
                    river.points,
                    river.width
                );

                auto riverMesh = std::shared_ptr<Moon::Mesh>(rawMesh);

                Moon::SceneNode* riverNode = scene->CreateNode("River_" + river.id);
                auto* mr = riverNode->AddComponent<Moon::MeshRenderer>();
                auto* mat = riverNode->AddComponent<Moon::Material>();

                mr->SetMesh(riverMesh);

                mat->SetBaseColor({ 0.1f, 0.4f, 0.6f });
                mat->SetRoughness(0.05f);
                mat->SetMetallic(0.0f);
            }
        }

        MOON_LOG_INFO("Test", "ZIP scene loaded successfully");
    }

}
