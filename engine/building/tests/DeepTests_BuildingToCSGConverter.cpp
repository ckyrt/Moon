// 深入测试 BuildingToCSGConverter - 验证 CSG 转换
#include <gtest/gtest.h>
#include "building/BuildingToCSGConverter.h"
#include "building/BuildingPipeline.h"
#include "building/SchemaValidator.h"
#include "TestHelpers.h"
#include "json.hpp"

using namespace Moon::Building;
using namespace Moon::Building::Test;
using json = nlohmann::json;

class BuildingToCSGConverterDeepTest : public ::testing::Test {
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
// 验证 CSG JSON 结构
// ========================================

TEST_F(BuildingToCSGConverterDeepTest, CSGOutput_HasRequiredFields) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    ASSERT_TRUE(IsValidJSON(csgJson)) << "输出必须是有效的 JSON";
    
    json csg = ParseJSON(csgJson);
    
    // 验证必需字段
    EXPECT_TRUE(csg.contains("schema_version")) 
        << "CSG JSON 必须包含 schema_version";
    EXPECT_TRUE(csg.contains("root")) 
        << "CSG JSON 必须包含 root 节点";
    
    EXPECT_TRUE(csg["root"].is_object()) 
        << "root 必须是对象";
}

TEST_F(BuildingToCSGConverterDeepTest, CSGRoot_HasCorrectStructure) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    ASSERT_TRUE(csg.contains("root"));
    
    const auto& root = csg["root"];
    
    // Root 应该有 operation 字段
    EXPECT_TRUE(root.contains("operation")) 
        << "Root 节点必须有 operation 字段";
    
    // Root 应该有 children 或 primitive
    bool hasChildren = root.contains("children") && root["children"].is_array();
    bool hasPrimitive = root.contains("primitive");
    
    EXPECT_TRUE(hasChildren || hasPrimitive) 
        << "Root 必须有 children 或 primitive";
}

// ========================================
// 验证墙体转换
// ========================================

TEST_F(BuildingToCSGConverterDeepTest, Walls_ConvertedToCSG) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    ASSERT_GT(building.walls.size(), 0) << "必须有墙";
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 墙应该在 CSG 树中的某个地方
    // 深度优先搜索查找墙相关的节点
    std::function<bool(const json&)> findWalls = [&](const json& node) -> bool {
        if (node.contains("type")) {
            std::string type = node["type"];
            if (type == "wall" || type == "box") {
                return true;
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (findWalls(child)) return true;
            }
        }
        
        return false;
    };
    
    bool foundWalls = findWalls(csg);
    EXPECT_TRUE(foundWalls) 
        << "CSG 输出必须包含墙体几何";
}

TEST_F(BuildingToCSGConverterDeepTest, Wall_HasCorrectDimensions) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    ASSERT_GT(building.walls.size(), 0);
    
    // 记录原始墙的尺寸
    const auto& firstWall = building.walls[0];
    float wallLength = std::sqrt(
        std::pow(firstWall.end[0] - firstWall.start[0], 2) +
        std::pow(firstWall.end[1] - firstWall.start[1], 2)
    );
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 查找包含尺寸信息的节点
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
    
    bool hasValidDimensions = checkDimensions(csg);
    EXPECT_TRUE(hasValidDimensions) 
        << "CSG 几何必须有合理的尺寸";
}

// ========================================
// 验证门窗转换
// ========================================

TEST_F(BuildingToCSGConverterDeepTest, Doors_ConvertedToCSG) {
    std::string inputJson = TestHelpers::CreateVilla();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    if (building.doors.size() == 0) {
        GTEST_SKIP() << "测试建筑没有门";
    }
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 门应该作为减法操作（subtract）或者有门类型标记
    std::function<bool(const json&)> findDoors = [&](const json& node) -> bool {
        if (node.contains("operation")) {
            std::string op = node["operation"];
            if (op == "subtract" || op == "difference") {
                // 可能是门的开口
                return true;
            }
        }
        
        if (node.contains("type")) {
            std::string type = node["type"];
            if (type == "door" || type == "opening") {
                return true;
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (findDoors(child)) return true;
            }
        }
        
        return false;
    };
    
    bool foundDoors = findDoors(csg);
    EXPECT_TRUE(foundDoors) 
        << "CSG 输出应该包含门的开口";
}

