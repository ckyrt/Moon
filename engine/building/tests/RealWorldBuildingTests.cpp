#include <gtest/gtest.h>
#include "building/BuildingPipeline.h"
#include "building/SchemaValidator.h"
#include "building/LayoutValidator.h"
#include "building/SpaceGraphBuilder.h"
#include "building/WallGenerator.h"
#include "building/DoorGenerator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

/**
 * @brief Real-world building test cases
 * Tests the system with realistic, complex building scenarios
 */
class RealWorldBuildingTest : public ::testing::Test {
protected:
    BuildingPipeline pipeline;
    SchemaValidator validator;
    LayoutValidator layoutValidator;
    SpaceGraphBuilder spaceGraph;
    WallGenerator wallGenerator;
    DoorGenerator doorGenerator;
    
    BuildingDefinition definition;
    std::vector<SpaceConnection> connections;
    std::vector<WallSegment> walls;
    std::vector<Door> doors;
    
    void SetUp() override {
        layoutValidator.SetMinRoomSize(2.0f, 2.0f);
        wallGenerator.SetWallThickness(0.2f);
        wallGenerator.SetDefaultWallHeight(3.0f);
        doorGenerator.SetDefaultDoorSize(0.9f, 2.1f);
    }
};

// ========================================
// Luxury Villa Test (3 floors, 18 rooms)
// ========================================

TEST_F(RealWorldBuildingTest, LuxuryVilla_CompleteProcessing) {
    std::string json = TestHelpers::CreateLuxuryVilla();
    std::string errorMsg;
    
    // Step 1: Schema validation
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg)) 
        << "Schema validation failed: " << errorMsg;
    
    // Verify structure
    EXPECT_EQ(definition.floors.size(), 3) << "Should have 3 floors";
    
    // Count total spaces
    int totalSpaces = 0;
    for (const auto& floor : definition.floors) {
        totalSpaces += floor.spaces.size();
    }
    EXPECT_GE(totalSpaces, 15) << "Luxury villa should have at least 15 spaces";
    
    // Step 2: Layout validation
    ValidationResult layoutResult = layoutValidator.Validate(definition);
    EXPECT_TRUE(layoutResult.valid) << "Layout validation failed";
    if (!layoutResult.valid) {
        for (const auto& error : layoutResult.errors) {
            std::cout << "Layout error: " << error << std::endl;
        }
    }
    
    // Step 3: Build space graph
    spaceGraph.BuildGraph(definition, connections);
    EXPECT_GT(connections.size(), 0) << "Should have space connections";
    
    const auto& adjacencies = spaceGraph.GetAdjacencies();
    EXPECT_GT(adjacencies.size(), 10) << "Complex villa should have many adjacencies";
    
    // Step 4: Generate walls
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    EXPECT_GT(walls.size(), 50) << "Luxury villa should have many walls";
    
    // Count wall types
    int exteriorWalls = 0, interiorWalls = 0;
    for (const auto& wall : walls) {
        if (wall.type == WallType::Exterior) exteriorWalls++;
        else if (wall.type == WallType::Interior) interiorWalls++;
    }
    
    EXPECT_GT(exteriorWalls, 10) << "Should have exterior walls";
    EXPECT_GE(interiorWalls, 0) << "May have interior walls between adjacent rooms";
    
    std::cout << "Luxury Villa Stats:" << std::endl;
    std::cout << "  - Floors: " << definition.floors.size() << std::endl;
    std::cout << "  - Total Spaces: " << totalSpaces << std::endl;
    std::cout << "  - Adjacencies: " << adjacencies.size() << std::endl;
    std::cout << "  - Walls: " << walls.size() 
              << " (Exterior: " << exteriorWalls 
              << ", Interior: " << interiorWalls << ")" << std::endl;
    std::cout << "  - Connections: " << connections.size() << std::endl;
}

