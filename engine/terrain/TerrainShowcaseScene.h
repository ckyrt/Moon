#pragma once

class EngineCore;

namespace Moon {

struct TerrainShowcaseOptions {
    bool configureCamera = true;
    bool createEnvironment = true;
    bool createSunLight = true;
    bool createTerrainRuntime = true;
};

class TerrainShowcaseScene {
public:
    static void BuildOpenWorldScene(EngineCore* engine, const TerrainShowcaseOptions& options = {});
};

} // namespace Moon
