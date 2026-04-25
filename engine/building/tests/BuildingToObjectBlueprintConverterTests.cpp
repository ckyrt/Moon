#include <gtest/gtest.h>
#include "building/BuildingToObjectBlueprintConverter.h"
#include "building/BuildingPipeline.h"
#include "building/SchemaValidator.h"
#include "core/Assets/AssetPaths.h"
#include "core/CSG/CSGBuilder.h"
#include "core/Object/Blueprint.h"
#include "core/Object/BlueprintLoader.h"
#include "TestHelpers.h"
#include "json.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <unordered_map>

using namespace Moon::Building;
using namespace Moon::Building::Test;
using json = nlohmann::json;

/**
 * @brief Test fixture for BuildingToObjectBlueprintConverter
 */
class BuildingToObjectBlueprintConverterTest : public ::testing::Test {
protected:
    BuildingPipeline pipeline;
    
    // Helper to validate JSON structure
    bool IsValidJSON(const std::string& jsonStr) {
        try {
            json j = json::parse(jsonStr);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    // Helper to check if JSON has required object blueprint fields
    bool HasObjectBlueprintFields(const std::string& jsonStr) {
        try {
            json j = json::parse(jsonStr);
            return j.contains("schema_version") && 
                   j.contains("root") &&
                   j["root"].is_object();
        } catch (...) {
            return false;
        }
    }
};

namespace {

bool PointInsideRect(const GridPos2D& point, const Rect& rect) {
    return point[0] > rect.origin[0] &&
           point[0] < rect.origin[0] + rect.size[0] &&
           point[1] > rect.origin[1] &&
           point[1] < rect.origin[1] + rect.size[1];
}

const Space* FindSpaceById(const BuildingDefinition& definition, int floorLevel, int spaceId) {
    for (const auto& floor : definition.floors) {
        if (floor.level != floorLevel) {
            continue;
        }
        for (const auto& space : floor.spaces) {
            if (space.spaceId == spaceId) {
                return &space;
            }
        }
    }
    return nullptr;
}

bool PointInsideSpaceRects(const GridPos2D& point, const Space& space) {
    for (const auto& rect : space.rects) {
        if (point[0] > rect.origin[0] + 0.01f &&
            point[0] < rect.origin[0] + rect.size[0] - 0.01f &&
            point[1] > rect.origin[1] + 0.01f &&
            point[1] < rect.origin[1] + rect.size[1] - 0.01f) {
            return true;
        }
    }
    return false;
}

GridPos2D GetExpectedWallPlacementOffset(const GeneratedBuilding& building, const WallSegment& wall) {
    if (wall.type != WallType::Exterior) {
        return {0.0f, 0.0f};
    }

    const Space* owningSpace = FindSpaceById(building.definition, wall.floorLevel, wall.spaceId);
    if (!owningSpace) {
        return {0.0f, 0.0f};
    }

    const float dx = wall.end[0] - wall.start[0];
    const float dz = wall.end[1] - wall.start[1];
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length < 0.001f) {
        return {0.0f, 0.0f};
    }

    const GridPos2D midpoint = {(wall.start[0] + wall.end[0]) * 0.5f, (wall.start[1] + wall.end[1]) * 0.5f};
    const GridPos2D leftNormal = {-dz / length, dx / length};
    const GridPos2D rightNormal = {dz / length, -dx / length};
    const float probeDistance = std::min(0.15f, std::max(0.05f, wall.thickness));

    const GridPos2D leftProbe = {midpoint[0] + leftNormal[0] * probeDistance, midpoint[1] + leftNormal[1] * probeDistance};
    if (PointInsideSpaceRects(leftProbe, *owningSpace)) {
        return {leftNormal[0] * wall.thickness * 0.5f, leftNormal[1] * wall.thickness * 0.5f};
    }

    const GridPos2D rightProbe = {midpoint[0] + rightNormal[0] * probeDistance, midpoint[1] + rightNormal[1] * probeDistance};
    if (PointInsideSpaceRects(rightProbe, *owningSpace)) {
        return {rightNormal[0] * wall.thickness * 0.5f, rightNormal[1] * wall.thickness * 0.5f};
    }

    return {0.0f, 0.0f};
}

const json* FindNodeByNameRecursive(const json& node, const std::string& name) {
    if (!node.is_object()) {
        return nullptr;
    }
    if (node.value("name", std::string()) == name) {
        return &node;
    }
    if (node.contains("left")) {
        if (const json* found = FindNodeByNameRecursive(node["left"], name)) {
            return found;
        }
    }
    if (node.contains("right")) {
        if (const json* found = FindNodeByNameRecursive(node["right"], name)) {
            return found;
        }
    }
    if (node.contains("children") && node["children"].is_array()) {
        for (const auto& child : node["children"]) {
            if (const json* found = FindNodeByNameRecursive(child, name)) {
                return found;
            }
        }
    }
    return nullptr;
}

void CollectNodesByPrefixRecursive(const json& node,
                                   const std::string& prefix,
                                   std::vector<const json*>& outNodes) {
    if (!node.is_object()) {
        return;
    }

    const std::string name = node.value("name", std::string());
    if (name.rfind(prefix, 0) == 0) {
        outNodes.push_back(&node);
    }

    if (node.contains("left")) {
        CollectNodesByPrefixRecursive(node["left"], prefix, outNodes);
    }
    if (node.contains("right")) {
        CollectNodesByPrefixRecursive(node["right"], prefix, outNodes);
    }
    if (node.contains("children") && node["children"].is_array()) {
        for (const auto& child : node["children"]) {
            CollectNodesByPrefixRecursive(child, prefix, outNodes);
        }
    }
}

class PreloadedBlueprintDatabase : public Moon::Object::BlueprintDatabase {
public:
    bool LoadObjectIndex(std::string& outError) {
        return LoadIndex(Moon::Assets::BuildObjectPath("index.json"), outError);
    }
};

} // namespace

// ========================================
// Basic Conversion Tests
// ========================================

TEST_F(BuildingToObjectBlueprintConverterTest, SimpleRoom_ValidObjectBlueprintOutput) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    EXPECT_FALSE(csgJson.empty()) << "object blueprint JSON should not be empty";
    EXPECT_TRUE(IsValidJSON(csgJson)) << "Output should be valid JSON";
    EXPECT_TRUE(HasObjectBlueprintFields(csgJson)) << "Should have object blueprint required fields";
}

