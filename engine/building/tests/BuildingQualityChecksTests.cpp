#include <gtest/gtest.h>

#include "building/BuildingPipeline.h"
#include "building/BuildingQualityChecks.h"
#include "core/Assets/AssetPaths.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace {

std::string ReadUtf8TextFile(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }
    return content;
}

Moon::Building::GeneratedBuilding BuildRepresentativeAsset(const std::string& path) {
    const std::string json = ReadUtf8TextFile(path);
    EXPECT_FALSE(json.empty()) << path;

    Moon::Building::BuildingPipeline pipeline;
    Moon::Building::GeneratedBuilding building;
    std::string error;
    EXPECT_TRUE(pipeline.ProcessBuilding(json, building, error)) << error;
    return building;
}

} // namespace

TEST(BuildingQualityChecksTests, RepresentativeAssetsPassQualityChecks) {
    const std::vector<std::string> assets = {
        Moon::Assets::BuildAssetPath("building/fixtures/villa.json"),
        Moon::Assets::BuildAssetPath("building/reference/midrise_apartment.json"),
        Moon::Assets::BuildAssetPath("building/fixtures/office_tower.json"),
        Moon::Assets::BuildAssetPath("building/fixtures/shopping_mall.json")
    };

    for (const std::string& path : assets) {
        SCOPED_TRACE(path);
        const Moon::Building::GeneratedBuilding building = BuildRepresentativeAsset(path);
        const auto report = Moon::Building::EvaluateBuildingQuality(building);
        std::ostringstream issues;
        for (const auto& error : report.errors) {
            issues << "[" << error.code << "] " << error.message << " floor=" << error.floorLevel << "\n";
        }
        EXPECT_TRUE(report.passed) << issues.str();
        EXPECT_TRUE(report.errors.empty()) << issues.str();
    }
}

TEST(BuildingQualityChecksTests, RemovingVerticalCirculationFailsQualityChecks) {
    auto building = BuildRepresentativeAsset(Moon::Assets::BuildAssetPath("building/fixtures/office_tower.json"));
    building.verticalTransports.clear();
    building.stairs.clear();

    const auto report = Moon::Building::EvaluateBuildingQuality(building);
    EXPECT_FALSE(report.passed);
    EXPECT_TRUE(report.HasErrorCode("missing_vertical_circulation"));
}

TEST(BuildingQualityChecksTests, DisconnectingFloorGraphFailsQualityChecks) {
    auto building = BuildRepresentativeAsset(Moon::Assets::BuildAssetPath("building/fixtures/villa.json"));
    building.connections.clear();

    const auto report = Moon::Building::EvaluateBuildingQuality(building);
    EXPECT_FALSE(report.passed);
    EXPECT_TRUE(report.HasErrorCode("disconnected_floor_graph"));
}

TEST(BuildingQualityChecksTests, OpenPlanFloorWithoutPartitionsDoesNotTriggerDisconnectedGraph) {
    Moon::Building::GeneratedBuilding building;
    building.definition.grid = 0.5f;

    Moon::Building::Floor floor;
    floor.level = 0;
    floor.floorHeight = 3.2f;

    Moon::Building::Space lobby;
    lobby.spaceId = 1;
    lobby.properties.usage = Moon::Building::SpaceUsage::Entrance;
    lobby.rects.push_back({ "lobby", {0.0f, 0.0f}, {4.0f, 4.0f} });

    Moon::Building::Space retail;
    retail.spaceId = 2;
    retail.properties.usage = Moon::Building::SpaceUsage::Office;
    retail.rects.push_back({ "retail", {4.0f, 0.0f}, {4.0f, 4.0f} });

    floor.spaces.push_back(lobby);
    floor.spaces.push_back(retail);
    building.definition.floors.push_back(floor);

    Moon::Building::FloorPlate plate;
    plate.floorLevel = 0;
    plate.origin = {0.0f, 0.0f};
    plate.size = {8.0f, 4.0f};
    building.floorPlates.push_back(plate);

    const auto report = Moon::Building::EvaluateBuildingQuality(building);
    EXPECT_FALSE(report.HasErrorCode("disconnected_floor_graph"));
}

TEST(BuildingQualityChecksTests, InvalidDoorAndWindowReferencesFailQualityChecks) {
    auto building = BuildRepresentativeAsset(Moon::Assets::BuildAssetPath("building/fixtures/villa.json"));
    ASSERT_FALSE(building.doors.empty());
    ASSERT_FALSE(building.windows.empty());

    building.doors.front().wallId = 999999;
    building.windows.front().wallId = -1;
    building.windows.front().spaceId = 999999;

    const auto report = Moon::Building::EvaluateBuildingQuality(building);
    EXPECT_FALSE(report.passed);
    EXPECT_TRUE(report.HasErrorCode("door_missing_wall"));
    EXPECT_TRUE(report.HasErrorCode("window_missing_wall"));
    EXPECT_TRUE(report.HasErrorCode("window_missing_space"));
}

TEST(BuildingQualityChecksTests, WindowMustStayOnHostWallAndWithinStory) {
    Moon::Building::GeneratedBuilding building;
    Moon::Building::Floor floor;
    floor.level = 0;
    floor.floorHeight = 3.5f;
    building.definition.floors.push_back(floor);

    Moon::Building::WallSegment wall;
    wall.wallId = 1;
    wall.start = {0.0f, 0.0f};
    wall.end = {4.0f, 0.0f};
    wall.type = Moon::Building::WallType::Exterior;
    wall.spaceId = 10;
    wall.neighborSpaceId = -1;
    wall.floorLevel = 0;
    wall.height = 3.2f;
    wall.thickness = 0.2f;
    building.walls.push_back(wall);

    Moon::Building::Window window;
    window.wallId = 1;
    window.position = {999.0f, 999.0f};
    window.width = 1.2f;
    window.height = 1.5f;
    window.sillHeight = 100.0f;
    window.floorLevel = 0;
    window.spaceId = 10;
    building.windows.push_back(window);

    Moon::Building::Space space;
    space.spaceId = 10;
    building.definition.floors.front().spaces.push_back(space);

    const auto report = Moon::Building::EvaluateBuildingQuality(building);
    EXPECT_FALSE(report.passed);
    EXPECT_TRUE(report.HasErrorCode("window_off_wall"));
    EXPECT_TRUE(report.HasErrorCode("window_exceeds_story_height"));
}

