#include <gtest/gtest.h>
#include "building/LayoutValidator.h"
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

/**
 * @brief Test fixture for LayoutValidator
 */
class LayoutValidatorTest : public ::testing::Test {
protected:
    LayoutValidator validator;
    BuildingDefinition definition;
    
    void SetUp() override {
        // Set up reasonable defaults
        validator.SetMinRoomSize(1.5f, 1.5f);
        validator.SetWallThickness(0.2f);
    }
};

// ========================================
// Positive Tests - Valid Layouts
// ========================================

TEST_F(LayoutValidatorTest, ValidateMinimalBuilding_Success) {
    // Parse a valid building
    SchemaValidator schemaValidator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateMinimalValidBuilding();
    ASSERT_TRUE(schemaValidator.ValidateAndParse(json, definition, errorMsg));
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_TRUE(result.valid) << "Validation should pass";
    if (!result.errors.empty()) {
        for (const auto& error : result.errors) {
            std::cout << "Error: " << error << std::endl;
        }
    }
}

TEST_F(LayoutValidatorTest, ValidateSimpleRoom_Success) {
    SchemaValidator schemaValidator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateSimpleRoom();
    ASSERT_TRUE(schemaValidator.ValidateAndParse(json, definition, errorMsg));
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_TRUE(result.valid);
}

TEST_F(LayoutValidatorTest, ValidateMultiFloorBuilding_Success) {
    SchemaValidator schemaValidator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateMultiFloorBuilding();
    ASSERT_TRUE(schemaValidator.ValidateAndParse(json, definition, errorMsg));
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_TRUE(result.valid);
}

// ========================================
// Negative Tests - Invalid Layouts
// ========================================

TEST_F(LayoutValidatorTest, OverlappingSpaces_Fails) {
    // Create a definition with overlapping spaces
    definition.grid = 0.5f;
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {20.0f, 20.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    // Space 1
    Space space1;
    space1.spaceId = 1;
    space1.rects.push_back({
        "rect_1",
        {1.0f, 1.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space1);
    
    // Space 2 - overlaps with space 1
    Space space2;
    space2.spaceId = 2;
    space2.rects.push_back({
        "rect_2",
        {5.0f, 5.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space2);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_FALSE(result.valid);
    EXPECT_GT(result.errors.size(), 0);
}

TEST_F(LayoutValidatorTest, SpaceOutsideMass_Fails) {
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
    
    // Space extends outside the mass boundary
    Space space;
    space.spaceId = 1;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {15.0f, 15.0f}  // Larger than mass
    });
    floor.spaces.push_back(space);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_FALSE(result.valid);
    EXPECT_GT(result.errors.size(), 0);
}

TEST_F(LayoutValidatorTest, TooSmallRoom_Fails) {
    definition.grid = 0.5f;
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.floorHeight = 3.0f;
    
    // Space too small (less than minimum 1.5m x 1.5m)
    Space space;
    space.spaceId = 1;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {1.0f, 1.0f}  // Too small
    });
    floor.spaces.push_back(space);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_FALSE(result.valid);
    EXPECT_GT(result.errors.size(), 0);
}

TEST_F(LayoutValidatorTest, InvalidMassReference_Fails) {
    definition.grid = 0.5f;
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_2";  // References non-existent mass
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_FALSE(result.valid);
    EXPECT_GT(result.errors.size(), 0);
}

// ========================================
// Configuration Tests
// ========================================

TEST_F(LayoutValidatorTest, CustomMinimumRoomSize) {
    validator.SetMinRoomSize(3.0f, 3.0f);
    
    definition.grid = 0.5f;
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.floorHeight = 3.0f;
    
    // Space is 2m x 2m (valid with default settings, but not with custom)
    Space space;
    space.spaceId = 1;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {2.0f, 2.0f}
    });
    floor.spaces.push_back(space);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_FALSE(result.valid) << "Should fail with custom minimum 3m x 3m";
}
