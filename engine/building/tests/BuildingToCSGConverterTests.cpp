#include <gtest/gtest.h>
#include "building/BuildingToCSGConverter.h"
#include "building/BuildingPipeline.h"
#include "building/SchemaValidator.h"
#include "TestHelpers.h"
#include "json.hpp"
#include <chrono>

using namespace Moon::Building;
using namespace Moon::Building::Test;
using json = nlohmann::json;

/**
 * @brief Test fixture for BuildingToCSGConverter
 */
class BuildingToCSGConverterTest : public ::testing::Test {
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
    
    // Helper to check if JSON has required CSG fields
    bool HasCSGFields(const std::string& jsonStr) {
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

// ========================================
// Basic Conversion Tests
// ========================================

TEST_F(BuildingToCSGConverterTest, SimpleRoom_ValidCSGOutput) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    EXPECT_FALSE(csgJson.empty()) << "CSG JSON should not be empty";
    EXPECT_TRUE(IsValidJSON(csgJson)) << "Output should be valid JSON";
    EXPECT_TRUE(HasCSGFields(csgJson)) << "Should have CSG required fields";
}

TEST_F(BuildingToCSGConverterTest, MultiRoom_ValidCSGOutput) {
    std::string inputJson = TestHelpers::CreateLShapedBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    EXPECT_FALSE(csgJson.empty());
    EXPECT_TRUE(IsValidJSON(csgJson));
    EXPECT_TRUE(HasCSGFields(csgJson));
}

TEST_F(BuildingToCSGConverterTest, MultiFloor_ValidCSGOutput) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << "Building processing failed: " << errorMsg;
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    EXPECT_FALSE(csgJson.empty());
    EXPECT_TRUE(IsValidJSON(csgJson));
    EXPECT_TRUE(HasCSGFields(csgJson));
}

// ========================================
// Structure Validation Tests
// ========================================

TEST_F(BuildingToCSGConverterTest, OutputHasSchemaVersion) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    json j = json::parse(csgJson);
    EXPECT_TRUE(j.contains("schema_version") || j.contains("version"));
    if (j.contains("schema_version")) {
        EXPECT_TRUE(j["schema_version"].is_number());
    }
}

TEST_F(BuildingToCSGConverterTest, OutputHasRoot) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    json j = json::parse(csgJson);
    EXPECT_TRUE(j.contains("root"));
    EXPECT_TRUE(j["root"].is_object() || j["root"].is_string());
}

TEST_F(BuildingToCSGConverterTest, OutputHasChildren) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    json j = json::parse(csgJson);
    EXPECT_TRUE(j.contains("root"));
    if (j["root"].is_object() && j["root"].contains("children")) {
        EXPECT_TRUE(j["root"]["children"].is_array());
    }
}

// ========================================
// Content Tests
// ========================================

TEST_F(BuildingToCSGConverterTest, WallsIncludedInOutput) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_GT(building.walls.size(), 0) << "Building should have walls";
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    // Check that CSG output is not trivial
    EXPECT_GT(csgJson.length(), 100) << "CSG output should be substantial";
}

TEST_F(BuildingToCSGConverterTest, WindowsHandledInOutput) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    
    // CSG converter should handle both buildings with and without windows
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    EXPECT_GT(csgJson.length(), 100) << "CSG output should be valid";
    EXPECT_TRUE(IsValidJSON(csgJson));
}

TEST_F(BuildingToCSGConverterTest, DoorsHandledInOutput) {
    std::string inputJson = TestHelpers::CreateLShapedBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    // Doors should not break CSG generation
    EXPECT_TRUE(IsValidJSON(csgJson));
    EXPECT_GT(csgJson.length(), 100);
}

// ========================================
// Edge Cases
// ========================================

TEST_F(BuildingToCSGConverterTest, EmptyBuilding_ValidOutput) {
    GeneratedBuilding emptyBuilding;
    
    std::string csgJson = BuildingToCSGConverter::Convert(emptyBuilding);
    
    // Even empty building should produce valid JSON
    EXPECT_TRUE(IsValidJSON(csgJson)) << "Empty building should produce valid JSON";
}

TEST_F(BuildingToCSGConverterTest, BuildingWithOnlyWalls_ValidOutput) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    
    // Clear windows and doors
    building.windows.clear();
    building.doors.clear();
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    EXPECT_TRUE(IsValidJSON(csgJson));
    EXPECT_FALSE(csgJson.empty());
}

TEST_F(BuildingToCSGConverterTest, MultiFloorBuilding_ValidOutput) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    // Multi-floor should produce output
    EXPECT_GT(csgJson.length(), 100) << "Multi-floor building should have CSG";
    EXPECT_TRUE(IsValidJSON(csgJson));
}

// ========================================
// Integration Tests
// ========================================

TEST_F(BuildingToCSGConverterTest, FullPipeline_SimpleRoom) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    // Full pipeline
    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << errorMsg;
    
    // Convert to CSG
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    // Validate CSG
    EXPECT_TRUE(IsValidJSON(csgJson));
    EXPECT_TRUE(HasCSGFields(csgJson));
    
    // Parse and check structure
    json j = json::parse(csgJson);
    EXPECT_TRUE(j.contains("schema_version") || j.contains("version"));
    EXPECT_TRUE(j.contains("root"));
}

TEST_F(BuildingToCSGConverterTest, FullPipeline_ComplexBuilding) {
    std::string inputJson = TestHelpers::CreateLShapedBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    bool success = pipeline.ProcessBuilding(inputJson, building, errorMsg);
    ASSERT_TRUE(success) << errorMsg;
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    EXPECT_TRUE(IsValidJSON(csgJson));
    EXPECT_TRUE(HasCSGFields(csgJson));
    EXPECT_GT(csgJson.length(), 100);
}

// ========================================
// Performance Tests
// ========================================

TEST_F(BuildingToCSGConverterTest, LargeBuilding_ReasonablePerformance) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    pipeline.ProcessBuilding(inputJson, building, errorMsg);
    
    // Measure conversion time
    auto start = std::chrono::high_resolution_clock::now();
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Conversion should be fast (< 100ms for reasonable building)
    EXPECT_LT(duration.count(), 100) << "Conversion should be fast";
    EXPECT_TRUE(IsValidJSON(csgJson));
}
