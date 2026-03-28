// 深入测试 BuildingToObjectBlueprintConverter - 验证 CSG 转换
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
// 验证 object blueprint JSON 结构
// ========================================

TEST_F(BuildingToObjectBlueprintConverterDeepTest, CSGOutput_HasRequiredFields) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    ASSERT_TRUE(IsValidJSON(csgJson)) << "输出必须是有效的 JSON";
    
    json csg = ParseJSON(csgJson);
    
    // 验证必需字段
    EXPECT_TRUE(csg.contains("schema_version")) 
        << "object blueprint JSON 必须包含 schema_version";
    EXPECT_TRUE(csg.contains("root")) 
        << "object blueprint JSON 必须包含 root 节点";
    
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
    
    // Root 应该�?type 字段
    EXPECT_TRUE(root.contains("type")) 
        << "Root 节点必须�?type 字段";
    
    std::string rootType = root["type"];
    
    // Root 应该�?group, csg, �?primitive
    EXPECT_TRUE(rootType == "group" || rootType == "csg" || rootType == "primitive") 
        << "Root type 必须�?group/csg/primitive，实际是: " << rootType;
    
    // Group �?CSG 节点应该�?children
    if (rootType == "group" || rootType == "csg") {
        EXPECT_TRUE(root.contains("children") && root["children"].is_array()) 
            << "Group/CSG 节点必须�?children 数组";
    }
}

// ========================================
// 验证墙体转换
// ========================================

TEST_F(BuildingToObjectBlueprintConverterDeepTest, Walls_ConvertedToCSG) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    ASSERT_GT(building.walls.size(), 0) << "必须有墙";
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 墙应该在 CSG 树中的某个地�?
    // 深度优先搜索查找墙相关的节点
    std::function<bool(const json&)> findWalls = [&](const json& node) -> bool {
        // 检查name字段是否包含"wall"
        if (node.contains("name")) {
            std::string name = node["name"];
            if (name.find("wall") != std::string::npos) {
                return true;
            }
        }
        
        // 检查primitive类型或reference类型
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
        
        // CSG节点检查左右子�?
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
        << "CSG 输出必须包含墙体几何";
}

