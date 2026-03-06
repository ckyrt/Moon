#include <gtest/gtest.h>
#include "building/BuildingPipeline.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"
#include <chrono>

using namespace Moon::Building;
using namespace Moon::Building::Test;

/**
 * @brief Integration test fixture for BuildingPipeline
 */
class BuildingPipelineTest : public ::testing::Test {
protected:
    BuildingPipeline pipeline;
    GeneratedBuilding building;
    std::string errorMsg;
};

// ========================================
// Integration Tests - Full Pipeline
// ========================================

TEST_F(BuildingPipelineTest, ProcessMinimalBuilding_Success) {
    std::string json = TestHelpers::CreateMinimalValidBuilding();
    
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.masses.size(), 1);
    EXPECT_EQ(building.definition.floors.size(), 1);
}

TEST_F(BuildingPipelineTest, ProcessSimpleRoom_Success) {
    std::string json = TestHelpers::CreateSimpleRoom();
    
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_TRUE(result) << "Error: " << errorMsg;
    ASSERT_EQ(building.definition.floors.size(), 1);
    EXPECT_EQ(building.definition.floors[0].spaces.size(), 1);
}

TEST_F(BuildingPipelineTest, ProcessMultiFloorBuilding_Success) {
    std::string json = TestHelpers::CreateMultiFloorBuilding();
    
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 2);
    
    // TODO: When generators are fully implemented, add checks for:
    // - Generated walls
    // - Generated doors
    // - Generated stairs
}

TEST_F(BuildingPipelineTest, ProcessInvalidJSON_Fails) {
    std::string json = TestHelpers::CreateInvalidJSON_MissingGrid();
    
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(errorMsg.empty());
}

// ========================================
// Validation-Only Tests
// ========================================

TEST_F(BuildingPipelineTest, ValidateOnly_ValidBuilding_Success) {
    std::string json = TestHelpers::CreateSimpleRoom();
    ValidationResult result;
    
    bool success = pipeline.ValidateOnly(json, result);
    
    EXPECT_TRUE(success);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.errors.size(), 0);
}

TEST_F(BuildingPipelineTest, ValidateOnly_InvalidBuilding_Fails) {
    std::string json = TestHelpers::CreateInvalidJSON_WrongGridSize();
    ValidationResult result;
    
    bool success = pipeline.ValidateOnly(json, result);
    
    EXPECT_FALSE(success);
    EXPECT_FALSE(result.valid);
    EXPECT_GT(result.errors.size(), 0);
}

// ========================================
// Error Handling Tests
// ========================================

TEST_F(BuildingPipelineTest, EmptyJSON_Fails) {
    std::string json = "";
    
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(BuildingPipelineTest, MalformedJSON_Fails) {
    std::string json = "{ not valid json at all }";
    
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_NE(errorMsg.find("parse"), std::string::npos);
}

// ========================================
// Performance Tests (Optional)
// ========================================

TEST_F(BuildingPipelineTest, DISABLED_PerformanceTest_LargeBuilding) {
    // Create a large building with many rooms
    std::string json = R"({
      "schema": "moon_building_v8",
      "grid": 0.5,
      "style": {
        "category": "modern",
        "facade": "glass_white",
        "roof": "flat",
        "windowStyle": "standard",
        "material": "concrete_white"
      },
      "masses": [
        {
          "massId": "mass_1",
          "origin": [0.0, 0.0],
          "size": [100.0, 100.0],
          "floors": 10
        }
      ],
      "floors": []
    })";
    
    // Add floors programmatically would go here
    // For now, just test with basic structure
    
    auto start = std::chrono::high_resolution_clock::now();
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_TRUE(result);
    std::cout << "Processing time: " << duration.count() << "ms" << std::endl;
}
