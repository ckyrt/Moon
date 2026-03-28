#pragma once

#include "TerrainTypes.h"
#include "WorldSpec.h"

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
    float relief = 0.78f;
    float mountainDensity = 0.62f;
    float mountainScale = 0.74f;
    float hillDensity = 0.35f;
    float flatAreaRatio = 0.18f;
    float roughness = 0.58f;
    float erosionStrength = 0.64f;
    float cliffFrequency = 0.27f;
    float ridgeDirectionDegrees = 35.0f;
    bool hasOcean = false;
    float oceanCoverage = 0.33f;
    float seaLevel01 = 0.18f;
    float beachWidth = 0.18f;
    float coastalShelf = 0.12f;
    uint32_t riverCount = 2;
    float riverMeander = 0.66f;
    float grassHeight = 0.58f;
    float shrubDensity = 0.24f;
    float treeDensity = 0.36f;
};

struct TerrainGenerationResult {
    TerrainData terrainData;
    std::vector<std::vector<float>> riverPolylines;
    std::vector<std::vector<float>> riverWidthProfiles;
    float riverWidth = 0.0f;
    float riverDepth = 0.0f;
    float seaLevelWorldY = 0.0f;
};

class ProceduralTerrainGenerator {
public:
    static TerrainGenerationSettings CreateSettingsFromWorldBuildSpec(const WorldBuildSpec& spec);
    static TerrainGenerationResult CreateFromWorldBuildSpec(const WorldBuildSpec& spec);
    static TerrainGenerationResult CreateOpenWorldLandscape(const TerrainGenerationSettings& settings);
};

} // namespace Moon
