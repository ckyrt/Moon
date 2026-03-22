#pragma once

#include "TerrainTypes.h"

#include <vector>

namespace Moon {

struct TerrainGenerationSettings {
    uint32_t resolution = 257;
    float worldWidth = 1400.0f;
    float worldDepth = 1400.0f;
    float heightScale = 180.0f;
    float baseHeight01 = 0.30f;
    float riverWidth = 42.0f;
    float riverDepth = 5.0f;
    uint32_t grassClusterBudget = 2200;
    uint32_t seed = 1337;
};

struct TerrainGenerationResult {
    TerrainData terrainData;
    std::vector<float> riverPolyline;
    float riverWidth = 0.0f;
    float riverDepth = 0.0f;
};

class ProceduralTerrainGenerator {
public:
    static TerrainGenerationResult CreateOpenWorldLandscape(const TerrainGenerationSettings& settings);
};

} // namespace Moon