TEST_F(BuildingToObjectBlueprintConverterTest, MultiRoom_ValidObjectBlueprintOutput) {
    std::string inputJson = TestHelpers::CreateLShapedBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    EXPECT_FALSE(csgJson.empty());
    EXPECT_TRUE(IsValidJSON(csgJson));
    EXPECT_TRUE(HasObjectBlueprintFields(csgJson));
}

TEST_F(BuildingToObjectBlueprintConverterTest, MultiFloor_ValidObjectBlueprintOutput) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    EXPECT_FALSE(csgJson.empty());
    EXPECT_TRUE(IsValidJSON(csgJson));
    EXPECT_TRUE(HasObjectBlueprintFields(csgJson));
}

// ========================================
// Structure Validation Tests
// ========================================

TEST_F(BuildingToObjectBlueprintConverterTest, OutputHasSchemaVersion) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    json j = json::parse(csgJson);
    EXPECT_TRUE(j.contains("schema_version") || j.contains("version"));
    if (j.contains("schema_version")) {
        EXPECT_TRUE(j["schema_version"].is_number());
    }
}

TEST_F(BuildingToObjectBlueprintConverterTest, OutputHasRoot) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    json j = json::parse(csgJson);
    EXPECT_TRUE(j.contains("root"));
    EXPECT_TRUE(j["root"].is_object() || j["root"].is_string());
}

TEST_F(BuildingToObjectBlueprintConverterTest, OutputHasChildren) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    json j = json::parse(csgJson);
    EXPECT_TRUE(j.contains("root"));
    if (j["root"].is_object() && j["root"].contains("children")) {
        EXPECT_TRUE(j["root"]["children"].is_array());
    }
}

// ========================================
// Content Tests
// ========================================

TEST_F(BuildingToObjectBlueprintConverterTest, WallsIncludedInOutput) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_GT(building.walls.size(), 0) << "Building should have walls";
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    // Check that CSG output is not trivial
    EXPECT_GT(csgJson.length(), 100) << "CSG output should be substantial";
}

TEST_F(BuildingToObjectBlueprintConverterTest, WindowsHandledInOutput) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    
    // CSG converter should handle both buildings with and without windows
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    EXPECT_GT(csgJson.length(), 100) << "CSG output should be valid";
    EXPECT_TRUE(IsValidJSON(csgJson));
}

TEST_F(BuildingToObjectBlueprintConverterTest, DoorsHandledInOutput) {
    std::string inputJson = TestHelpers::CreateLShapedBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    // Doors should not break CSG generation
    EXPECT_TRUE(IsValidJSON(csgJson));
    EXPECT_GT(csgJson.length(), 100);
}

// ========================================
// Edge Cases
// ========================================

TEST_F(BuildingToObjectBlueprintConverterTest, EmptyBuilding_ValidOutput) {
    GeneratedBuilding emptyBuilding;
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(emptyBuilding);
    
    // Even empty building should produce valid JSON
    EXPECT_TRUE(IsValidJSON(csgJson)) << "Empty building should produce valid JSON";
}

TEST_F(BuildingToObjectBlueprintConverterTest, BuildingWithOnlyWalls_ValidOutput) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    
    // Clear windows and doors
    building.windows.clear();
    building.doors.clear();
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    EXPECT_TRUE(IsValidJSON(csgJson));
    EXPECT_FALSE(csgJson.empty());
}

