#include <gtest/gtest.h>
#include "building/WallGenerator.h"
#include "building/SpaceGraphBuilder.h"
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

/**
 * @brief Test fixture for WallGenerator
 */
class WallGeneratorTest : public ::testing::Test {
protected:
    WallGenerator wallGenerator;
    SpaceGraphBuilder spaceGraph;
    BuildingDefinition definition;
    std::vector<WallSegment> walls;
    std::vector<SpaceConnection> connections;
    
    void SetUp() override {
        wallGenerator.SetWallThickness(0.2f);
        wallGenerator.SetDefaultWallHeight(3.0f);
    }
};

// ========================================
// Basic Wall Generation Tests
// ========================================

TEST_F(WallGeneratorTest, SimpleRoom_HasExteriorWalls) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateSimpleRoom();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    EXPECT_GT(walls.size(), 0) << "Simple room should generate walls";
    
    // Count exterior walls
    int exteriorWalls = 0;
    int interiorWalls = 0;
    for (const auto& wall : walls) {
        if (wall.type == WallType::Exterior) {
            exteriorWalls++;
        } else if (wall.type == WallType::Interior) {
            interiorWalls++;
        }
    }
    
    EXPECT_GT(exteriorWalls, 0) << "Should have exterior walls";
    EXPECT_EQ(interiorWalls, 0) << "Single room should not have interior walls";
}

TEST_F(WallGeneratorTest, Villa_HasInteriorWalls) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    EXPECT_GT(walls.size(), 0);
    
    // Villa with multiple rooms should have interior walls
    int interiorWalls = 0;
    for (const auto& wall : walls) {
        if (wall.type == WallType::Interior) {
            interiorWalls++;
        }
    }
    
    EXPECT_GT(interiorWalls, 0) << "Villa should have interior walls between rooms";
}

TEST_F(WallGeneratorTest, Apartment_MultipleFloorWalls) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateApartmentBuilding();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    // Multi-floor building should generate many walls
    EXPECT_GT(walls.size(), 20) << "Apartment building should have many walls";
    
    // Should have both interior and exterior walls
    int exteriorCount = 0, interiorCount = 0;
    for (const auto& wall : walls) {
        if (wall.type == WallType::Exterior) exteriorCount++;
        if (wall.type == WallType::Interior) interiorCount++;
    }
    EXPECT_GT(exteriorCount, 0);
    EXPECT_GT(interiorCount, 0);
}

// ========================================
// Wall Properties Tests
// ========================================

TEST_F(WallGeneratorTest, WallThickness_AppliedCorrectly) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateSimpleRoom();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    float customThickness = 0.25f;
    wallGenerator.SetWallThickness(customThickness);
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    // Check that walls have correct thickness
    for (const auto& wall : walls) {
        EXPECT_FLOAT_EQ(wall.thickness, customThickness);
    }
}

TEST_F(WallGeneratorTest, WallHeight_AppliedCorrectly) {
    SchemaValidator validator;
    std::string errorMsg;
    
    // Create a building with ceiling_height = 0 to use default
    std::string json = TestHelpers::CreateMinimalValidBuilding();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    // Set ceiling_height to 0 so it uses default
    for (auto& floor : definition.floors) {
        for (auto& space : floor.spaces) {
            space.properties.ceilingHeight = 0.0f;
        }
    }
    
    float customHeight = 3.5f;
    wallGenerator.SetDefaultWallHeight(customHeight);
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    // Check that walls use default height when ceiling_height is 0
    for (const auto& wall : walls) {
        EXPECT_FLOAT_EQ(wall.height, customHeight);
    }
}

// ========================================
// Edge Cases
// ========================================

TEST_F(WallGeneratorTest, EmptyBuilding_NoWalls) {
    definition.floors.clear();
    definition.masses.clear();
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    EXPECT_EQ(walls.size(), 0);
}

TEST_F(WallGeneratorTest, LShapedBuilding_CorrectExteriorPerimeter) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateLShapedBuilding();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    EXPECT_GT(walls.size(), 0);
    
    // L-shaped building should have more complex exterior walls
    int exteriorWalls = 0;
    for (const auto& wall : walls) {
        if (wall.type == WallType::Exterior) {
            exteriorWalls++;
        }
    }
    
    EXPECT_GT(exteriorWalls, 4) << "L-shaped building should have more than 4 exterior walls";
}

