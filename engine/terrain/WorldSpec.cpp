#include "WorldSpec.h"

#include "../../external/nlohmann/json.hpp"

#include <algorithm>
#include <fstream>

namespace Moon {

namespace {

using json = nlohmann::json;

float Clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

float ClampPositive(float value, float fallback)
{
    return value > 0.0f ? value : fallback;
}

uint32_t ChooseResolutionForScale(const std::string& worldScale)
{
    if (worldScale == "small") {
        return 257;
    }
    if (worldScale == "large") {
        return 1025;
    }
    return 513;
}

float ChooseSizeForScale(const std::string& worldScale)
{
    if (worldScale == "small") {
        return 2048.0f;
    }
    if (worldScale == "large") {
        return 8192.0f;
    }
    return 4096.0f;
}

void ApplyTerrainStyleDefaults(WorldBuildSpec& build, const std::string& terrainStyle)
{
    if (terrainStyle == "coastal_island") {
        build.hydrology.hasOcean = true;
        build.hydrology.oceanCoverage = 0.36f;
        build.hydrology.riverCount = 1;
        build.terrain.flatAreaRatio = 0.24f;
        build.terrain.mountainDensity = 0.38f;
        build.terrain.roughness = 0.42f;
        build.terrain.ridgeDirectionDegrees = 22.0f;
    } else if (terrainStyle == "mountain_river_valley") {
        build.hydrology.hasOcean = false;
        build.hydrology.riverCount = 2;
        build.terrain.flatAreaRatio = 0.18f;
        build.terrain.mountainDensity = 0.62f;
        build.terrain.roughness = 0.58f;
        build.terrain.ridgeDirectionDegrees = 35.0f;
    } else if (terrainStyle == "rolling_highlands") {
        build.hydrology.hasOcean = false;
        build.hydrology.riverCount = 1;
        build.terrain.flatAreaRatio = 0.26f;
        build.terrain.mountainDensity = 0.40f;
        build.terrain.hillDensity = 0.58f;
        build.terrain.roughness = 0.36f;
        build.terrain.ridgeDirectionDegrees = 12.0f;
    } else if (terrainStyle == "arid_badlands") {
        build.hydrology.hasOcean = false;
        build.hydrology.riverCount = 0;
        build.terrain.flatAreaRatio = 0.10f;
        build.terrain.mountainDensity = 0.48f;
        build.terrain.roughness = 0.72f;
        build.terrain.cliffFrequency = 0.52f;
        build.surface.sandNearWater = 0.62f;
    }
}

void ApplyClimateDefaults(WorldBuildSpec& build, const std::string& climate)
{
    if (climate == "tropical") {
        build.vegetation.grassDensity = 0.84f;
        build.vegetation.treeDensity = 0.52f;
        build.atmosphere.fog = 0.16f;
    } else if (climate == "temperate") {
        build.vegetation.grassDensity = 0.72f;
        build.vegetation.treeDensity = 0.36f;
        build.atmosphere.fog = 0.10f;
    } else if (climate == "cold") {
        build.vegetation.grassDensity = 0.38f;
        build.vegetation.treeDensity = 0.22f;
        build.surface.snowLine = 0.72f;
        build.atmosphere.fog = 0.14f;
    } else if (climate == "arid") {
        build.vegetation.grassDensity = 0.18f;
        build.vegetation.treeDensity = 0.06f;
        build.surface.sandNearWater = 0.72f;
        build.atmosphere.fog = 0.03f;
    }
}

void ApplyPlayStyleDefaults(WorldBuildSpec& build, const std::string& playStyle)
{
    if (playStyle == "exploration_vehicle") {
        build.human.openFieldRatio = 0.28f;
        build.constraints.preferVehicleTraversal = true;
        build.constraints.preferLongSightlines = true;
        build.terrain.flatAreaRatio = std::max(build.terrain.flatAreaRatio, 0.20f);
        build.terrain.roughness = std::min(build.terrain.roughness, 0.60f);
    } else if (playStyle == "combat_sightlines") {
        build.human.coverDensity = 0.44f;
        build.constraints.preferLongSightlines = true;
        build.terrain.flatAreaRatio = std::max(build.terrain.flatAreaRatio, 0.16f);
    } else if (playStyle == "vertical_traversal") {
        build.constraints.avoidExtremeCliffs = false;
        build.terrain.cliffFrequency = std::max(build.terrain.cliffFrequency, 0.42f);
        build.terrain.relief = std::max(build.terrain.relief, 0.82f);
    }
}

void ApplyConstraintOverrides(WorldBuildSpec& build, const WorldPromptSpec& prompt)
{
    build.constraints.mustHaveLargeFlatArea = prompt.mustHaveLargeFlatArea;
    build.constraints.mustHaveMainRiver = prompt.mustHaveMainRiver;
    build.constraints.avoidExtremeCliffs = prompt.avoidExtremeCliffs;
    build.constraints.preferVehicleTraversal = prompt.preferVehicleTraversal;
    build.constraints.preferLongSightlines = prompt.preferLongSightlines;

    if (prompt.mustHaveLargeFlatArea) {
        build.terrain.flatAreaRatio = std::max(build.terrain.flatAreaRatio, 0.24f);
        build.human.openFieldRatio = std::max(build.human.openFieldRatio, 0.28f);
    }
    if (prompt.mustHaveMainRiver) {
        build.hydrology.riverCount = std::max(1u, build.hydrology.riverCount);
    }
    if (prompt.avoidExtremeCliffs) {
        build.terrain.cliffFrequency = std::min(build.terrain.cliffFrequency, 0.18f);
        build.terrain.roughness = std::min(build.terrain.roughness, 0.52f);
    }
    if (prompt.preferVehicleTraversal) {
        build.terrain.flatAreaRatio = std::max(build.terrain.flatAreaRatio, 0.20f);
        build.terrain.roughness = std::min(build.terrain.roughness, 0.56f);
    }
    if (prompt.preferLongSightlines) {
        build.human.openFieldRatio = std::max(build.human.openFieldRatio, 0.24f);
    }
}

template <typename T>
bool LoadJsonFile(const std::string& path, T& outSpec, std::string* outError)
{
    std::ifstream stream(path);
    if (!stream.is_open()) {
        if (outError) {
            *outError = "Failed to open file: " + path;
        }
        return false;
    }

    try {
        json document;
        stream >> document;
        outSpec = document.get<T>();
        return true;
    } catch (const std::exception& e) {
        if (outError) {
            *outError = e.what();
        }
        return false;
    }
}

template <typename T>
bool SaveJsonFile(const std::string& path, const T& spec, std::string* outError)
{
    std::ofstream stream(path);
    if (!stream.is_open()) {
        if (outError) {
            *outError = "Failed to open file for writing: " + path;
        }
        return false;
    }

    try {
        const json document = spec;
        stream << document.dump(2);
        return true;
    } catch (const std::exception& e) {
        if (outError) {
            *outError = e.what();
        }
        return false;
    }
}

} // namespace

void to_json(json& j, const WorldBiomeSpec& biome)
{
    j = json{{"type", biome.type}, {"coverage", biome.coverage}};
}

void from_json(const json& j, WorldBiomeSpec& biome)
{
    biome.type = j.value("type", std::string());
    biome.coverage = j.value("coverage", 0.0f);
}

void to_json(json& j, const WorldLandmarkSpec& landmark)
{
    j = json{
        {"type", landmark.type},
        {"position", {landmark.positionX, landmark.positionY}},
        {"importance", landmark.importance},
        {"radius_meters", landmark.radiusMeters},
        {"orientation_degrees", landmark.orientationDegrees}
    };
}

void from_json(const json& j, WorldLandmarkSpec& landmark)
{
    landmark.type = j.value("type", std::string());
    if (j.contains("position") && j["position"].is_array() && j["position"].size() >= 2) {
        landmark.positionX = j["position"][0].get<float>();
        landmark.positionY = j["position"][1].get<float>();
    }
    landmark.importance = j.value("importance", landmark.importance);
    landmark.radiusMeters = j.value("radius_meters", landmark.radiusMeters);
    landmark.orientationDegrees = j.value("orientation_degrees", landmark.orientationDegrees);
}

void to_json(json& j, const WorldPromptSpec& spec)
{
    j = json{
        {"version", spec.version},
        {"kind", "world_prompt_spec"},
        {"seed", spec.seed},
        {"theme", spec.theme},
        {"mood", spec.mood},
        {"climate", spec.climate},
        {"world_scale", spec.worldScale},
        {"terrain_style", spec.terrainStyle},
        {"play_style", spec.playStyle},
        {"human_presence", spec.humanPresence},
        {"biomes", spec.biomes},
        {"landmarks", spec.landmarks},
        {"constraints", {
            {"must_have_large_flat_area", spec.mustHaveLargeFlatArea},
            {"must_have_main_river", spec.mustHaveMainRiver},
            {"avoid_extreme_cliffs", spec.avoidExtremeCliffs},
            {"prefer_vehicle_traversal", spec.preferVehicleTraversal},
            {"prefer_long_sightlines", spec.preferLongSightlines}
        }}
    };
}

void from_json(const json& j, WorldPromptSpec& spec)
{
    spec.version = j.value("version", spec.version);
    spec.seed = j.value("seed", spec.seed);
    spec.theme = j.value("theme", spec.theme);
    spec.mood = j.value("mood", spec.mood);
    spec.climate = j.value("climate", spec.climate);
    spec.worldScale = j.value("world_scale", spec.worldScale);
    spec.terrainStyle = j.value("terrain_style", spec.terrainStyle);
    spec.playStyle = j.value("play_style", spec.playStyle);
    spec.humanPresence = j.value("human_presence", spec.humanPresence);
    spec.biomes = j.value("biomes", std::vector<WorldBiomeSpec>{});
    spec.landmarks = j.value("landmarks", std::vector<WorldLandmarkSpec>{});
    if (j.contains("constraints")) {
        const json& constraints = j["constraints"];
        spec.mustHaveLargeFlatArea = constraints.value("must_have_large_flat_area", spec.mustHaveLargeFlatArea);
        spec.mustHaveMainRiver = constraints.value("must_have_main_river", spec.mustHaveMainRiver);
        spec.avoidExtremeCliffs = constraints.value("avoid_extreme_cliffs", spec.avoidExtremeCliffs);
        spec.preferVehicleTraversal = constraints.value("prefer_vehicle_traversal", spec.preferVehicleTraversal);
        spec.preferLongSightlines = constraints.value("prefer_long_sightlines", spec.preferLongSightlines);
    }
}

void to_json(json& j, const WorldBuildSpec& spec)
{
    j = json{
        {"version", spec.version},
        {"kind", "world_build_spec"},
        {"seed", spec.seed},
        {"world", {
            {"size_m", {spec.world.sizeMetersX, spec.world.sizeMetersZ}},
            {"playable_area_ratio", spec.world.playableAreaRatio},
            {"heightmap_resolution", spec.world.heightmapResolution},
            {"sea_level", spec.world.seaLevel}
        }},
        {"terrain", {
            {"style", spec.terrain.style},
            {"relief", spec.terrain.relief},
            {"max_height_m", spec.terrain.maxHeightMeters},
            {"mountain_density", spec.terrain.mountainDensity},
            {"mountain_scale", spec.terrain.mountainScale},
            {"hill_density", spec.terrain.hillDensity},
            {"flat_area_ratio", spec.terrain.flatAreaRatio},
            {"roughness", spec.terrain.roughness},
            {"erosion_strength", spec.terrain.erosionStrength},
            {"terrace_strength", spec.terrain.terraceStrength},
            {"cliff_frequency", spec.terrain.cliffFrequency},
            {"ridge_direction_deg", spec.terrain.ridgeDirectionDegrees}
        }},
        {"hydrology", {
            {"has_ocean", spec.hydrology.hasOcean},
            {"ocean_coverage", spec.hydrology.oceanCoverage},
            {"river_count", spec.hydrology.riverCount},
            {"river_width_m", {spec.hydrology.riverWidthMinMeters, spec.hydrology.riverWidthMaxMeters}},
            {"river_depth_m", {spec.hydrology.riverDepthMinMeters, spec.hydrology.riverDepthMaxMeters}},
            {"river_meander", spec.hydrology.riverMeander},
            {"lake_count", spec.hydrology.lakeCount},
            {"wetland_ratio", spec.hydrology.wetlandRatio}
        }},
        {"biomes", spec.biomes},
        {"landmarks", spec.landmarks},
        {"human", {
            {"settlement_density", spec.human.settlementDensity},
            {"road_density", spec.human.roadDensity},
            {"road_style", spec.human.roadStyle},
            {"poi_count", spec.human.poiCount},
            {"open_field_ratio", spec.human.openFieldRatio},
            {"cover_density", spec.human.coverDensity}
        }},
        {"vegetation", {
            {"grass_density", spec.vegetation.grassDensity},
            {"grass_height", spec.vegetation.grassHeight},
            {"shrub_density", spec.vegetation.shrubDensity},
            {"tree_density", spec.vegetation.treeDensity},
            {"tree_line_height", spec.vegetation.treeLineHeight},
            {"wind_response", spec.vegetation.windResponse}
        }},
        {"surface", {
            {"grass_on_flat", spec.surface.grassOnFlat},
            {"rock_on_steep", spec.surface.rockOnSteep},
            {"soil_on_mid_slope", spec.surface.soilOnMidSlope},
            {"sand_near_water", spec.surface.sandNearWater},
            {"snow_line", spec.surface.snowLine}
        }},
        {"atmosphere", {
            {"time_of_day", spec.atmosphere.timeOfDay},
            {"weather", spec.atmosphere.weather},
            {"fog", spec.atmosphere.fog},
            {"wind", spec.atmosphere.wind}
        }},
        {"constraints", {
            {"must_have_large_flat_area", spec.constraints.mustHaveLargeFlatArea},
            {"must_have_main_river", spec.constraints.mustHaveMainRiver},
            {"avoid_extreme_cliffs", spec.constraints.avoidExtremeCliffs},
            {"prefer_vehicle_traversal", spec.constraints.preferVehicleTraversal},
            {"prefer_long_sightlines", spec.constraints.preferLongSightlines}
        }}
    };
}

void from_json(const json& j, WorldBuildSpec& spec)
{
    spec.version = j.value("version", spec.version);
    spec.seed = j.value("seed", spec.seed);
    if (j.contains("world")) {
        const json& world = j["world"];
        if (world.contains("size_m") && world["size_m"].is_array() && world["size_m"].size() >= 2) {
            spec.world.sizeMetersX = world["size_m"][0].get<float>();
            spec.world.sizeMetersZ = world["size_m"][1].get<float>();
        }
        spec.world.playableAreaRatio = world.value("playable_area_ratio", spec.world.playableAreaRatio);
        spec.world.heightmapResolution = world.value("heightmap_resolution", spec.world.heightmapResolution);
        spec.world.seaLevel = world.value("sea_level", spec.world.seaLevel);
    }
    if (j.contains("terrain")) {
        const json& terrain = j["terrain"];
        spec.terrain.style = terrain.value("style", spec.terrain.style);
        spec.terrain.relief = terrain.value("relief", spec.terrain.relief);
        spec.terrain.maxHeightMeters = terrain.value("max_height_m", spec.terrain.maxHeightMeters);
        spec.terrain.mountainDensity = terrain.value("mountain_density", spec.terrain.mountainDensity);
        spec.terrain.mountainScale = terrain.value("mountain_scale", spec.terrain.mountainScale);
        spec.terrain.hillDensity = terrain.value("hill_density", spec.terrain.hillDensity);
        spec.terrain.flatAreaRatio = terrain.value("flat_area_ratio", spec.terrain.flatAreaRatio);
        spec.terrain.roughness = terrain.value("roughness", spec.terrain.roughness);
        spec.terrain.erosionStrength = terrain.value("erosion_strength", spec.terrain.erosionStrength);
        spec.terrain.terraceStrength = terrain.value("terrace_strength", spec.terrain.terraceStrength);
        spec.terrain.cliffFrequency = terrain.value("cliff_frequency", spec.terrain.cliffFrequency);
        spec.terrain.ridgeDirectionDegrees = terrain.value("ridge_direction_deg", spec.terrain.ridgeDirectionDegrees);
    }
    if (j.contains("hydrology")) {
        const json& hydrology = j["hydrology"];
        spec.hydrology.hasOcean = hydrology.value("has_ocean", spec.hydrology.hasOcean);
        spec.hydrology.oceanCoverage = hydrology.value("ocean_coverage", spec.hydrology.oceanCoverage);
        spec.hydrology.riverCount = hydrology.value("river_count", spec.hydrology.riverCount);
        if (hydrology.contains("river_width_m") && hydrology["river_width_m"].is_array() && hydrology["river_width_m"].size() >= 2) {
            spec.hydrology.riverWidthMinMeters = hydrology["river_width_m"][0].get<float>();
            spec.hydrology.riverWidthMaxMeters = hydrology["river_width_m"][1].get<float>();
        }
        if (hydrology.contains("river_depth_m") && hydrology["river_depth_m"].is_array() && hydrology["river_depth_m"].size() >= 2) {
            spec.hydrology.riverDepthMinMeters = hydrology["river_depth_m"][0].get<float>();
            spec.hydrology.riverDepthMaxMeters = hydrology["river_depth_m"][1].get<float>();
        }
        spec.hydrology.riverMeander = hydrology.value("river_meander", spec.hydrology.riverMeander);
        spec.hydrology.lakeCount = hydrology.value("lake_count", spec.hydrology.lakeCount);
        spec.hydrology.wetlandRatio = hydrology.value("wetland_ratio", spec.hydrology.wetlandRatio);
    }
    spec.biomes = j.value("biomes", std::vector<WorldBiomeSpec>{});
    spec.landmarks = j.value("landmarks", std::vector<WorldLandmarkSpec>{});
    if (j.contains("human")) {
        const json& human = j["human"];
        spec.human.settlementDensity = human.value("settlement_density", spec.human.settlementDensity);
        spec.human.roadDensity = human.value("road_density", spec.human.roadDensity);
        spec.human.roadStyle = human.value("road_style", spec.human.roadStyle);
        spec.human.poiCount = human.value("poi_count", spec.human.poiCount);
        spec.human.openFieldRatio = human.value("open_field_ratio", spec.human.openFieldRatio);
        spec.human.coverDensity = human.value("cover_density", spec.human.coverDensity);
    }
    if (j.contains("vegetation")) {
        const json& vegetation = j["vegetation"];
        spec.vegetation.grassDensity = vegetation.value("grass_density", spec.vegetation.grassDensity);
        spec.vegetation.grassHeight = vegetation.value("grass_height", spec.vegetation.grassHeight);
        spec.vegetation.shrubDensity = vegetation.value("shrub_density", spec.vegetation.shrubDensity);
        spec.vegetation.treeDensity = vegetation.value("tree_density", spec.vegetation.treeDensity);
        spec.vegetation.treeLineHeight = vegetation.value("tree_line_height", spec.vegetation.treeLineHeight);
        spec.vegetation.windResponse = vegetation.value("wind_response", spec.vegetation.windResponse);
    }
    if (j.contains("surface")) {
        const json& surface = j["surface"];
        spec.surface.grassOnFlat = surface.value("grass_on_flat", spec.surface.grassOnFlat);
        spec.surface.rockOnSteep = surface.value("rock_on_steep", spec.surface.rockOnSteep);
        spec.surface.soilOnMidSlope = surface.value("soil_on_mid_slope", spec.surface.soilOnMidSlope);
        spec.surface.sandNearWater = surface.value("sand_near_water", spec.surface.sandNearWater);
        spec.surface.snowLine = surface.value("snow_line", spec.surface.snowLine);
    }
    if (j.contains("atmosphere")) {
        const json& atmosphere = j["atmosphere"];
        spec.atmosphere.timeOfDay = atmosphere.value("time_of_day", spec.atmosphere.timeOfDay);
        spec.atmosphere.weather = atmosphere.value("weather", spec.atmosphere.weather);
        spec.atmosphere.fog = atmosphere.value("fog", spec.atmosphere.fog);
        spec.atmosphere.wind = atmosphere.value("wind", spec.atmosphere.wind);
    }
    if (j.contains("constraints")) {
        const json& constraints = j["constraints"];
        spec.constraints.mustHaveLargeFlatArea = constraints.value("must_have_large_flat_area", spec.constraints.mustHaveLargeFlatArea);
        spec.constraints.mustHaveMainRiver = constraints.value("must_have_main_river", spec.constraints.mustHaveMainRiver);
        spec.constraints.avoidExtremeCliffs = constraints.value("avoid_extreme_cliffs", spec.constraints.avoidExtremeCliffs);
        spec.constraints.preferVehicleTraversal = constraints.value("prefer_vehicle_traversal", spec.constraints.preferVehicleTraversal);
        spec.constraints.preferLongSightlines = constraints.value("prefer_long_sightlines", spec.constraints.preferLongSightlines);
    }
}

WorldPromptSpec WorldSpecIO::CreateDefaultPromptSpec()
{
    WorldPromptSpec spec;
    spec.biomes = {
        {"grassland", 0.34f},
        {"forest", 0.26f},
        {"rocky_mountain", 0.22f},
        {"riverbank", 0.08f},
        {"bare_soil", 0.10f}
    };
    spec.landmarks = {
        {"main_mountain", 0.72f, 0.31f, 1.0f, 360.0f, 35.0f},
        {"lake", 0.48f, 0.55f, 0.7f, 220.0f, 0.0f},
        {"valley", 0.36f, 0.62f, 0.8f, 280.0f, 25.0f},
        {"town_plain", 0.18f, 0.74f, 0.9f, 240.0f, 0.0f}
    };
    return spec;
}

WorldBuildSpec WorldSpecIO::BuildFromPrompt(const WorldPromptSpec& prompt)
{
    WorldBuildSpec build;
    build.seed = prompt.seed;
    build.world.heightmapResolution = ChooseResolutionForScale(prompt.worldScale);
    build.world.sizeMetersX = ChooseSizeForScale(prompt.worldScale);
    build.world.sizeMetersZ = ChooseSizeForScale(prompt.worldScale);
    build.terrain.style = prompt.terrainStyle;
    build.biomes = prompt.biomes;
    build.landmarks = prompt.landmarks;

    ApplyTerrainStyleDefaults(build, prompt.terrainStyle);
    ApplyClimateDefaults(build, prompt.climate);
    ApplyPlayStyleDefaults(build, prompt.playStyle);
    ApplyConstraintOverrides(build, prompt);

    if (prompt.humanPresence == "minimal") {
        build.human.settlementDensity = 0.08f;
        build.human.roadDensity = 0.12f;
    } else if (prompt.humanPresence == "settled") {
        build.human.settlementDensity = 0.48f;
        build.human.roadDensity = 0.58f;
    }

    build.world.playableAreaRatio = Clamp01(build.world.playableAreaRatio);
    build.world.seaLevel = Clamp01(build.world.seaLevel);
    build.terrain.relief = Clamp01(build.terrain.relief);
    build.terrain.mountainDensity = Clamp01(build.terrain.mountainDensity);
    build.terrain.mountainScale = Clamp01(build.terrain.mountainScale);
    build.terrain.hillDensity = Clamp01(build.terrain.hillDensity);
    build.terrain.flatAreaRatio = Clamp01(build.terrain.flatAreaRatio);
    build.terrain.roughness = Clamp01(build.terrain.roughness);
    build.terrain.erosionStrength = Clamp01(build.terrain.erosionStrength);
    build.terrain.cliffFrequency = Clamp01(build.terrain.cliffFrequency);
    build.hydrology.oceanCoverage = Clamp01(build.hydrology.oceanCoverage);
    build.hydrology.riverMeander = Clamp01(build.hydrology.riverMeander);
    build.hydrology.wetlandRatio = Clamp01(build.hydrology.wetlandRatio);
    build.vegetation.grassDensity = Clamp01(build.vegetation.grassDensity);
    build.vegetation.grassHeight = Clamp01(build.vegetation.grassHeight);
    build.surface.grassOnFlat = Clamp01(build.surface.grassOnFlat);
    build.surface.rockOnSteep = Clamp01(build.surface.rockOnSteep);
    build.surface.soilOnMidSlope = Clamp01(build.surface.soilOnMidSlope);
    build.surface.sandNearWater = Clamp01(build.surface.sandNearWater);
    build.surface.snowLine = Clamp01(build.surface.snowLine);
    build.terrain.maxHeightMeters = ClampPositive(build.terrain.maxHeightMeters, 820.0f);
    build.hydrology.riverWidthMinMeters = ClampPositive(build.hydrology.riverWidthMinMeters, 18.0f);
    build.hydrology.riverWidthMaxMeters = std::max(build.hydrology.riverWidthMinMeters, build.hydrology.riverWidthMaxMeters);
    build.hydrology.riverDepthMinMeters = ClampPositive(build.hydrology.riverDepthMinMeters, 2.0f);
    build.hydrology.riverDepthMaxMeters = std::max(build.hydrology.riverDepthMinMeters, build.hydrology.riverDepthMaxMeters);
    return build;
}

bool WorldSpecIO::LoadPromptSpecFromFile(const std::string& path, WorldPromptSpec& outSpec, std::string* outError)
{
    return LoadJsonFile(path, outSpec, outError);
}

bool WorldSpecIO::SavePromptSpecToFile(const std::string& path, const WorldPromptSpec& spec, std::string* outError)
{
    return SaveJsonFile(path, spec, outError);
}

bool WorldSpecIO::LoadBuildSpecFromFile(const std::string& path, WorldBuildSpec& outSpec, std::string* outError)
{
    return LoadJsonFile(path, outSpec, outError);
}

bool WorldSpecIO::SaveBuildSpecToFile(const std::string& path, const WorldBuildSpec& spec, std::string* outError)
{
    return SaveJsonFile(path, spec, outError);
}

} // namespace Moon