TEST_F(BuildingToObjectBlueprintConverterTest, MultiFloorBuilding_ValidOutput) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    // Multi-floor should produce output
    EXPECT_GT(csgJson.length(), 100) << "Multi-floor building should have CSG";
    EXPECT_TRUE(IsValidJSON(csgJson));
}

TEST_F(BuildingToObjectBlueprintConverterTest, MultiFloorBuilding_EmitsSupportColumns) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;

    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    ASSERT_TRUE(j.contains("root"));
    ASSERT_TRUE(j["root"].contains("children"));

    bool foundSupportColumn = false;
    for (const auto& child : j["root"]["children"]) {
        if (child.contains("name") && child["name"].is_string()) {
            const std::string name = child["name"].get<std::string>();
            if (name.rfind("support_column_", 0) == 0) {
                foundSupportColumn = true;
                break;
            }
        }
    }

    EXPECT_TRUE(foundSupportColumn) << "Expected support columns for elevated floors";
}

TEST_F(BuildingToObjectBlueprintConverterTest, ComplexShoppingMall_EmitsPlannedColumns) {
    std::string inputJson = TestHelpers::CreateComplexShoppingMall();
    GeneratedBuilding building;
    std::string errorMsg;

    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;
    ASSERT_GT(building.supportColumns.size(), 0u);

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    bool foundSupportColumn = false;
    for (const auto& child : j["root"]["children"]) {
        if (child.contains("name") && child["name"].is_string()) {
            const std::string name = child["name"].get<std::string>();
            if (name.rfind("support_", 0) == 0) {
                foundSupportColumn = true;
                break;
            }
        }
    }

    EXPECT_TRUE(foundSupportColumn) << "Expected planned support columns for shopping mall";
}

TEST_F(BuildingToObjectBlueprintConverterTest, MassDrivenBuilding_DoesNotPlaceSupportColumnsInsideVerticalCores) {
    std::string inputJson = TestHelpers::CreateShoppingCenter();
    GeneratedBuilding building;
    std::string errorMsg;

    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;
    ASSERT_FALSE(building.verticalCores.empty()) << "Expected vertical cores";

    for (const auto& column : building.supportColumns) {
        for (const auto& core : building.verticalCores) {
            EXPECT_FALSE(PointInsideRect(column.center, core.rect))
                << "Support column '" << column.columnId << "' overlaps vertical core '"
                << core.coreId << "'";
        }
    }
}

TEST_F(BuildingToObjectBlueprintConverterTest, StraightStairsExportAsExplicitGeometry) {
    std::string inputJson = TestHelpers::CreateShoppingCenter();
    GeneratedBuilding building;
    std::string errorMsg;

    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;
    ASSERT_FALSE(building.stairs.empty()) << "Expected generated stairs";

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    ASSERT_TRUE(j.contains("root"));
    ASSERT_TRUE(j["root"].contains("children"));

    std::function<bool(const json&)> containsProceduralStair = [&](const json& node) {
        if (node.is_object()) {
            if (node.contains("type") && node["type"].is_string() &&
                node["type"].get<std::string>() == "stair") {
                return true;
            }
            if (node.contains("children") && node["children"].is_array()) {
                for (const auto& child : node["children"]) {
                    if (containsProceduralStair(child)) {
                        return true;
                    }
                }
            }
        } else if (node.is_array()) {
            for (const auto& child : node) {
                if (containsProceduralStair(child)) {
                    return true;
                }
            }
        }
        return false;
    };

    std::function<int(const json&)> countCubeNodes = [&](const json& node) {
        int count = 0;
        if (node.is_object()) {
            if (node.contains("type") && node["type"].is_string() &&
                node["type"].get<std::string>() == "primitive" &&
                node.contains("primitive") && node["primitive"].is_string() &&
                node["primitive"].get<std::string>() == "cube") {
                ++count;
            }
            if (node.contains("children") && node["children"].is_array()) {
                for (const auto& child : node["children"]) {
                    count += countCubeNodes(child);
                }
            }
        } else if (node.is_array()) {
            for (const auto& child : node) {
                count += countCubeNodes(child);
            }
        }
        return count;
    };

    EXPECT_FALSE(containsProceduralStair(j["root"]))
        << "Expected stair export to use explicit geometry instead of procedural stair nodes";
    EXPECT_GT(countCubeNodes(j["root"]), 0)
        << "Expected stair export to contribute explicit cube geometry";
}

