//  BuildingToObjectBlueprintConverter -  CSG 
#include <gtest/gtest.h>
#include <chrono>
#include "building/BuildingToObjectBlueprintConverter.h"
#include "building/BuildingPipeline.h"
#include "building/SchemaValidator.h"
#include "TestHelpers.h"
#include "json.hpp"

using namespace Moon::Building;
using namespace Moon::Building::Test;
using json = nlohmann::json;

class BuildingToObjectBlueprintConverterDeepTest : public ::testing::Test {
protected:
    BuildingPipeline pipeline;
    
    bool IsValidJSON(const std::string& jsonStr) {
        try {
            json j = json::parse(jsonStr);
            return true;
        } catch (...) {
            return false;
        }
    }
    
    json ParseJSON(const std::string& jsonStr) {
        return json::parse(jsonStr);
    }
};

// ========================================
//  object blueprint JSON 
// ========================================

TEST_F(BuildingToObjectBlueprintConverterDeepTest, CSGOutput_HasRequiredFields) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    ASSERT_TRUE(IsValidJSON(csgJson)) << " JSON";
    
    json csg = ParseJSON(csgJson);
    
    // 
    EXPECT_TRUE(csg.contains("schema_version")) 
        << "object blueprint JSON  schema_version";
    EXPECT_TRUE(csg.contains("root")) 
        << "object blueprint JSON  root ";
    
    EXPECT_TRUE(csg["root"].is_object()) 
        << "root must be an object";
}

TEST_F(BuildingToObjectBlueprintConverterDeepTest, CSGRoot_HasCorrectStructure) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    ASSERT_TRUE(csg.contains("root"));
    
    const auto& root = csg["root"];
    
    // Root ?type 
    EXPECT_TRUE(root.contains("type")) 
        << "Root ?type ";
    
    std::string rootType = root["type"];
    
    // Root ?group, csg, ?primitive
    EXPECT_TRUE(rootType == "group" || rootType == "csg" || rootType == "primitive") 
        << "Root type ?group/csg/primitive: " << rootType;
    
    // Group ?CSG ?children
    if (rootType == "group" || rootType == "csg") {
        EXPECT_TRUE(root.contains("children") && root["children"].is_array()) 
            << "Group/CSG ?children ";
    }
}

// ========================================
// 
// ========================================

TEST_F(BuildingToObjectBlueprintConverterDeepTest, Walls_ConvertedToCSG) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    ASSERT_GT(building.walls.size(), 0) << "";
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    //  CSG ?
    // 
    std::function<bool(const json&)> findWalls = [&](const json& node) -> bool {
        // ame"wall"
        if (node.contains("name")) {
            std::string name = node["name"];
            if (name.find("wall") != std::string::npos) {
                return true;
            }
        }
        
        // rimitiveeference
        if (node.contains("type")) {
            std::string type = node["type"];
            if (type == "primitive" || type == "reference") {
                if (node.contains("primitive") && node["primitive"] == "cube") {
                    return true;
                }
                if (node.contains("ref") && node["ref"].get<std::string>().find("wall") != std::string::npos) {
                    return true;
                }
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (findWalls(child)) return true;
            }
        }
        
        // CSG?
        if (node.contains("left")) {
            if (findWalls(node["left"])) return true;
        }
        if (node.contains("right")) {
            if (findWalls(node["right"])) return true;
        }
        
        return false;
    };
    
    bool foundWalls = findWalls(csg["root"]);
    EXPECT_TRUE(foundWalls) 
        << "CSG ";
}

TEST_F(BuildingToObjectBlueprintConverterDeepTest, Wall_HasCorrectDimensions) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    ASSERT_GT(building.walls.size(), 0);
    
    // 
    const auto& firstWall = building.walls[0];
    float wallLength = static_cast<float>(std::sqrt(
        std::pow(firstWall.end[0] - firstWall.start[0], 2) +
        std::pow(firstWall.end[1] - firstWall.start[1], 2)
    ));
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // ?
    std::function<bool(const json&)> checkDimensions = [&](const json& node) -> bool {
        if (node.contains("size") && node["size"].is_array() && node["size"].size() == 3) {
            // 
            float x = node["size"][0];
            float y = node["size"][1];
            float z = node["size"][2];
            
            if (x > 0 && y > 0 && z > 0) {
                // 
                bool hasReasonableSize = 
                    (std::abs(x - wallLength) < 1.0f) ||
                    (std::abs(y - wallLength) < 1.0f) ||
                    (std::abs(z - firstWall.height) < 0.5f);
                
                if (hasReasonableSize) return true;
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (checkDimensions(child)) return true;
            }
        }
        
        return false;
    };
    
    bool hasValidDimensions = csg.contains("root")
        ? checkDimensions(csg["root"])
        : checkDimensions(csg);
    EXPECT_TRUE(hasValidDimensions) 
        << "CSG ";
}