TEST(BuildingQualityChecksTests, DoorMustFitItsHostWall) {
    auto building = BuildRepresentativeAsset(Moon::Assets::BuildAssetPath("building/fixtures/villa.json"));
    ASSERT_FALSE(building.doors.empty());

    building.doors.front().width = 20.0f;

    const auto report = Moon::Building::EvaluateBuildingQuality(building);
    EXPECT_FALSE(report.passed);
    EXPECT_TRUE(report.HasErrorCode("door_exceeds_wall_span"));
}

TEST(BuildingQualityChecksTests, WallCannotExceedClearStoryHeight) {
    Moon::Building::GeneratedBuilding building;
    Moon::Building::Floor floor;
    floor.level = 0;
    floor.floorHeight = 3.5f;
    building.definition.floors.push_back(floor);

    Moon::Building::WallSegment wall;
    wall.wallId = 1;
    wall.start = {0.0f, 0.0f};
    wall.end = {4.0f, 0.0f};
    wall.type = Moon::Building::WallType::Exterior;
    wall.spaceId = 10;
    wall.neighborSpaceId = -1;
    wall.floorLevel = 0;
    wall.height = -1.0f;
    wall.thickness = 0.2f;
    building.walls.push_back(wall);

    const auto report = Moon::Building::EvaluateBuildingQuality(building);
    EXPECT_FALSE(report.passed);
    EXPECT_TRUE(report.HasErrorCode("wall_invalid_height"));
}

TEST(BuildingQualityChecksTests, ZeroLengthWallFailsQualityChecks) {
    Moon::Building::GeneratedBuilding building;
    Moon::Building::Floor floor;
    floor.level = 0;
    floor.floorHeight = 3.5f;
    building.definition.floors.push_back(floor);

    Moon::Building::WallSegment wall;
    wall.wallId = 1;
    wall.start = {2.0f, 2.0f};
    wall.end = {2.0f, 2.0f};
    wall.type = Moon::Building::WallType::Exterior;
    wall.spaceId = 10;
    wall.neighborSpaceId = -1;
    wall.floorLevel = 0;
    wall.height = 3.0f;
    wall.thickness = 0.2f;
    building.walls.push_back(wall);

    const auto report = Moon::Building::EvaluateBuildingQuality(building);
    EXPECT_FALSE(report.passed);
    EXPECT_TRUE(report.HasErrorCode("wall_zero_length"));
}

TEST(BuildingQualityChecksTests, ShortButNonZeroWallDoesNotTriggerZeroLengthRule) {
    Moon::Building::GeneratedBuilding building;
    Moon::Building::Floor floor;
    floor.level = 0;
    floor.floorHeight = 3.5f;
    building.definition.floors.push_back(floor);

    Moon::Building::WallSegment wall;
    wall.wallId = 1;
    wall.start = {0.0f, 0.0f};
    wall.end = {0.12f, 0.0f};
    wall.type = Moon::Building::WallType::Exterior;
    wall.spaceId = 10;
    wall.neighborSpaceId = -1;
    wall.floorLevel = 0;
    wall.height = 3.0f;
    wall.thickness = 0.2f;
    building.walls.push_back(wall);

    const auto report = Moon::Building::EvaluateBuildingQuality(building);
    EXPECT_FALSE(report.HasErrorCode("wall_zero_length"));
}

TEST(BuildingQualityChecksTests, ProgramOverflowFailsQualityChecks) {
    auto building = BuildRepresentativeAsset(Moon::Assets::BuildAssetPath("building/fixtures/villa.json"));
    ASSERT_FALSE(building.floorPlates.empty());
    building.floorPlates.front().size = {2.0f, 2.0f};
    building.floorPlates.front().outline.clear();
    building.floorPlates.front().envelopeOutline.clear();
    building.floorPlates.front().voids.clear();

    const auto report = Moon::Building::EvaluateBuildingQuality(building);
    EXPECT_FALSE(report.passed);
    EXPECT_TRUE(report.HasErrorCode("program_exceeds_floor_plate"));
}

TEST(BuildingQualityChecksTests, ColumnInsideShaftFailsQualityChecks) {
    auto building = BuildRepresentativeAsset(Moon::Assets::BuildAssetPath("building/office_dual_egress_tower_demo.json"));
    ASSERT_FALSE(building.verticalTransports.empty());

    Moon::Building::SupportColumn column;
    column.columnId = "bad_column";
    column.center = building.verticalTransports.front().shaftRect.origin;
    column.center[0] += building.verticalTransports.front().shaftRect.size[0] * 0.5f;
    column.center[1] += building.verticalTransports.front().shaftRect.size[1] * 0.5f;
    column.floorFrom = building.verticalTransports.front().floorFrom;
    column.floorTo = building.verticalTransports.front().floorTo;
    column.width = 0.4f;
    column.depth = 0.4f;
    building.supportColumns.push_back(column);

    const auto report = Moon::Building::EvaluateBuildingQuality(building);
    EXPECT_FALSE(report.passed);
    EXPECT_TRUE(report.HasErrorCode("column_inside_shaft"));
}
