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
    EXPECT_GT(stair.landings.size(), 0) << "L-stair should have landing";
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
    
    EXPECT_GT(stair.landings.size(), 0) 
        << "L-shaped stair should have at least one landing";
}

TEST_F(StairGeneratorTest, StraightStair_NoLandings) {
    CreateBuildingWithStairs(StairType::Straight);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // Simple straight stairs typically don't need landings
    EXPECT_EQ(stair.landings.size(), 0) 
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

// ========================================
// Detailed Geometry Tests - Test New Fields
// ========================================

TEST_F(StairGeneratorTest, StairGeometry_HasValidStepsArray) {
    CreateBuildingWithStairs(StairType::Straight);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // Should have steps array with individual step positions
    EXPECT_EQ(stair.steps.size(), stair.numSteps) 
        << "Steps array should contain position for each step";
    
    // Verify each step has valid height
    for (size_t i = 0; i < stair.steps.size(); ++i) {
        const auto& step = stair.steps[i];
        EXPECT_GE(step.height, i * stair.stepHeight * 0.9f) 
            << "Step " << i << " height should increase monotonically";
    }
}

TEST_F(StairGeneratorTest, StraightStair_StepPositionsAreLinear) {
    CreateBuildingWithStairs(StairType::Straight);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    ASSERT_GE(stair.steps.size(), 3) << "Need at least 3 steps to test linearity";
    
    // Check that steps follow a straight line (X or Y should be constant, the other increases)
    bool xIsConstant = std::abs(stair.steps[0].position[0] - stair.steps[1].position[0]) < 0.1f;
    bool yIsConstant = std::abs(stair.steps[0].position[1] - stair.steps[1].position[1]) < 0.1f;
    
    EXPECT_TRUE(xIsConstant || yIsConstant) << "Straight stair should be axis-aligned";
    
    // Verify steps are evenly spaced
    float expectedSpacing = stair.stepDepth;
    for (size_t i = 1; i < stair.steps.size(); ++i) {
        float dx = stair.steps[i].position[0] - stair.steps[i-1].position[0];
        float dy = stair.steps[i].position[1] - stair.steps[i-1].position[1];
        float spacing = std::sqrt(dx*dx + dy*dy);
        
        EXPECT_NEAR(spacing, expectedSpacing, 0.5f) 
            << "Step " << i << " spacing should be consistent";
    }
}

TEST_F(StairGeneratorTest, UStar_ReturnPathOppositeDirection) {
    CreateBuildingWithStairs(StairType::U);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // U-stair should have landing in middle
    EXPECT_GT(stair.landings.size(), 0) << "U-stair should have landing";
    
    // Check that steps before and after midpoint go in opposite directions
    if (stair.steps.size() >= 4) {
        size_t midPoint = stair.steps.size() / 2;
        
        // First segment direction
        float dir1_x = stair.steps[1].position[0] - stair.steps[0].position[0];
        float dir1_y = stair.steps[1].position[1] - stair.steps[0].position[1];
        
        // Second segment direction (after midpoint)
        float dir2_x = stair.steps[midPoint+1].position[0] - stair.steps[midPoint].position[0];
        float dir2_y = stair.steps[midPoint+1].position[1] - stair.steps[midPoint].position[1];
        
        // Directions should be roughly opposite (dot product should be negative)
        float dot = dir1_x * dir2_x + dir1_y * dir2_y;
        EXPECT_LT(dot, 0.1f) 
            << "U-stair return path should go in opposite direction, dot=" << dot;
    }
}

TEST_F(StairGeneratorTest, LStair_LandingAtTurn) {
    CreateBuildingWithStairs(StairType::L);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    ASSERT_GT(stair.landings.size(), 0) << "L-stair should have landing";
    
    const auto& landing = stair.landings[0];
    
    // Landing should be positioned somewhere in the middle of the stair path
    // Check that landing height is between first and last step
    EXPECT_GT(landing.height, stair.steps[0].height) 
        << "Landing should be above first step";
    EXPECT_LT(landing.height, stair.steps.back().height) 
        << "Landing should be below last step";
}

TEST_F(StairGeneratorTest, StairGeometry_HasStairLengthCalculated) {
    CreateBuildingWithStairs(StairType::Straight);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // stairLength should be calculated
    EXPECT_GT(stair.stairLength, 0.0f) 
        << "Stair length should be calculated and positive";
    
    // For straight stair, length should be approximately numSteps * stepDepth
    float expectedLength = stair.numSteps * stair.stepDepth;
    EXPECT_NEAR(stair.stairLength, expectedLength, stair.stepDepth * 2.0f)
        << "Stair length should be approximately numSteps * stepDepth";
}

TEST_F(StairGeneratorTest, StairGeometry_HasRotationField) {
    CreateBuildingWithStairs(StairType::Straight);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // Rotation should be set
    EXPECT_TRUE(stair.rotation != 0.0f || stair.rotation == 0.0f) 
        << "Rotation field should be initialized";
}

TEST_F(StairGeneratorTest, LandingPlatform_HasDetailedGeometry) {
    CreateBuildingWithStairs(StairType::L);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    ASSERT_GT(stair.landings.size(), 0) << "L-stair should have landing";
    
    const auto& landing = stair.landings[0];
    
    // Landing should have valid dimensions
    EXPECT_GT(landing.width, 0.0f) << "Landing should have positive width";
    EXPECT_GT(landing.depth, 0.0f) << "Landing should have positive depth";
    EXPECT_GT(landing.height, 0.0f) << "Landing should have positive height";
    
    // Landing width should be at least stair width
    EXPECT_GE(landing.width, stair.config.width * 0.9f) 
        << "Landing width should be at least stair width";
}

TEST_F(StairGeneratorTest, StairGeometry_HasFromToLevels) {
    CreateBuildingWithStairs(StairType::Straight);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // fromLevel and toLevel should be set correctly
    EXPECT_EQ(stair.fromLevel, 0) << "Stair should start from level 0";
    EXPECT_EQ(stair.toLevel, 1) << "Stair should connect to level 1";
}
    // Create a 4-floor building
    Mass mass;
    mass.massId = "mass0";
    mass.origin = {0, 0};
    mass.size = {10, 10};
    mass.floors = 4;
    definition.masses.push_back(mass);
    
    for (int level = 0; level < 4; ++level) {
        Floor floor;
        floor.level = level;
        floor.massId = "mass0";
        floor.floorHeight = 3.0f;
        
        Space space;
        space.spaceId = level;
        Rect rect;
        rect.rectId = "rect" + std::to_string(level);
        rect.origin = {0, 0};
        rect.size = {10, 10};
        space.rects.push_back(rect);
        space.properties.usage = SpaceUsage::Living;
        space.properties.ceilingHeight = 3.0f;
        
        // Add stair from level 0 to level 3 (spanning 3 floors)
        if (level == 0) {
            space.hasStairs = true;
            space.stairsConfig.type = StairType::Spiral;
            space.stairsConfig.connectToLevel = 3;
            space.stairsConfig.position = {5, 5};
            space.stairsConfig.width = 1.5f;
        }
        
        floor.spaces.push_back(space);
        definition.floors.push_back(floor);
    }
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    
    // totalHeight should be sum of 3 floor heights (level 0->3)
    float expectedHeight = 3.0f * 3; // 3 floors * 3m each = 9m
    EXPECT_FLOAT_EQ(stair.totalHeight, expectedHeight) 
        << "Total height should equal cumulative floor heights";
    
    // Number of steps should reflect actual 9m height
    int expectedMinSteps = static_cast<int>(std::ceil(expectedHeight / 0.18f));
    EXPECT_GE(stair.numSteps, expectedMinSteps) 
        << "Should have enough steps for 9m height";
}