TEST_F(BuildingToObjectBlueprintConverterTest, StairsExportAsExplicitGeometryInsteadOfProceduralNodes) {
    std::string inputJson = TestHelpers::LoadFromFile("office_dual_egress_tower_demo.json");
    GeneratedBuilding building;
    std::string errorMsg;

    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    ASSERT_TRUE(j.contains("root"));
    ASSERT_TRUE(j["root"].contains("children"));

    std::function<bool(const json&)> containsProceduralStair = [&](const json& node) {
        if (node.is_object()) {
            if (node.contains("type") && node["type"].is_string() &&
                node["type"].get<std::string>() == "stair") {
                return true;
            }
            if (node.contains("children") && node["children"].is_array()) {
                for (const auto& child : node["children"]) {
                    if (containsProceduralStair(child)) {
                        return true;
                    }
                }
            }
            if (node.contains("left") && containsProceduralStair(node["left"])) {
                return true;
            }
            if (node.contains("right") && containsProceduralStair(node["right"])) {
                return true;
            }
        }
        return false;
    };

    EXPECT_FALSE(containsProceduralStair(j["root"]));
}

TEST_F(BuildingToObjectBlueprintConverterTest, ElevatorShaftsEmitCabinReferences) {
    const std::string inputJson = TestHelpers::LoadFromFile("office_enclosed_core_demo.json");
    GeneratedBuilding building;
    std::string errorMsg;

    const bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;
    ASSERT_FALSE(building.verticalTransports.empty()) << "Expected vertical transports";

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    ASSERT_TRUE(j.contains("root"));
    ASSERT_TRUE(j["root"].contains("children"));

    bool foundElevatorCabin = false;
    for (const auto& child : j["root"]["children"]) {
        if (!child.is_object() || !child.contains("ref")) {
            continue;
        }

        if (child["ref"] == "elevator_cabin_v1") {
            foundElevatorCabin = true;
            break;
        }
    }

    EXPECT_TRUE(foundElevatorCabin) << "Expected building converter to emit an elevator cabin object";
}

TEST_F(BuildingToObjectBlueprintConverterTest, OutputDoesNotContainProgramPreviewBlocks) {
    std::string inputJson = TestHelpers::CreateShoppingCenter();
    GeneratedBuilding building;
    std::string errorMsg;

    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;
    ASSERT_GT(building.programBlocks.size(), 0u) << "Fixture should still generate debug program blocks upstream";

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    ASSERT_TRUE(j.contains("root"));
    ASSERT_TRUE(j["root"].contains("children"));

    for (const auto& child : j["root"]["children"]) {
        if (!child.is_object() || !child.contains("name") || !child["name"].is_string()) {
            continue;
        }

        const std::string name = child["name"].get<std::string>();
        EXPECT_TRUE(name.rfind("program_", 0) != 0)
            << "Final building output should not contain debug preview blocks: " << name;
    }
}

TEST_F(BuildingToObjectBlueprintConverterTest, FloorPlatesWithOutlineEmitClippedCsgSlabs) {
    GeneratedBuilding building;
    building.definition.grid = 0.5f;

    Floor floor;
    floor.level = 0;
    floor.floorHeight = 4.0f;
    building.definition.floors.push_back(floor);

    FloorPlate plate;
    plate.floorLevel = 0;
    plate.origin = {0.0f, 0.0f};
    plate.size = {8.0f, 8.0f};
    plate.outline = {
        GridPos2D{0.0f, 0.0f},
        GridPos2D{8.0f, 0.0f},
        GridPos2D{6.0f, 6.0f},
        GridPos2D{0.0f, 8.0f}
    };
    building.floorPlates.push_back(plate);

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    ASSERT_TRUE(j.contains("root"));
    ASSERT_TRUE(j["root"].contains("children"));
    ASSERT_FALSE(j["root"]["children"].empty());

    const json& floorNode = j["root"]["children"][0];
    ASSERT_TRUE(floorNode.is_object());
    EXPECT_EQ(floorNode.value("type", ""), "csg");
    EXPECT_EQ(floorNode.value("operation", ""), "subtract");
}

TEST_F(BuildingToObjectBlueprintConverterTest, FloorPlatesPreferEnvelopeOutlineForSlabShape) {
    GeneratedBuilding building;
    building.definition.grid = 0.5f;

    Floor floor;
    floor.level = 0;
    floor.floorHeight = 4.0f;
    building.definition.floors.push_back(floor);

    FloorPlate plate;
    plate.floorLevel = 0;
    plate.origin = {0.0f, 0.0f};
    plate.size = {8.0f, 8.0f};
    plate.envelopeOutline = {
        GridPos2D{0.0f, 0.0f},
        GridPos2D{10.0f, 0.0f},
        GridPos2D{10.0f, 10.0f},
        GridPos2D{0.0f, 10.0f}
    };
    plate.outline = {
        GridPos2D{2.0f, 2.0f},
        GridPos2D{8.0f, 2.0f},
        GridPos2D{8.0f, 8.0f},
        GridPos2D{2.0f, 8.0f}
    };
    building.floorPlates.push_back(plate);

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    const json& floorNode = j["root"]["children"][0];
    ASSERT_TRUE(floorNode.is_object());
    std::function<const json*(const json&)> findBboxNode = [&](const json& node) -> const json* {
        if (!node.is_object()) {
            return nullptr;
        }
        if (node.value("name", std::string()).find("_bbox") != std::string::npos) {
            return &node;
        }
        if (node.contains("left")) {
            if (const json* found = findBboxNode(node["left"])) {
                return found;
            }
        }
        if (node.contains("right")) {
            if (const json* found = findBboxNode(node["right"])) {
                return found;
            }
        }
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (const json* found = findBboxNode(child)) {
                    return found;
                }
            }
        }
        return nullptr;
    };

    const json* bboxNode = findBboxNode(floorNode);
    ASSERT_NE(bboxNode, nullptr);
    ASSERT_TRUE(bboxNode->contains("params"));
    EXPECT_FLOAT_EQ((*bboxNode)["params"]["size_x"].get<float>(), 1000.0f);
    EXPECT_FLOAT_EQ((*bboxNode)["params"]["size_z"].get<float>(), 1000.0f);
}

