#include "TestScenes.h"

#include <EngineCore.h>
#include "../terrain/TerrainShowcaseScene.h"

namespace TestScenes {

void TestTerrain(EngineCore* engine)
{
    if (!engine) {
        return;
    }
    Moon::TerrainShowcaseScene::BuildOpenWorldScene(engine);
}

} // namespace TestScenes
