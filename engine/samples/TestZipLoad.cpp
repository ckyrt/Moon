#include "TestScenes.h"
#include <Logging/Logger.h>
#include <CSG/CSGOperations.h>
#include "CSGUnitTest.h"
#include <EngineCore.h>
#include <Scene/MeshRenderer.h>
#include <Scene/Material.h>
#include "SceneZipLoader.h"

namespace TestScenes {

    void TestZipLoad(EngineCore* engine)
    {
        MOON_LOG_INFO("Test", "Running ZIP scene loading test");

        SceneData sceneData;
        if (!SceneZipLoader::LoadSceneFromZip("C:/Users/Administrator/Downloads/my_scene (2).zip", sceneData)) {
            MOON_LOG_ERROR("Test", "Failed to load scene from zip");
            return;
        }

        // TODO:
        // 1. Terrain → mesh
        // 2. Grass → instancing
        // 3. Rivers → spline mesh

        MOON_LOG_INFO("Test", "ZIP scene loaded successfully");
    }

}
