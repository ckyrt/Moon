#include "TestScenes.h"

#include <EngineCore.h>
#include <Logging/Logger.h>
#include "../terrain/TerrainShowcaseScene.h"
#include "../terrain/WorldSpec.h"
#include "../core/Assets/AssetPaths.h"

namespace TestScenes {

void TestTerrain(EngineCore* engine)
{
    if (!engine) {
        return;
    }

    const std::string promptSpecPath = Moon::Assets::BuildAssetPath("terrain/demo_coastal_island.prompt.json");
    Moon::WorldPromptSpec promptSpec;
    std::string loadError;
    if (Moon::WorldSpecIO::LoadPromptSpecFromFile(promptSpecPath, promptSpec, &loadError)) {
        const Moon::WorldBuildSpec buildSpec = Moon::WorldSpecIO::BuildFromPrompt(promptSpec);
        Moon::TerrainShowcaseScene::BuildOpenWorldScene(engine, buildSpec);
    } else {
        MOON_LOG_WARN("TestTerrain", "Failed to load prompt spec '%s': %s", promptSpecPath.c_str(), loadError.c_str());
        Moon::TerrainShowcaseScene::BuildOpenWorldScene(engine);
    }
}

} // namespace TestScenes