TEST_F(BuildingToObjectBlueprintConverterTest, WallsAndOpeningsStartAboveFallbackSlab) {
    GeneratedBuilding building;

    Floor floor;
    floor.level = 0;
    floor.floorHeight = 4.0f;
    building.definition.floors.push_back(floor);

    WallSegment wall;
    wall.wallId = 1;
    wall.start = {0.0f, 0.0f};
    wall.end = {4.0f, 0.0f};
    wall.type = WallType::Exterior;
    wall.spaceId = 10;
    wall.neighborSpaceId = -1;
    wall.floorLevel = 0;
    wall.height = 3.0f;
    wall.thickness = 0.2f;
    building.walls.push_back(wall);

    Door door;
    door.wallId = 1;
    door.position = {2.0f, 0.0f};
    door.rotation = 0.0f;
    door.type = DoorType::Entrance;
    door.width = 1.0f;
    door.height = 2.1f;
    door.spaceA = 10;
    door.spaceB = -1;
    door.floorLevel = 0;
    building.doors.push_back(door);

    Window window;
    window.wallId = 1;
    window.position = {1.0f, 0.0f};
    window.rotation = 0.0f;
    window.width = 1.2f;
    window.height = 1.5f;
    window.sillHeight = 0.9f;
    window.floorLevel = 0;
    window.spaceId = 10;
    building.windows.push_back(window);

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    const json* wallPanelNode = FindNodeByNameRecursive(j["root"], "wall_panel_0");
    const json* wallNode = FindNodeByNameRecursive(j["root"], "wall_0");
    const json* doorNode = FindNodeByNameRecursive(j["root"], "door_0");
    const json* windowNode = FindNodeByNameRecursive(j["root"], "window_0");

    ASSERT_NE(wallPanelNode, nullptr);
    ASSERT_NE(doorNode, nullptr);
    ASSERT_NE(windowNode, nullptr);

    EXPECT_FLOAT_EQ((*wallPanelNode)["transform"]["position"][1].get<float>(), 5.0f);
    ASSERT_NE(wallNode, nullptr);
    EXPECT_FLOAT_EQ((*wallNode)["size"][1].get<float>(), 3.95f);
    EXPECT_FLOAT_EQ((*doorNode)["transform"]["position"][1].get<float>(), 5.0f);
    EXPECT_FLOAT_EQ((*windowNode)["transform"]["position"][1].get<float>(), 95.0f);
}

TEST_F(BuildingToObjectBlueprintConverterTest, WallsAndOpeningsStartAboveMassFloorPlateSlab) {
    GeneratedBuilding building;

    Floor floor;
    floor.level = 0;
    floor.floorHeight = 4.0f;
    building.definition.floors.push_back(floor);

    FloorPlate plate;
    plate.floorLevel = 0;
    plate.origin = {0.0f, 0.0f};
    plate.size = {6.0f, 6.0f};
    plate.envelopeOutline = {
        GridPos2D{0.0f, 0.0f},
        GridPos2D{6.0f, 0.0f},
        GridPos2D{6.0f, 6.0f},
        GridPos2D{0.0f, 6.0f}
    };
    plate.outline = plate.envelopeOutline;
    building.floorPlates.push_back(plate);

    WallSegment wall;
    wall.wallId = 1;
    wall.start = {0.0f, 0.0f};
    wall.end = {4.0f, 0.0f};
    wall.type = WallType::Exterior;
    wall.spaceId = 10;
    wall.neighborSpaceId = -1;
    wall.floorLevel = 0;
    wall.height = 3.0f;
    wall.thickness = 0.2f;
    building.walls.push_back(wall);

    Door door;
    door.wallId = 1;
    door.position = {2.0f, 0.0f};
    door.rotation = 0.0f;
    door.type = DoorType::Entrance;
    door.width = 1.0f;
    door.height = 2.1f;
    door.spaceA = 10;
    door.spaceB = -1;
    door.floorLevel = 0;
    building.doors.push_back(door);

    Window window;
    window.wallId = 1;
    window.position = {1.0f, 0.0f};
    window.rotation = 0.0f;
    window.width = 1.2f;
    window.height = 1.5f;
    window.sillHeight = 0.9f;
    window.floorLevel = 0;
    window.spaceId = 10;
    building.windows.push_back(window);

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    const json* wallPanelNode = FindNodeByNameRecursive(j["root"], "wall_panel_0");
    const json* wallNode = FindNodeByNameRecursive(j["root"], "wall_0");
    const json* doorNode = FindNodeByNameRecursive(j["root"], "door_0");
    const json* windowNode = FindNodeByNameRecursive(j["root"], "window_0");

    ASSERT_NE(wallPanelNode, nullptr);
    ASSERT_NE(doorNode, nullptr);
    ASSERT_NE(windowNode, nullptr);

    EXPECT_FLOAT_EQ((*wallPanelNode)["transform"]["position"][1].get<float>(), 18.0f);
    ASSERT_NE(wallNode, nullptr);
EXPECT_FLOAT_EQ((*wallNode)["size"][1].get<float>(), 3.82f);
EXPECT_FLOAT_EQ((*doorNode)["transform"]["position"][1].get<float>(), 18.0f);
EXPECT_FLOAT_EQ((*windowNode)["transform"]["position"][1].get<float>(), 108.0f);
}

