#include <gtest/gtest.h>
#include "building/StairGenerator.h"
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

/**
 * @brief Test fixture for StairGenerator
 */
class StairGeneratorTest : public ::testing::Test {
protected:
    StairGenerator stairGenerator;
    BuildingDefinition definition;
    std::vector<StairGeometry> stairs;
    
    void SetUp() override {
        stairGenerator.SetStepParameters(0.18f, 0.28f);
    }
    
    // Helper to create a simple multi-floor building with stairs
    void CreateBuildingWithStairs(StairType type) {
        Mass mass;
        mass.massId = "mass0";
        mass.origin = {0, 0};
        mass.size = {10, 10};
        mass.floors = 2;
        definition.masses.push_back(mass);
        
        // Ground floor
        Floor floor0;
        floor0.level = 0;
        floor0.massId = "mass0";
        floor0.floorHeight = 3.0f;
        
        Space space0;
        space0.spaceId = 0;
        Rect rect0;
        rect0.rectId = "rect0";
        rect0.origin = {0, 0};
        rect0.size = {10, 10};
        space0.rects.push_back(rect0);
        space0.properties.usage = SpaceUsage::Living;
        space0.properties.isOutdoor = false;
        space0.properties.hasStairs = true;
        space0.properties.ceilingHeight = 3.0f;
        space0.hasStairs = true;
        space0.stairsConfig.type = type;
        space0.stairsConfig.connectToLevel = 1;
        space0.stairsConfig.position = {5, 5};
        space0.stairsConfig.width = 1.2f;
        
        floor0.spaces.push_back(space0);
        definition.floors.push_back(floor0);
        
        // First floor
        Floor floor1;
        floor1.level = 1;
        floor1.massId = "mass0";
        floor1.floorHeight = 3.0f;
        
        Space space1;
        space1.spaceId = 1;
        Rect rect1;
        rect1.rectId = "rect1";
        rect1.origin = {0, 0};
        rect1.size = {10, 10};
        space1.rects.push_back(rect1);
        space1.properties.usage = SpaceUsage::Bedroom;
        space1.properties.isOutdoor = false;
        space1.properties.hasStairs = false;
        space1.properties.ceilingHeight = 3.0f;
        
        floor1.spaces.push_back(space1);
        definition.floors.push_back(floor1);
    }
};

// ========================================
// Basic Stair Generation Tests
// ========================================

TEST_F(StairGeneratorTest, StraightStair_Generated) {
    CreateBuildingWithStairs(StairType::Straight);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    EXPECT_EQ(stairs.size(), 1) << "Should generate one straight stair";
    
    const auto& stair = stairs[0];
    EXPECT_EQ(stair.config.type, StairType::Straight);
    EXPECT_GT(stair.numSteps, 0) << "Should have steps";
    EXPECT_GT(stair.stepHeight, 0.0f) << "Step height should be positive";
    EXPECT_GT(stair.stepDepth, 0.0f) << "Step depth should be positive";
}

TEST_F(StairGeneratorTest, LStair_Generated) {
    CreateBuildingWithStairs(StairType::L);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    EXPECT_EQ(stairs.size(), 1) << "Should generate one L-shaped stair";
    
    const auto& stair = stairs[0];
    EXPECT_EQ(stair.config.type, StairType::L);
    EXPECT_GT(stair.numSteps, 0);
    EXPECT_GT(stair.landingPositions.size(), 0) << "L-stair should have landing";
}

TEST_F(StairGeneratorTest, UStair_Generated) {
    CreateBuildingWithStairs(StairType::U);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    EXPECT_EQ(stairs.size(), 1) << "Should generate one U-shaped stair";
    
    const auto& stair = stairs[0];
    EXPECT_EQ(stair.config.type, StairType::U);
    EXPECT_GT(stair.numSteps, 0);
}

TEST_F(StairGeneratorTest, SpiralStair_Generated) {
    CreateBuildingWithStairs(StairType::Spiral);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    EXPECT_EQ(stairs.size(), 1) << "Should generate one spiral stair";
    
    const auto& stair = stairs[0];
    EXPECT_EQ(stair.config.type, StairType::Spiral);
    EXPECT_GT(stair.numSteps, 0);
}

// ========================================
// Step Calculation Tests
// ========================================

TEST_F(StairGeneratorTest, StepHeight_WithinBuildingCode) {
    CreateBuildingWithStairs(StairType::Straight);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // Building code typically limits step height to 18-20cm
    EXPECT_LE(stair.stepHeight, 0.20f) << "Step height should meet building code";
    EXPECT_GE(stair.stepHeight, 0.10f) << "Step height should be reasonable";
}

