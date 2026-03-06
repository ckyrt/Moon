#include <gtest/gtest.h>
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

/**
 * @brief Test fixture for SchemaValidator
 */
class SchemaValidatorTest : public ::testing::Test {
protected:
    SchemaValidator validator;
    BuildingDefinition definition;
    std::string errorMsg;
};

// ========================================
// Positive Tests - Valid Inputs
// ========================================

TEST_F(SchemaValidatorTest, ValidateMinimalBuilding_Success) {
    std::string json = TestHelpers::CreateMinimalValidBuilding();
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(definition.grid, 0.5f);
    EXPECT_EQ(definition.style.category, "modern");
    EXPECT_EQ(definition.masses.size(), 1);
    EXPECT_EQ(definition.floors.size(), 1);
}

TEST_F(SchemaValidatorTest, ValidateSimpleRoom_Success) {
    std::string json = TestHelpers::CreateSimpleRoom();
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_TRUE(result) << "Error: " << errorMsg;
    ASSERT_EQ(definition.floors.size(), 1);
    ASSERT_EQ(definition.floors[0].spaces.size(), 1);
    
    const Space& space = definition.floors[0].spaces[0];
    EXPECT_EQ(space.properties.usageHint, "living");
    EXPECT_FLOAT_EQ(space.properties.ceilingHeight, 3.0f);
}

TEST_F(SchemaValidatorTest, ValidateMultiFloorBuilding_Success) {
    std::string json = TestHelpers::CreateMultiFloorBuilding();
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(definition.floors.size(), 2);
    
    // Check floor levels
    EXPECT_EQ(definition.floors[0].level, 0);
    EXPECT_EQ(definition.floors[1].level, 1);
    
    // Check stairs connection
    ASSERT_EQ(definition.floors[0].spaces.size(), 1);
    const Space& groundFloor = definition.floors[0].spaces[0];
    EXPECT_TRUE(groundFloor.hasStairs);
    EXPECT_EQ(groundFloor.stairsConfig.connectToLevel, 1);
}

// ========================================
// Negative Tests - Invalid Inputs
// ========================================

TEST_F(SchemaValidatorTest, InvalidJSON_MissingGrid_Fails) {
    std::string json = TestHelpers::CreateInvalidJSON_MissingGrid();
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(SchemaValidatorTest, InvalidJSON_WrongGridSize_Fails) {
    std::string json = TestHelpers::CreateInvalidJSON_WrongGridSize();
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(errorMsg.empty());
    EXPECT_NE(errorMsg.find("grid"), std::string::npos) << "Error should mention 'grid'";
}

TEST_F(SchemaValidatorTest, InvalidJSON_NonAlignedCoordinates_Fails) {
    std::string json = TestHelpers::CreateInvalidJSON_NonAlignedCoordinates();
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(SchemaValidatorTest, InvalidJSON_MalformedJSON_Fails) {
    std::string json = "{ this is not valid json }";
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_NE(errorMsg.find("parse"), std::string::npos);
}

TEST_F(SchemaValidatorTest, InvalidJSON_EmptyString_Fails) {
    std::string json = "";
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_FALSE(result);
}

// ========================================
// Edge Cases
// ========================================

TEST_F(SchemaValidatorTest, GridAlignment_ValidValues) {
    // Test various valid grid-aligned values
    std::vector<float> validValues = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 5.5f};
    
    for (float value : validValues) {
        std::string json = R"({
          "schema": "moon_building_v8",
          "grid": 0.5,
          "style": {"category": "modern", "facade": "glass", "roof": "flat", "window_style": "standard", "material": "concrete"},
          "masses": [
            {
              "mass_id": "m1",
              "origin": [)" + std::to_string(value) + R"(, )" + std::to_string(value) + R"(],
              "size": [10.0, 10.0],
              "floors": 1
            }
          ],
          "floors": [{
            "level": 0,
            "mass_id": "m1",
            "floor_height": 3.0,
            "spaces": []
          }]
        })";
        
        bool result = validator.ValidateAndParse(json, definition, errorMsg);
        EXPECT_TRUE(result) << "Value " << value << " should be valid. Error: " << errorMsg;
    }
}

TEST_F(SchemaValidatorTest, GridAlignment_InvalidValues) {
    // Test various invalid (non-aligned) values
    std::vector<float> invalidValues = {0.1f, 0.3f, 0.7f, 1.2f, 2.3f};
    
    for (float value : invalidValues) {
        std::string json = R"({
          "schema": "moon_building_v8",
          "grid": 0.5,
          "style": {"category": "modern", "facade": "glass", "roof": "flat", "window_style": "standard", "material": "concrete"},
          "masses": [
            {
              "mass_id": "m1",
              "origin": [)" + std::to_string(value) + R"(, 0.0],
              "size": [10.0, 10.0],
              "floors": 1
            }
          ],
          "floors": [{
            "level": 0,
            "mass_id": "m1",
            "floor_height": 3.0,
            "spaces": []
          }]
        })";
        
        bool result = validator.ValidateAndParse(json, definition, errorMsg);
        EXPECT_FALSE(result) << "Value " << value << " should be invalid";
    }
}