TEST_F(BuildingToObjectBlueprintConverterTest, ShortRequestedWallsStillFillClearStoryHeight) {
    GeneratedBuilding building;

    Floor groundFloor;
    groundFloor.level = 0;
    groundFloor.floorHeight = 4.0f;
    building.definition.floors.push_back(groundFloor);

    Floor upperFloor;
    upperFloor.level = 1;
    upperFloor.floorHeight = 4.0f;
    building.definition.floors.push_back(upperFloor);

    FloorPlate groundPlate;
    groundPlate.floorLevel = 0;
    groundPlate.origin = {0.0f, 0.0f};
    groundPlate.size = {6.0f, 6.0f};
    groundPlate.envelopeOutline = {
        GridPos2D{0.0f, 0.0f},
        GridPos2D{6.0f, 0.0f},
        GridPos2D{6.0f, 6.0f},
        GridPos2D{0.0f, 6.0f}
    };
    groundPlate.outline = groundPlate.envelopeOutline;
    building.floorPlates.push_back(groundPlate);

    FloorPlate upperPlate = groundPlate;
    upperPlate.floorLevel = 1;
    building.floorPlates.push_back(upperPlate);

    WallSegment wall;
    wall.wallId = 1;
    wall.start = {0.0f, 0.0f};
    wall.end = {4.0f, 0.0f};
    wall.type = WallType::Exterior;
    wall.spaceId = 10;
    wall.neighborSpaceId = -1;
    wall.floorLevel = 0;
    wall.height = 3.1f;
    wall.thickness = 0.2f;
    building.walls.push_back(wall);

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    const json* wallNode = FindNodeByNameRecursive(j["root"], "wall_0");

    ASSERT_NE(wallNode, nullptr);
    EXPECT_FLOAT_EQ((*wallNode)["transform"]["position"][1].get<float>(), 18.0f);
    EXPECT_FLOAT_EQ((*wallNode)["size"][1].get<float>(), 3.82f);
}

TEST_F(BuildingToObjectBlueprintConverterTest, IntentionallyShortWallsStayShort) {
    GeneratedBuilding building;

    Floor floor;
    floor.level = 0;
    floor.floorHeight = 4.0f;
    building.definition.floors.push_back(floor);

    FloorPlate plate;
    plate.floorLevel = 0;
    plate.origin = {0.0f, 0.0f};
    plate.size = {6.0f, 6.0f};
    plate.envelopeOutline = {
        GridPos2D{0.0f, 0.0f},
        GridPos2D{6.0f, 0.0f},
        GridPos2D{6.0f, 6.0f},
        GridPos2D{0.0f, 6.0f}
    };
    plate.outline = plate.envelopeOutline;
    building.floorPlates.push_back(plate);

    WallSegment wall;
    wall.wallId = 1;
    wall.start = {0.0f, 0.0f};
    wall.end = {4.0f, 0.0f};
    wall.type = WallType::Exterior;
    wall.spaceId = 10;
    wall.neighborSpaceId = -1;
    wall.floorLevel = 0;
    wall.height = 1.2f;
    wall.thickness = 0.2f;
    building.walls.push_back(wall);

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    const json* wallNode = FindNodeByNameRecursive(j["root"], "wall_0");
    ASSERT_NE(wallNode, nullptr);
    EXPECT_FLOAT_EQ((*wallNode)["size"][1].get<float>(), 1.2f);
}