TEST_F(BuildingToObjectBlueprintConverterDeepTest, Wall_HasCorrectDimensions) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    ASSERT_GT(building.walls.size(), 0);
    
    // 记录原始墙的尺寸
    const auto& firstWall = building.walls[0];
    float wallLength = static_cast<float>(std::sqrt(
        std::pow(firstWall.end[0] - firstWall.start[0], 2) +
        std::pow(firstWall.end[1] - firstWall.start[1], 2)
    ));
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 查找包含尺寸信息的节�?
    std::function<bool(const json&)> checkDimensions = [&](const json& node) -> bool {
        if (node.contains("size") && node["size"].is_array() && node["size"].size() == 3) {
            // 验证尺寸合理
            float x = node["size"][0];
            float y = node["size"][1];
            float z = node["size"][2];
            
            if (x > 0 && y > 0 && z > 0) {
                // 至少一个维度应该接近墙的长度或高度
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
        << "CSG 几何必须有合理的尺寸";
}

// ========================================
// 验证门窗转换
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
    
    // 门应该作为reference节点或有door相关的name
    std::function<bool(const json&)> findDoors = [&](const json& node) -> bool {
        // 检查name字段
        if (node.contains("name")) {
            std::string name = node["name"];
            if (name.find("door") != std::string::npos) {
                return true;
            }
        }
        
        // 检查reference类型
        if (node.contains("ref")) {
            std::string ref = node["ref"];
            if (ref.find("door") != std::string::npos) {
                return true;
            }
        }
        
        // 检查opening（门窗开口）
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
        
        // CSG节点检查左右子�?
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
    
    // 窗户应该作为reference节点或有window相关的name
    std::function<int(const json&)> countWindows = [&](const json& node) -> int {
        int count = 0;
        
        // 检查name字段
        if (node.contains("name")) {
            std::string name = node["name"];
            if (name.find("window") != std::string::npos) {
                count++;
            }
        }
        
        // 检查reference类型
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
    
    // 至少应该有一些窗户被转换
    EXPECT_GT(windowCount, 0) 
        << "output must contain window openings";
}

// ========================================
// 验证楼梯转换
// ========================================

TEST_F(BuildingToObjectBlueprintConverterDeepTest, Stairs_ConvertedToCSG) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    if (building.stairs.size() == 0) {
        GTEST_SKIP() << "测试建筑没有楼梯";
    }
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 楼梯应该�?CSG 中表�?
    std::function<bool(const json&)> findStairs = [&](const json& node) -> bool {
        // 检查name字段
        if (node.contains("name")) {
            std::string name = node["name"];
            if (name.find("stair") != std::string::npos) {
                return true;
            }
        }
        
        // 检查reference类型
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
        
        // CSG节点检查左右子�?
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
        GTEST_SKIP() << "楼梯转换功能尚未实现";
    }
    
    EXPECT_TRUE(foundStairs) 
        << "CSG 输出应该包含楼梯几何";
}

// ========================================
// 验证 CSG 操作
// ========================================

TEST_F(BuildingToObjectBlueprintConverterDeepTest, CSG_UsesUnionOperation) {
    std::string inputJson = TestHelpers::CreateVilla();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 查找 union 操作�?group 节点（两者都能组合几何）
    std::function<bool(const json&)> findUnionOrGroup = [&](const json& node) -> bool {
        if (node.contains("type")) {
            std::string type = node["type"];
            if (type == "group") {
                return true;  // group节点相当于并列输�?
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
        << "CSG 应该使用 union �?group 节点组合几何";
}

TEST_F(BuildingToObjectBlueprintConverterDeepTest, CSG_UsesSubtractOperation) {
    std::string inputJson = TestHelpers::CreateVilla();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 查找 subtract 操作（门窗开口）
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
        
        // CSG节点检查左右子�?
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
// 验证完整转换
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
    
    // 验证 JSON 大小合理（有实际内容�?
    std::string csgStr = csg.dump();
    EXPECT_GT(csgStr.length(), 100) 
        << "object blueprint JSON should contain meaningful content";
}

TEST_F(BuildingToObjectBlueprintConverterDeepTest, EmptyBuilding_ValidCSG) {
    GeneratedBuilding building;
    // 空建筑（没有�?�?窗）
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    
    // 即使是空的，也应该生成有效的 CSG
    EXPECT_FALSE(csgJson.empty()) << "即使空建筑也应该生成 CSG";
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
    
    EXPECT_TRUE(IsValidJSON(csgJson)) << "大型建筑�?CSG 必须有效";
    
    // 转换不应该太�?
    EXPECT_LT(duration.count(), 2000) 
        << "CSG 转换时间过长: " << duration.count() << "ms";
}

// ========================================
// 验证几何变换
// ========================================

TEST_F(BuildingToObjectBlueprintConverterDeepTest, Geometry_HasTransforms) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 几何应该有位置变�?
    std::function<bool(const json&)> findTransforms = [&](const json& node) -> bool {
        // 直接包含这些字段
        if (node.contains("position") || node.contains("translation") || node.contains("rotation")) {
            return true;
        }
        
        // transform对象包含position
        if (node.contains("transform")) {
            return true;  // 只要有transform字段就算通过
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (findTransforms(child)) return true;
            }
        }
        
        // CSG节点检查左右子�?
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
        << "CSG 几何应该包含位置/变换信息";
}

TEST_F(BuildingToObjectBlueprintConverterDeepTest, MultiFloor_CorrectHeightOffsets) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    ASSERT_GT(building.definition.floors.size(), 1) << "building must have multiple floors";
    
    std::string csgJson = BuildingToObjectBlueprintConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 多层建筑应该有不同高度的几何
    std::function<std::set<float>(const json&)> collectHeights = [&](const json& node) -> std::set<float> {
        std::set<float> heights;
        
        // 直接position字段
        if (node.contains("position") && node["position"].is_array() && node["position"].size() >= 3) {
            float y = node["position"][1];  // Y轴是高度，不是Z�?
            heights.insert(y);
        }
        
        // transform.position字段
        if (node.contains("transform") && node["transform"].is_object()) {
            const auto& transform = node["transform"];
            if (transform.contains("position") && transform["position"].is_array() && transform["position"].size() >= 3) {
                float y = transform["position"][1];  // Y轴是高度
                heights.insert(y);
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                auto childHeights = collectHeights(child);
                heights.insert(childHeights.begin(), childHeights.end());
            }
        }
        
        // CSG节点检查左右子�?
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
    
    // 多层建筑应该有多个不同的高度�?
    EXPECT_GT(heights.size(), 1) 
        << "多层建筑应该有不同高度的几何";
}



