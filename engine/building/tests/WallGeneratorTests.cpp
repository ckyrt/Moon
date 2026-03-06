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