TEST_F(BuildingToObjectBlueprintConverterTest, ExteriorWallsOffsetInwardByHalfThickness) {
    GeneratedBuilding building;

    Floor floor;
    floor.level = 0;
    floor.floorHeight = 4.0f;

    Space space;
    space.spaceId = 10;
    space.rects.push_back({"room", {0.0f, 0.0f}, {4.0f, 4.0f}});
    floor.spaces.push_back(space);
    building.definition.floors.push_back(floor);

    FloorPlate plate;
    plate.floorLevel = 0;
    plate.origin = {0.0f, 0.0f};
    plate.size = {4.0f, 4.0f};
    plate.envelopeOutline = {
        GridPos2D{0.0f, 0.0f},
        GridPos2D{4.0f, 0.0f},
        GridPos2D{4.0f, 4.0f},
        GridPos2D{0.0f, 4.0f}
    };
    plate.outline = plate.envelopeOutline;
    building.floorPlates.push_back(plate);

    WallSegment wall;
    wall.wallId = 1;
    wall.start = {0.0f, 0.0f};
    wall.end = {4.0f, 0.0f};
    wall.type = WallType::Exterior;
    wall.spaceId = 10;
    wall.neighborSpaceId = -1;
    wall.floorLevel = 0;
    wall.height = 3.0f;
    wall.thickness = 0.2f;
    building.walls.push_back(wall);

    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    const json* wallNode = FindNodeByNameRecursive(j["root"], "wall_0");
    ASSERT_NE(wallNode, nullptr);

    EXPECT_FLOAT_EQ((*wallNode)["transform"]["position"][0].get<float>(), 200.0f);
    EXPECT_FLOAT_EQ((*wallNode)["transform"]["position"][2].get<float>(), 10.0f);
}

TEST_F(BuildingToObjectBlueprintConverterTest, TownhouseSegmentedOpenStair_WindowsUseHostWallPlacementOffset) {
    const std::string inputJson = TestHelpers::LoadFromFile("townhouse_segmented_open_stair_demo.json");
    ASSERT_FALSE(inputJson.empty());

    GeneratedBuilding building;
    std::string errorMsg;
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg)) << errorMsg;
    ASSERT_FALSE(building.windows.empty());

    const std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    std::vector<const json*> windowNodes;
    CollectNodesByPrefixRecursive(j["root"], "window_", windowNodes);
    ASSERT_EQ(windowNodes.size(), building.windows.size());

    std::unordered_map<int, const WallSegment*> wallsById;
    for (const auto& wall : building.walls) {
        wallsById[wall.wallId] = &wall;
    }

    for (size_t i = 0; i < building.windows.size(); ++i) {
        const Window& window = building.windows[i];
        SCOPED_TRACE(i);
        ASSERT_GE(window.wallId, 0);

        const auto wallIt = wallsById.find(window.wallId);
        ASSERT_NE(wallIt, wallsById.end());

        const GridPos2D offset = GetExpectedWallPlacementOffset(building, *wallIt->second);
        const json& node = *windowNodes[i];
        ASSERT_TRUE(node.contains("transform"));
        ASSERT_TRUE(node["transform"].contains("position"));

        EXPECT_NEAR(node["transform"]["position"][0].get<float>(), (window.position[0] + offset[0]) * 100.0f, 0.25f);
        EXPECT_NEAR(node["transform"]["position"][2].get<float>(), (window.position[1] + offset[1]) * 100.0f, 0.25f);
    }
}

TEST_F(BuildingToObjectBlueprintConverterTest, VoidedFloorLevelsFallbackToFloorPlateCsgWhenPreviewMeshIsMissing) {
    GeneratedBuilding building;

    Floor lowerFloor;
    lowerFloor.level = 0;
    lowerFloor.floorHeight = 3.6f;
    building.definition.floors.push_back(lowerFloor);

    Floor upperFloor;
    upperFloor.level = 1;
    upperFloor.floorHeight = 3.6f;
    building.definition.floors.push_back(upperFloor);

    FloorPlate lowerPlate;
    lowerPlate.floorLevel = 0;
    lowerPlate.origin = {0.0f, 0.0f};
    lowerPlate.size = {6.0f, 6.0f};
    lowerPlate.envelopeOutline = {
        GridPos2D{0.0f, 0.0f},
        GridPos2D{6.0f, 0.0f},
        GridPos2D{6.0f, 6.0f},
        GridPos2D{0.0f, 6.0f}
    };
    lowerPlate.outline = lowerPlate.envelopeOutline;
    building.floorPlates.push_back(lowerPlate);

    FloorPlate upperPlate = lowerPlate;
    upperPlate.floorLevel = 1;
    upperPlate.voids.push_back({"stair_opening", {2.0f, 2.0f}, {2.0f, 2.0f}});
    building.floorPlates.push_back(upperPlate);

    GeneratedMeshPart lowerMesh;
    lowerMesh.partId = "floor_plate_mesh_0";
    lowerMesh.material = "concrete_floor";
    building.floorPlateMeshes.push_back(std::move(lowerMesh));

    const std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json j = json::parse(csgJson);

    EXPECT_EQ(FindNodeByNameRecursive(j["root"], "floor_plate_0"), nullptr);
    EXPECT_NE(FindNodeByNameRecursive(j["root"], "floor_plate_1"), nullptr);
}

