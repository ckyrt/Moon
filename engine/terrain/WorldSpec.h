#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Moon {

struct WorldBiomeSpec {
    std::string type;
    float coverage = 0.0f;
};

struct WorldLandmarkSpec {
    std::string type;
    float positionX = 0.5f;
    float positionY = 0.5f;
    float importance = 0.5f;
    float radiusMeters = 180.0f;
    float orientationDegrees = 0.0f;
};

struct WorldPromptSpec {
    uint32_t version = 1;
    uint32_t seed = 1337;
    std::string theme = "open_world_highland";
    std::string mood = "grounded";
    std::string climate = "temperate";
    std::string worldScale = "medium";
    std::string terrainStyle = "mountain_river_valley";
    std::string playStyle = "exploration_vehicle";
    std::string humanPresence = "sparse_rural";
    std::vector<WorldBiomeSpec> biomes;
    std::vector<WorldLandmarkSpec> landmarks;
    bool mustHaveLargeFlatArea = false;
    bool mustHaveMainRiver = true;
    bool avoidExtremeCliffs = false;
    bool preferVehicleTraversal = true;
    bool preferLongSightlines = true;
};

struct WorldBuildWorldSettings {
    float sizeMetersX = 4096.0f;
    float sizeMetersZ = 4096.0f;
    float playableAreaRatio = 0.82f;
    uint32_t heightmapResolution = 513;
    float seaLevel = 0.18f;
};

struct WorldBuildTerrainSettings {
    std::string style = "mountain_river_valley";
    float relief = 0.78f;
    float maxHeightMeters = 820.0f;
    float mountainDensity = 0.62f;
    float mountainScale = 0.74f;
    float hillDensity = 0.35f;
    float flatAreaRatio = 0.18f;
    float roughness = 0.58f;
    float erosionStrength = 0.64f;
    float terraceStrength = 0.0f;
    float cliffFrequency = 0.27f;
    float ridgeDirectionDegrees = 35.0f;
};

struct WorldBuildHydrologySettings {
    bool hasOcean = false;
    float oceanCoverage = 0.33f;
    uint32_t riverCount = 2;
    float riverWidthMinMeters = 18.0f;
    float riverWidthMaxMeters = 42.0f;
    float riverDepthMinMeters = 2.0f;
    float riverDepthMaxMeters = 6.0f;
    float riverMeander = 0.66f;
    uint32_t lakeCount = 1;
    float wetlandRatio = 0.06f;
};

struct WorldBuildHumanSettings {
    float settlementDensity = 0.24f;
    float roadDensity = 0.31f;
    std::string roadStyle = "winding_rural";
    uint32_t poiCount = 6;
    float openFieldRatio = 0.22f;
    float coverDensity = 0.48f;
};

struct WorldBuildVegetationSettings {
    float grassDensity = 0.72f;
    float grassHeight = 0.58f;
    float shrubDensity = 0.24f;
    float treeDensity = 0.36f;
    float treeLineHeight = 0.74f;
    float windResponse = 0.65f;
};

struct WorldBuildSurfaceSettings {
    float grassOnFlat = 0.82f;
    float rockOnSteep = 0.76f;
    float soilOnMidSlope = 0.42f;
    float sandNearWater = 0.33f;
    float snowLine = 0.88f;
};

struct WorldBuildAtmosphereSettings {
    std::string timeOfDay = "morning";
    std::string weather = "clear";
    float fog = 0.10f;
    float wind = 0.42f;
};

struct WorldBuildConstraintSettings {
    bool mustHaveLargeFlatArea = false;
    bool mustHaveMainRiver = true;
    bool avoidExtremeCliffs = false;
    bool preferVehicleTraversal = true;
    bool preferLongSightlines = true;
};

struct WorldBuildSpec {
    uint32_t version = 1;
    uint32_t seed = 1337;
    WorldBuildWorldSettings world;
    WorldBuildTerrainSettings terrain;
    WorldBuildHydrologySettings hydrology;
    std::vector<WorldBiomeSpec> biomes;
    std::vector<WorldLandmarkSpec> landmarks;
    WorldBuildHumanSettings human;
    WorldBuildVegetationSettings vegetation;
    WorldBuildSurfaceSettings surface;
    WorldBuildAtmosphereSettings atmosphere;
    WorldBuildConstraintSettings constraints;
};

class WorldSpecIO {
public:
    static WorldPromptSpec CreateDefaultPromptSpec();
    static WorldBuildSpec BuildFromPrompt(const WorldPromptSpec& prompt);

    static bool LoadPromptSpecFromFile(const std::string& path, WorldPromptSpec& outSpec, std::string* outError = nullptr);
    static bool SavePromptSpecToFile(const std::string& path, const WorldPromptSpec& spec, std::string* outError = nullptr);

    static bool LoadBuildSpecFromFile(const std::string& path, WorldBuildSpec& outSpec, std::string* outError = nullptr);
    static bool SaveBuildSpecToFile(const std::string& path, const WorldBuildSpec& spec, std::string* outError = nullptr);
};

} // namespace Moon