TEST_F(StairGeneratorTest, StepDepth_WithinBuildingCode) {
    CreateBuildingWithStairs(StairType::Straight);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // Building code typically requires step depth >= 28cm
    EXPECT_GE(stair.stepDepth, 0.28f) << "Step depth should meet building code";
}

TEST_F(StairGeneratorTest, NumSteps_CorrectForFloorHeight) {
    CreateBuildingWithStairs(StairType::Straight);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // With 3m floor height and max step height 0.18m, need at least 17 steps
    float calculatedHeight = stair.numSteps * stair.stepHeight;
    EXPECT_FLOAT_EQ(calculatedHeight, stair.totalHeight) 
        << "Total step height should equal floor height";
}

// ========================================
// Custom Parameters Tests
// ========================================

TEST_F(StairGeneratorTest, CustomStepParameters_Applied) {
    float customMaxHeight = 0.17f;
    float customMinDepth = 0.30f;
    stairGenerator.SetStepParameters(customMaxHeight, customMinDepth);
    
    CreateBuildingWithStairs(StairType::Straight);
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    EXPECT_LE(stair.stepHeight, customMaxHeight) << "Should respect max step height";
    EXPECT_GE(stair.stepDepth, customMinDepth) << "Should respect min step depth";
}

// ========================================
// Landing Tests
// ========================================

TEST_F(StairGeneratorTest, LStair_HasLanding) {
    CreateBuildingWithStairs(StairType::L);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    EXPECT_GT(stair.landingPositions.size(), 0) 
        << "L-shaped stair should have at least one landing";
}

TEST_F(StairGeneratorTest, StraightStair_NoLandings) {
    CreateBuildingWithStairs(StairType::Straight);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // Simple straight stairs typically don't need landings
    EXPECT_EQ(stair.landingPositions.size(), 0) 
        << "Simple straight stair should not have landings";
}

// ========================================
// Edge Cases
// ========================================

TEST_F(StairGeneratorTest, NoStairs_NothingGenerated) {
    CreateBuildingWithStairs(StairType::Straight);
    
    // Remove stair flag
    for (auto& floor : definition.floors) {
        for (auto& space : floor.spaces) {
            space.hasStairs = false;
        }
    }
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    EXPECT_EQ(stairs.size(), 0) << "No stairs should be generated";
}

TEST_F(StairGeneratorTest, EmptyBuilding_NoStairs) {
    definition.floors.clear();
    definition.masses.clear();
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    EXPECT_EQ(stairs.size(), 0);
}

TEST_F(StairGeneratorTest, MultipleStaircases) {
    CreateBuildingWithStairs(StairType::Straight);
    
    // Add another space with stairs
    Space space2;
    space2.spaceId = 2;
    Rect rect2;
    rect2.rectId = "rect2";
    rect2.origin = {5, 5};
    rect2.size = {5, 5};
    space2.rects.push_back(rect2);
    space2.properties.usage = SpaceUsage::Corridor;
    space2.properties.isOutdoor = false;
    space2.properties.hasStairs = true;
    space2.properties.ceilingHeight = 3.0f;
    space2.hasStairs = true;
    space2.stairsConfig.type = StairType::L;
    space2.stairsConfig.connectToLevel = 1;
    space2.stairsConfig.position = {7, 7};
    space2.stairsConfig.width = 1.0f;
    
    definition.floors[0].spaces.push_back(space2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    EXPECT_EQ(stairs.size(), 2) << "Should generate two staircases";
}

TEST_F(StairGeneratorTest, TallFloor_ManySteps) {
    CreateBuildingWithStairs(StairType::Straight);
    
    // Make floor very tall
    definition.floors[0].floorHeight = 6.0f;
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // 6m floor needs many steps
    EXPECT_GT(stair.numSteps, 30) << "Tall floor should need many steps";
}

TEST_F(StairGeneratorTest, ShortFloor_FewerSteps) {
    CreateBuildingWithStairs(StairType::Straight);
    
    // Make floor shorter
    definition.floors[0].floorHeight = 2.5f;
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // 2.5m floor needs fewer steps
    EXPECT_LT(stair.numSteps, 20) << "Short floor should need fewer steps";
    EXPECT_GT(stair.numSteps, 10) << "But still needs reasonable number";
}