TEST_F(BuildingToObjectBlueprintConverterTest, ApartmentSingleStairDemo_BuiltWallMeshesStartAtSlabTop) {
    const std::string inputJson = TestHelpers::LoadFromFile("apartment_single_stair_demo.json");
    ASSERT_FALSE(inputJson.empty());

    GeneratedBuilding building;
    std::string errorMsg;
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg)) << errorMsg;

    const std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    std::string parseError;
    auto blueprint = Moon::Object::BlueprintLoader::ParseFromString(csgJson, parseError);
    ASSERT_TRUE(blueprint) << parseError;

    PreloadedBlueprintDatabase database;
    std::string indexError;
    ASSERT_TRUE(database.LoadObjectIndex(indexError)) << indexError;

    Moon::CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(&database);

    std::unordered_map<std::string, float> params;
    std::string buildError;
    const Moon::CSG::BuildResult result = builder.Build(blueprint.get(), params, buildError);
    ASSERT_FALSE(result.meshes.empty()) << buildError;

    std::vector<float> validBaseHeightsM;
    for (const auto& floor : building.definition.floors) {
        validBaseHeightsM.push_back(GetFloorBaseHeight(building.definition, floor.level) + 0.18f);
    }

    int checkedWallLikeMeshes = 0;
    for (const auto& meshItem : result.meshes) {
        if (!meshItem.mesh || !meshItem.mesh->IsValid()) {
            continue;
        }

        const auto& vertices = meshItem.mesh->GetVertices();
        if (vertices.empty()) {
            continue;
        }

        float minX = vertices.front().position.x;
        float maxX = vertices.front().position.x;
        float minY = vertices.front().position.y;
        float maxY = vertices.front().position.y;
        float minZ = vertices.front().position.z;
        float maxZ = vertices.front().position.z;
        for (const auto& vertex : vertices) {
            minX = std::min(minX, vertex.position.x);
            maxX = std::max(maxX, vertex.position.x);
            minY = std::min(minY, vertex.position.y);
            maxY = std::max(maxY, vertex.position.y);
            minZ = std::min(minZ, vertex.position.z);
            maxZ = std::max(maxZ, vertex.position.z);
        }

        const float spanX = (maxX - minX) * meshItem.worldTransform.scale.x;
        const float spanY = (maxY - minY) * meshItem.worldTransform.scale.y;
        const float spanZ = (maxZ - minZ) * meshItem.worldTransform.scale.z;
        const float length = std::max(spanX, spanZ);
        const float thickness = std::min(spanX, spanZ);
        const bool wallLike = spanY > 2.5f && length > 1.0f && thickness < 0.35f;
        if (!wallLike) {
            continue;
        }

        const float bottomY = minY * meshItem.worldTransform.scale.y + meshItem.worldTransform.position.y;
        const bool matchesSlabTop = std::any_of(validBaseHeightsM.begin(), validBaseHeightsM.end(),
            [&](float expectedY) { return std::abs(bottomY - expectedY) < 0.03f; });
        EXPECT_TRUE(matchesSlabTop)
            << "Wall-like mesh bottom at " << bottomY << "m"
            << " length=" << length << " thickness=" << thickness << " height=" << spanY;
        checkedWallLikeMeshes++;
    }

    EXPECT_GT(checkedWallLikeMeshes, 0);
}

// ========================================
// Integration Tests
// ========================================

TEST_F(BuildingToObjectBlueprintConverterTest, FullPipeline_SimpleRoom) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    // Full pipeline
    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << errorMsg;
    
    // Convert to CSG
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    // Validate CSG
    EXPECT_TRUE(IsValidJSON(csgJson));
    EXPECT_TRUE(HasObjectBlueprintFields(csgJson));
    
    // Parse and check structure
    json j = json::parse(csgJson);
    EXPECT_TRUE(j.contains("schema_version") || j.contains("version"));
    EXPECT_TRUE(j.contains("root"));
}

TEST_F(BuildingToObjectBlueprintConverterTest, FullPipeline_ComplexBuilding) {
    std::string inputJson = TestHelpers::CreateLShapedBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << errorMsg;
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    EXPECT_TRUE(IsValidJSON(csgJson));
    EXPECT_TRUE(HasObjectBlueprintFields(csgJson));
    EXPECT_GT(csgJson.length(), 100);
}

// ========================================
// Performance Tests
// ========================================

TEST_F(BuildingToObjectBlueprintConverterTest, LargeBuilding_ReasonablePerformance) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    
    // Measure conversion time
    auto start = std::chrono::high_resolution_clock::now();
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Conversion should be fast (< 100ms for reasonable building)
    EXPECT_LT(duration.count(), 100) << "Conversion should be fast";
    EXPECT_TRUE(IsValidJSON(csgJson));
}