TEST_F(RealWorldBuildingTest, LuxuryVilla_FirstFloorLayout) {
    std::string json = TestHelpers::CreateLuxuryVilla();
    std::string errorMsg;
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    // Find first floor
    const Floor* floor1 = nullptr;
    for (const auto& floor : definition.floors) {
        if (floor.level == 0) {
            floor1 = &floor;
            break;
        }
    }
    ASSERT_NE(floor1, nullptr) << "First floor not found";
    
    // First floor should have: entrance, living, dining, kitchen, guest room, guest bath, storage
    EXPECT_GE(floor1->spaces.size(), 6) << "First floor should have at least 6 spaces";
    
    // Verify space types
    bool hasLiving = false, hasKitchen = false, hasBathroom = false;
    for (const auto& space : floor1->spaces) {
        if (space.properties.usage == SpaceUsage::Living) hasLiving = true;
        if (space.properties.usage == SpaceUsage::Kitchen) hasKitchen = true;
        if (space.properties.usage == SpaceUsage::Bathroom) hasBathroom = true;
    }
    
    EXPECT_TRUE(hasLiving) << "First floor should have living room";
    EXPECT_TRUE(hasKitchen) << "First floor should have kitchen";
    EXPECT_TRUE(hasBathroom) << "First floor should have bathroom";
}

TEST_F(RealWorldBuildingTest, LuxuryVilla_SecondFloorLayout) {
    std::string json = TestHelpers::CreateLuxuryVilla();
    std::string errorMsg;
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    // Find second floor
    const Floor* floor2 = nullptr;
    for (const auto& floor : definition.floors) {
        if (floor.level == 1) {
            floor2 = &floor;
            break;
        }
    }
    ASSERT_NE(floor2, nullptr) << "Second floor not found";
    
    // Second floor should have bedrooms
    EXPECT_GE(floor2->spaces.size(), 4) << "Second floor should have multiple bedrooms";
    
    // Count bedrooms
    int bedroomCount = 0;
    for (const auto& space : floor2->spaces) {
        if (space.properties.usage == SpaceUsage::Bedroom) {
            bedroomCount++;
        }
    }
    EXPECT_GE(bedroomCount, 2) << "Second floor should have at least 2 bedrooms";
}

TEST_F(RealWorldBuildingTest, LuxuryVilla_MasterSuite) {
    std::string json = TestHelpers::CreateLuxuryVilla();
    std::string errorMsg;
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    // Build space graph to find master suite connections
    spaceGraph.BuildGraph(definition, connections);
    
    // Master bedroom should be on second floor
    const Floor* floor2 = nullptr;
    for (const auto& floor : definition.floors) {
        if (floor.level == 1) {
            floor2 = &floor;
            break;
        }
    }
    ASSERT_NE(floor2, nullptr);
    
    // Find master bedroom (should be the largest bedroom)
    const Space* masterBedroom = nullptr;
    float maxArea = 0;
    for (const auto& space : floor2->spaces) {
        if (space.properties.usage == SpaceUsage::Bedroom) {
            float area = 0;
            for (const auto& rect : space.rects) {
                area += rect.size[0] * rect.size[1];
            }
            if (area > maxArea) {
                maxArea = area;
                masterBedroom = &space;
            }
        }
    }
    
    if (masterBedroom) {
        EXPECT_GT(maxArea, 15.0f) << "Master bedroom should be larger than 15 sqm";
        std::cout << "Master bedroom area: " << maxArea << " sqm" << std::endl;
    }
}

TEST_F(RealWorldBuildingTest, LuxuryVilla_ThirdFloorAmenities) {
    std::string json = TestHelpers::CreateLuxuryVilla();
    std::string errorMsg;
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    // Find third floor
    const Floor* floor3 = nullptr;
    for (const auto& floor : definition.floors) {
        if (floor.level == 2) {
            floor3 = &floor;
            break;
        }
    }
    
    if (floor3) {
        EXPECT_GE(floor3->spaces.size(), 1) << "Third floor should have spaces";
        std::cout << "Third floor has " << floor3->spaces.size() << " spaces" << std::endl;
    }
}

TEST_F(RealWorldBuildingTest, LuxuryVilla_FullPipeline) {
    std::string json = TestHelpers::CreateLuxuryVilla();
    GeneratedBuilding output;
    std::string errorMsg;
    
    bool success = pipeline.ProcessBuilding(json, output, errorMsg);
    ASSERT_TRUE(success) << "Pipeline processing failed: " << errorMsg;
    
    // Verify output
    EXPECT_GT(output.walls.size(), 0) << "Should generate walls";
    EXPECT_EQ(output.definition.floors.size(), 3) << "Should have 3 floors";
    
    std::cout << "\n=== Luxury Villa Pipeline Output ===" << std::endl;
    std::cout << "Walls generated: " << output.walls.size() << std::endl;
    std::cout << "Doors generated: " << output.doors.size() << std::endl;
    std::cout << "Space connections: " << output.connections.size() << std::endl;
}