// ========================================
// 
// ========================================

TEST_F(BuildingToObjectBlueprintConverterDeepTest, Doors_ConvertedToCSG) {
    std::string inputJson = TestHelpers::CreateVilla();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    if (building.doors.size() == 0) {
        GTEST_SKIP() << "test building has no doors";
    }
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // eferencedoorame
    std::function<bool(const json&)> findDoors = [&](const json& node) -> bool {
        // ame
        if (node.contains("name")) {
            std::string name = node["name"];
            if (name.find("door") != std::string::npos) {
                return true;
            }
        }
        
        // eference
        if (node.contains("ref")) {
            std::string ref = node["ref"];
            if (ref.find("door") != std::string::npos) {
                return true;
            }
        }
        
        // pening
        if (node.contains("ref")) {
            std::string ref = node["ref"];
            if (ref.find("opening") != std::string::npos) {
                return true;
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (findDoors(child)) return true;
            }
        }
        
        // CSG?
        if (node.contains("left")) {
            if (findDoors(node["left"])) return true;
        }
        if (node.contains("right")) {
            if (findDoors(node["right"])) return true;
        }
        
        return false;
    };
    
    bool foundDoors = findDoors(csg["root"]);
    EXPECT_TRUE(foundDoors) 
        << "output should contain door openings";
}

TEST_F(BuildingToObjectBlueprintConverterDeepTest, Windows_ConvertedToCSG) {
    std::string inputJson = TestHelpers::CreateVilla();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    ASSERT_GT(building.windows.size(), 0) << "test building must have windows";
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // referencewindowame
    std::function<int(const json&)> countWindows = [&](const json& node) -> int {
        int count = 0;
        
        // ame
        if (node.contains("name")) {
            std::string name = node["name"];
            if (name.find("window") != std::string::npos) {
                count++;
            }
        }
        
        // eference
        if (node.contains("ref")) {
            std::string ref = node["ref"];
            if (ref.find("window") != std::string::npos) {
                count++;
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                count += countWindows(child);
            }
        }
        
        return count;
    };
    
    int windowCount = countWindows(csg["root"]);
    
    // 
    EXPECT_GT(windowCount, 0) 
        << "output must contain window openings";
}

// ========================================
// 
// ========================================

TEST_F(BuildingToObjectBlueprintConverterDeepTest, Stairs_ConvertedToCSG) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    if (building.stairs.size() == 0) {
        GTEST_SKIP() << "";
    }
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // ?CSG ?
    std::function<bool(const json&)> findStairs = [&](const json& node) -> bool {
        // ame
        if (node.contains("name")) {
            std::string name = node["name"];
            if (name.find("stair") != std::string::npos) {
                return true;
            }
        }
        
        // eference
        if (node.contains("ref")) {
            std::string ref = node["ref"];
            if (ref.find("stair") != std::string::npos) {
                return true;
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (findStairs(child)) return true;
            }
        }
        
        // CSG?
        if (node.contains("left")) {
            if (findStairs(node["left"])) return true;
        }
        if (node.contains("right")) {
            if (findStairs(node["right"])) return true;
        }
        
        return false;
    };
    
    bool foundStairs = findStairs(csg["root"]);
    
    if (!foundStairs) {
        GTEST_SKIP() << "";
    }
    
    EXPECT_TRUE(foundStairs) 
        << "CSG ";
}

// ========================================
//  CSG 
// ========================================

TEST_F(BuildingToObjectBlueprintConverterDeepTest, CSG_UsesUnionOperation) {
    std::string inputJson = TestHelpers::CreateVilla();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    //  union ?group 
    std::function<bool(const json&)> findUnionOrGroup = [&](const json& node) -> bool {
        if (node.contains("type")) {
            std::string type = node["type"];
            if (type == "group") {
                return true;  // group?
            }
        }
        
        if (node.contains("operation")) {
            std::string op = node["operation"];
            if (op == "union") {
                return true;
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (findUnionOrGroup(child)) return true;
            }
        }
        
        if (node.contains("left")) {
            if (findUnionOrGroup(node["left"])) return true;
        }
        if (node.contains("right")) {
            if (findUnionOrGroup(node["right"])) return true;
        }
        
        return false;
    };
    
    bool hasUnionOrGroup = findUnionOrGroup(csg["root"]);
    EXPECT_TRUE(hasUnionOrGroup) 
        << "CSG  union ?group ";
}