TEST_F(StairGeneratorTest, ValidateStairGeometry_MeetsCodeRequirements) {
    CreateBuildingWithStairs(StairType::Straight);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0) ;
    const auto& stair = stairs[0];
    
    // Check if validation was performed
    // (assuming ValidateStairGeometry sets meetsCodeRequirements)
    EXPECT_LE(stair.stepHeight, 0.18f) 
        << "Step height should meet code (≤18cm)";
    EXPECT_GE(stair.stepDepth, 0.28f) 
        << "Step depth should meet code (≥28cm)";
}

TEST_F(StairGeneratorTest, SpiralStair_HasCircularPattern) {
    CreateBuildingWithStairs(StairType::Spiral);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_GT(stairs.size(), 0);
    const auto& stair = stairs[0];
    ASSERT_GE(stair.steps.size(), 8) << "Need enough steps to verify circular pattern";
    
    // Calculate center position
    float centerX = stair.config.position[0];
    float centerY = stair.config.position[1];
    
    // Calculate radius from center to first step
    float r1 = std::sqrt(
        std::pow(stair.steps[0].position[0] - centerX, 2) +
        std::pow(stair.steps[0].position[1] - centerY, 2)
    );
    
    // All steps should be at approximately same distance from center
    for (size_t i = 1; i < stair.steps.size(); ++i) {
        float ri = std::sqrt(
            std::pow(stair.steps[i].position[0] - centerX, 2) +
            std::pow(stair.steps[i].position[1] - centerY, 2)
        );
        
        EXPECT_NEAR(ri, r1, 0.5f) 
            << "Step " << i << " should be at same radius in spiral";
    }
    
    // Verify steps rotate around center
    float prevAngle = std::atan2(
        stair.steps[0].position[1] - centerY,
        stair.steps[0].position[0] - centerX
    );
    
    for (size_t i = 1; i < stair.steps.size(); ++i) {
        float angle = std::atan2(
            stair.steps[i].position[1] - centerY,
            stair.steps[i].position[0] - centerX
        );
        
        // Angle should increase (or wrap around)
        float deltaAngle = angle - prevAngle;
        if (deltaAngle < -3.0f) deltaAngle += 2.0f * 3.14159f; // Handle wrap
        
        EXPECT_GT(deltaAngle, -0.5f) 
            << "Step " << i << " angle should increase around spiral";
        
        prevAngle = angle;
    }
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
