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
        if (!SceneZipLoader::LoadSceneFromZip("C:/Users/Administrator/Downloads/my_scene (2).zip", sceneData)) {
            MOON_LOG_ERROR("Test", "Failed to load scene from zip");
            return;
        }

        Moon::Scene* scene = engine->GetScene();
        Moon::MeshManager* meshManager = engine->GetMeshManager();
        Moon::TextureManager* textureManager = engine->GetTextureManager();

        Moon::SceneNode* terrainNode = scene->CreateNode("Terrain");

        auto* mr = terrainNode->AddComponent<Moon::MeshRenderer>();
        auto* mat = terrainNode->AddComponent<Moon::Material>();

        Moon::Mesh* rawMesh = Moon::MeshGenerator::CreateTerrainFromHeightmap(
            sceneData.terrain.resolution,
            sceneData.terrain.resolution,
            sceneData.terrain.heightMap.data(),
            1.0f,
            1.0f,
            true
        );
        std::shared_ptr<Moon::Mesh> terrainMesh(rawMesh);
        mr->SetMesh(terrainMesh);

        mat->SetBaseColor({ 0.3f, 0.7f, 0.3f });
        mat->SetRoughness(1.0f);
        mat->SetMetallic(0.0f);

        MOON_LOG_INFO("Test", "ZIP scene loaded successfully");
    }

}