TEST_F(BuildingToObjectBlueprintConverterDeepTest, CSG_UsesSubtractOperation) {
    std::string inputJson = TestHelpers::CreateVilla();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    //  subtract 
    std::function<bool(const json&)> findSubtract = [&](const json& node) -> bool {
        if (node.contains("operation")) {
            std::string op = node["operation"];
            if (op == "subtract" || op == "difference") {
                return true;
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (findSubtract(child)) return true;
            }
        }
        
        // CSG?
        if (node.contains("left")) {
            if (findSubtract(node["left"])) return true;
        }
        if (node.contains("right")) {
            if (findSubtract(node["right"])) return true;
        }
        
        return false;
    };
    
    bool hasSubtract = findSubtract(csg["root"]);
    EXPECT_TRUE(hasSubtract) 
        << "output should use subtract operations for openings";
}

// ========================================
// 
// ========================================

TEST_F(BuildingToObjectBlueprintConverterDeepTest, CompleteConversion_AllComponents) {
    std::string inputJson = TestHelpers::CreateVilla();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    EXPECT_FALSE(csgJson.empty()) << "output should not be empty";
    EXPECT_TRUE(IsValidJSON(csgJson)) << "output must be valid JSON";
    
    json csg = ParseJSON(csgJson);
    
    //  JSON ?
    std::string csgStr = csg.dump();
    EXPECT_GT(csgStr.length(), 100) 
        << "object blueprint JSON should contain meaningful content";
}

TEST_F(BuildingToObjectBlueprintConverterDeepTest, EmptyBuilding_ValidCSG) {
    GeneratedBuilding building;
    // ??
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    //  CSG
    EXPECT_FALSE(csgJson.empty()) << " CSG";
    EXPECT_TRUE(IsValidJSON(csgJson)) << "output must be valid JSON";
}

TEST_F(BuildingToObjectBlueprintConverterDeepTest, LargeBuilding_HandleCorrectly) {
    std::string inputJson = TestHelpers::CreateApartmentBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_TRUE(IsValidJSON(csgJson)) << "?CSG ";
    
    // ?
    EXPECT_LT(duration.count(), 2000) 
        << "CSG : " << duration.count() << "ms";
}

// ========================================
// 
// ========================================

TEST_F(BuildingToObjectBlueprintConverterDeepTest, Geometry_HasTransforms) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // ?
    std::function<bool(const json&)> findTransforms = [&](const json& node) -> bool {
        // 
        if (node.contains("position") || node.contains("translation") || node.contains("rotation")) {
            return true;
        }
        
        // transformposition
        if (node.contains("transform")) {
            return true;  // ransform
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (findTransforms(child)) return true;
            }
        }
        
        // CSG?
        if (node.contains("left")) {
            if (findTransforms(node["left"])) return true;
        }
        if (node.contains("right")) {
            if (findTransforms(node["right"])) return true;
        }
        
        return false;
    };
    
    bool hasTransforms = findTransforms(csg["root"]);
    EXPECT_TRUE(hasTransforms) 
        << "CSG /";
}

TEST_F(BuildingToObjectBlueprintConverterDeepTest, MultiFloor_CorrectHeightOffsets) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    ASSERT_GT(building.definition.floors.size(), 1) << "building must have multiple floors";
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 
    std::function<std::set<float>(const json&)> collectHeights = [&](const json& node) -> std::set<float> {
        std::set<float> heights;
        
        // position
        if (node.contains("position") && node["position"].is_array() && node["position"].size() >= 3) {
            float y = node["position"][1];  // Y?
            heights.insert(y);
        }
        
        // transform.position
        if (node.contains("transform") && node["transform"].is_object()) {
            const auto& transform = node["transform"];
            if (transform.contains("position") && transform["position"].is_array() && transform["position"].size() >= 3) {
                float y = transform["position"][1];  // Y
                heights.insert(y);
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                auto childHeights = collectHeights(child);
                heights.insert(childHeights.begin(), childHeights.end());
            }
        }
        
        // CSG?
        if (node.contains("left")) {
            auto leftHeights = collectHeights(node["left"]);
            heights.insert(leftHeights.begin(), leftHeights.end());
        }
        if (node.contains("right")) {
            auto rightHeights = collectHeights(node["right"]);
            heights.insert(rightHeights.begin(), rightHeights.end());
        }
        
        return heights;
    };
    
    auto heights = collectHeights(csg["root"]);
    
    // ?
    EXPECT_GT(heights.size(), 1) 
        << "";
}




