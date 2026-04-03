#include <gtest/gtest.h>
#include "building/BuildingToObjectBlueprintConverter.h"
#include "building/BuildingPipeline.h"
#include "building/SchemaValidator.h"
#include "TestHelpers.h"
#include "json.hpp"
#include <chrono>
#include <functional>

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

TEST_F(BuildingToObjectBlueprintConverterTest, StraightStairsEmitProceduralStairNodes) {
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

    bool foundProceduralStair = false;
    for (const auto& child : j["root"]["children"]) {
        if (!child.is_object() || !child.contains("type")) {
            continue;
        }
        if (child["type"] == "stair") {
            foundProceduralStair = true;
            break;
        }
    }

    EXPECT_TRUE(foundProceduralStair) << "Expected building converter to emit procedural stair nodes";
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