TEST_F(BuildingToCSGConverterDeepTest, Windows_ConvertedToCSG) {
    std::string inputJson = TestHelpers::CreateVilla();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    ASSERT_GT(building.windows.size(), 0) << "测试建筑必须有窗户";
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 窗户应该作为减法操作或有窗户类型标记
    std::function<int(const json&)> countWindows = [&](const json& node) -> int {
        int count = 0;
        
        if (node.contains("type")) {
            std::string type = node["type"];
            if (type == "window") {
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
    
    int windowCount = countWindows(csg);
    
    // 至少应该有一些窗户被转换
    EXPECT_GT(windowCount, 0) 
        << "CSG 输出必须包含窗户开口";
}

// ========================================
// 验证楼梯转换
// ========================================

TEST_F(BuildingToCSGConverterDeepTest, Stairs_ConvertedToCSG) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    if (building.stairs.size() == 0) {
        GTEST_SKIP() << "测试建筑没有楼梯";
    }
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 楼梯应该在 CSG 中表示
    std::function<bool(const json&)> findStairs = [&](const json& node) -> bool {
        if (node.contains("type")) {
            std::string type = node["type"];
            if (type == "stair" || type == "stairs" || type == "step") {
                return true;
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (findStairs(child)) return true;
            }
        }
        
        return false;
    };
    
    bool foundStairs = findStairs(csg);
    EXPECT_TRUE(foundStairs) 
        << "CSG 输出应该包含楼梯几何";
}

// ========================================
// 验证 CSG 操作
// ========================================

TEST_F(BuildingToCSGConverterDeepTest, CSG_UsesUnionOperation) {
    std::string inputJson = TestHelpers::CreateVilla();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 查找 union 操作（合并墙体）
    std::function<bool(const json&)> findUnion = [&](const json& node) -> bool {
        if (node.contains("operation")) {
            std::string op = node["operation"];
            if (op == "union") {
                return true;
            }
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (findUnion(child)) return true;
            }
        }
        
        return false;
    };
    
    bool hasUnion = findUnion(csg);
    EXPECT_TRUE(hasUnion) 
        << "CSG 应该使用 union 操作合并几何";
}

TEST_F(BuildingToCSGConverterDeepTest, CSG_UsesSubtractOperation) {
    std::string inputJson = TestHelpers::CreateVilla();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
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
        
        return false;
    };
    
    bool hasSubtract = findSubtract(csg);
    EXPECT_TRUE(hasSubtract) 
        << "CSG 应该使用 subtract 操作创建开口";
}

// ========================================
// 验证完整转换
// ========================================

TEST_F(BuildingToCSGConverterDeepTest, CompleteConversion_AllComponents) {
    std::string inputJson = TestHelpers::CreateVilla();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    EXPECT_FALSE(csgJson.empty()) << "CSG 输出不应该为空";
    EXPECT_TRUE(IsValidJSON(csgJson)) << "CSG 输出必须是有效 JSON";
    
    json csg = ParseJSON(csgJson);
    
    // 验证 JSON 大小合理（有实际内容）
    std::string csgStr = csg.dump();
    EXPECT_GT(csgStr.length(), 100) 
        << "CSG JSON 应该有实质内容";
}

TEST_F(BuildingToCSGConverterDeepTest, EmptyBuilding_ValidCSG) {
    GeneratedBuilding building;
    // 空建筑（没有墙/门/窗）
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    // 即使是空的，也应该生成有效的 CSG
    EXPECT_FALSE(csgJson.empty()) << "即使空建筑也应该生成 CSG";
    EXPECT_TRUE(IsValidJSON(csgJson)) << "空建筑的 CSG 也应该是有效 JSON";
}

TEST_F(BuildingToCSGConverterDeepTest, LargeBuilding_HandleCorrectly) {
    std::string inputJson = TestHelpers::CreateApartmentBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_TRUE(IsValidJSON(csgJson)) << "大型建筑的 CSG 必须有效";
    
    // 转换不应该太慢
    EXPECT_LT(duration.count(), 2000) 
        << "CSG 转换时间过长: " << duration.count() << "ms";
}

// ========================================
// 验证几何变换
// ========================================

TEST_F(BuildingToCSGConverterDeepTest, Geometry_HasTransforms) {
    std::string inputJson = TestHelpers::CreateSimpleRoom();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 几何应该有位置变换
    std::function<bool(const json&)> findTransforms = [&](const json& node) -> bool {
        if (node.contains("position") || node.contains("transform") || 
            node.contains("translation") || node.contains("rotation")) {
            return true;
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                if (findTransforms(child)) return true;
            }
        }
        
        return false;
    };
    
    bool hasTransforms = findTransforms(csg);
    EXPECT_TRUE(hasTransforms) 
        << "CSG 几何应该包含位置/变换信息";
}

TEST_F(BuildingToCSGConverterDeepTest, MultiFloor_CorrectHeightOffsets) {
    std::string inputJson = TestHelpers::CreateMultiFloorBuilding();
    GeneratedBuilding building;
    std::string errorMsg;
    
    ASSERT_TRUE(pipeline.ProcessBuilding(inputJson, building, errorMsg));
    ASSERT_GT(building.definition.floors.size(), 1) << "必须是多层建筑";
    
    std::string csgJson = BuildingToCSGConverter::Convert(building);
    json csg = ParseJSON(csgJson);
    
    // 多层建筑应该有不同高度的几何
    std::function<std::set<float>(const json&)> collectHeights = [&](const json& node) -> std::set<float> {
        std::set<float> heights;
        
        if (node.contains("position") && node["position"].is_array() && node["position"].size() >= 3) {
            float z = node["position"][2];
            heights.insert(z);
        }
        
        if (node.contains("children") && node["children"].is_array()) {
            for (const auto& child : node["children"]) {
                auto childHeights = collectHeights(child);
                heights.insert(childHeights.begin(), childHeights.end());
            }
        }
        
        return heights;
    };
    
    auto heights = collectHeights(csg);
    
    // 多层建筑应该有多个不同的高度值
    EXPECT_GT(heights.size(), 1) 
        << "多层建筑应该有不同高度的几何";
}