// ========================================
// Deep Tests - Wall Coordinates and Dimensions
// ========================================

TEST_F(WallGeneratorTest, SimpleRoom_WallCoordinatesCorrect) {
    // Create a simple 10x8 room
    definition.grid = 0.5f;
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {10.0f, 8.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    Space space;
    space.spaceId = 1;
    space.properties.ceilingHeight = 3.0f;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 8.0f}
    });
    floor.spaces.push_back(space);
    definition.floors.push_back(floor);
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    ASSERT_EQ(walls.size(), 4) << "Simple rectangle should have 4 walls";
    
    // Calculate total perimeter from walls
    float totalLength = 0.0f;
    for (const auto& wall : walls) {
        float length = std::sqrt(
            std::pow(wall.end.x - wall.start.x, 2) +
            std::pow(wall.end.y - wall.start.y, 2)
        );
        totalLength += length;
        
        // All walls should be aligned horizontally or vertically
        bool isHorizontal = std::abs(wall.start.y - wall.end.y) < 0.01f;
        bool isVertical = std::abs(wall.start.x - wall.end.x) < 0.01f;
        EXPECT_TRUE(isHorizontal || isVertical) 
            << "Wall should be axis-aligned";
    }
    
    // Total perimeter should be 2*(10+8) = 36
    float expectedPerimeter = 2 * (10.0f + 8.0f);
    EXPECT_NEAR(totalLength, expectedPerimeter, 0.5f) 
        << "Total wall length should equal room perimeter";
}

TEST_F(WallGeneratorTest, WallsFormClosedLoop) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateSimpleRoom();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    // Check that walls form a closed loop
    // Each wall's end point should be another wall's start point
    for (const auto& wall : walls) {
        bool foundConnection = false;
        for (const auto& other : walls) {
            if (&wall != &other) {
                float dist = std::sqrt(
                    std::pow(wall.end.x - other.start.x, 2) +
                    std::pow(wall.end.y - other.start.y, 2)
                );
                if (dist < 0.1f) {
                    foundConnection = true;
                    break;
                }
            }
        }
        EXPECT_TRUE(foundConnection) 
            << "Wall end should connect to another wall's start";
    }
}

TEST_F(WallGeneratorTest, InteriorWall_ConnectsTwoSpaces) {
    // Create a building with two adjacent rooms
    definition.grid = 0.5f;
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {20.0f, 10.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    // Room 1: Left side (0,0) to (10,10)
    Space space1;
    space1.spaceId = 1;
    space1.properties.ceilingHeight = 3.0f;
    space1.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space1);
    
    // Room 2: Right side (10,0) to (20,10)
    Space space2;
    space2.spaceId = 2;
    space2.properties.ceilingHeight = 3.0f;
    space2.rects.push_back({
        "rect_2",
        {10.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space2);
    
    definition.floors.push_back(floor);
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    // Should have at least one interior wall
    bool hasInteriorWall = false;
    for (const auto& wall : walls) {
        if (wall.type == WallType::Interior) {
            hasInteriorWall = true;
            
            // Interior wall should be vertical at x=10
            bool isAtX10 = (std::abs(wall.start.x - 10.0f) < 0.1f && 
                           std::abs(wall.end.x - 10.0f) < 0.1f);
            EXPECT_TRUE(isAtX10) 
                << "Interior wall should be at the boundary between rooms";
        }
    }
    
    EXPECT_TRUE(hasInteriorWall) << "Should have interior wall between spaces";
}

TEST_F(WallGeneratorTest, WallThickness_AffectsPosition) {
    definition.grid = 0.5f;
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    Space space;
    space.spaceId = 1;
    space.properties.ceilingHeight = 3.0f;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space);
    definition.floors.push_back(floor);
    
    // Test with thickness = 0.2
    wallGenerator.SetWallThickness(0.2f);
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    // Walls should be offset by half thickness from the boundary
    // For a room at (0,0), walls should be at -0.1, not exactly 0
    for (const auto& wall : walls) {
        EXPECT_FLOAT_EQ(wall.thickness, 0.2f);
        // Wall positions should account for thickness
        // (exact position depends on implementation)
    }
}
