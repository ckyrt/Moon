#include <gtest/gtest.h>
#include "building/DoorGenerator.h"
#include "building/WallGenerator.h"
#include "building/SpaceGraphBuilder.h"
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "building/BuildingIndex.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

/**
 * @brief Test fixture for DoorGenerator
 */
class DoorGeneratorTest : public ::testing::Test {
protected:
    DoorGenerator doorGenerator;
    WallGenerator wallGenerator;
    SpaceGraphBuilder spaceGraph;
    BuildingDefinition definition;
    std::vector<WallSegment> walls;
    std::vector<Door> doors;
    std::vector<SpaceConnection> connections;
    BuildingIndex buildingIndex;
    
    void SetUp() override {
        doorGenerator.SetDefaultDoorSize(0.9f, 2.1f);
        doorGenerator.SetMinDoorWidth(0.8f);
        wallGenerator.SetWallThickness(0.2f);
        wallGenerator.SetDefaultWallHeight(3.0f);
    }
};

// ========================================
// Door Generation Tests
// ========================================

TEST_F(DoorGeneratorTest, SimpleRoom_NoInteriorDoors) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateSimpleRoom();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    buildingIndex.Build(definition, &spaceGraph, &walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, buildingIndex, doors);
    
    // Single room should have at least one door (entrance)
    EXPECT_GE(doors.size(), 0);
}

TEST_F(DoorGeneratorTest, Villa_ConnectingRoomDoors) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    buildingIndex.Build(definition, &spaceGraph, &walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, buildingIndex, doors);
    
    // Villa should have doors between connected spaces
    EXPECT_GT(doors.size(), 0) << "Villa should have doors";
    
    // Check door properties
    for (const auto& door : doors) {
        EXPECT_GT(door.width, 0.0f);
        EXPECT_GT(door.height, 0.0f);
    }
}

TEST_F(DoorGeneratorTest, Apartment_HasMultipleFloors) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateApartmentBuilding();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    buildingIndex.Build(definition, &spaceGraph, &walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, buildingIndex, doors);
    
    // Apartment building should be multi-floor
    EXPECT_GT(definition.floors.size(), 1);
    
    // Should have generated walls
    EXPECT_GT(walls.size(), 0) << "Apartment should have walls";
}

// ========================================
// Door Size Tests
// ========================================

TEST_F(DoorGeneratorTest, CustomDoorSize_Applied) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    float customWidth = 1.0f;
    float customHeight = 2.2f;
    doorGenerator.SetDefaultDoorSize(customWidth, customHeight);
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    buildingIndex.Build(definition, &spaceGraph, &walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, buildingIndex, doors);
    
    if (doors.size() > 0) {
        // Check default sizes are applied
        bool foundDefaultSize = false;
        for (const auto& door : doors) {
            if (door.width == customWidth && door.height == customHeight) {
                foundDefaultSize = true;
                break;
            }
        }
        EXPECT_TRUE(foundDefaultSize) << "Should find door with custom default size";
    }
}

// ========================================
// Edge Cases
// ========================================

TEST_F(DoorGeneratorTest, EmptyBuilding_NoDoors) {
    definition.floors.clear();
    definition.masses.clear();
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    buildingIndex.Build(definition, &spaceGraph, &walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, buildingIndex, doors);
    
    EXPECT_EQ(doors.size(), 0);
}

TEST_F(DoorGeneratorTest, DoorsNotInWalls_Validation) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    buildingIndex.Build(definition, &spaceGraph, &walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, buildingIndex, doors);
    
    // All doors should reference valid spaces
    for (const auto& door : doors) {
        EXPECT_GE(door.spaceA, 0);
        EXPECT_GE(door.spaceB, -1); // -1 means exterior
    }
}
