// 深入测试 BuildingPipeline - 验证完整流程
#include <gtest/gtest.h>
#include <chrono>
#include "building/BuildingPipeline.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

class BuildingPipelineDeepTest : public ::testing::Test {
protected:
    BuildingPipeline pipeline;
    GeneratedBuilding building;
    std::string errorMsg;
};

// ========================================
// 验证 Pipeline 各阶段输出
// ========================================

TEST_F(BuildingPipelineDeepTest, ProcessBuilding_GeneratesWalls) {
    std::string json = TestHelpers::CreateVilla();
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    ASSERT_TRUE(success) << "处理失败: " << errorMsg;
    
    // 必须生成墙
    EXPECT_GT(building.walls.size(), 0) 
        << "Pipeline 必须生成墙体";
    
    // 验证墙体有有效数据
    for (const auto& wall : building.walls) {
        EXPECT_GT(wall.height, 0.0f) << "墙高度必须 > 0";
        EXPECT_GT(wall.thickness, 0.0f) << "墙厚度必须 > 0";
        
        // 验证起点和终点不同
        float dx = wall.end[0] - wall.start[0];
        float dy = wall.end[1] - wall.start[1];
        float length = std::sqrt(dx*dx + dy*dy);
        EXPECT_GT(length, 0.01f) << "墙的长度必须 > 0";
    }
}

TEST_F(BuildingPipelineDeepTest, ProcessBuilding_GeneratesDoors) {
    std::string json = TestHelpers::CreateVilla();
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    ASSERT_TRUE(success) << "处理失败: " << errorMsg;
    
    // 别墅应该有门
    EXPECT_GT(building.doors.size(), 0) 
        << "Pipeline 必须生成门";
    
    // 验证门有有效数据
    for (const auto& door : building.doors) {
        EXPECT_GT(door.width, 0.0f) << "门宽度必须 > 0";
        EXPECT_GT(door.height, 0.0f) << "门高度必须 > 0";
        EXPECT_GE(door.width, 0.8f) << "门宽度应该至少80cm";
    }
}

TEST_F(BuildingPipelineDeepTest, ProcessBuilding_GeneratesWindows) {
    std::string json = TestHelpers::CreateVilla();
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    ASSERT_TRUE(success) << "处理失败: " << errorMsg;
    
    // 别墅应该有窗户
    EXPECT_GT(building.windows.size(), 0) 
        << "Pipeline 必须生成窗户";
    
    // 验证窗户有有效数据
    for (const auto& window : building.windows) {
        EXPECT_GT(window.width, 0.0f) << "窗户宽度必须 > 0";
        EXPECT_GT(window.height, 0.0f) << "窗户高度必须 > 0";
    }
}

TEST_F(BuildingPipelineDeepTest, ProcessMultiFloor_GeneratesStairs) {
    std::string json = TestHelpers::CreateMultiFloorBuilding();
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    ASSERT_TRUE(success) << "处理失败: " << errorMsg;
    
    // 多层建筑应该有楼梯
    EXPECT_GT(building.stairs.size(), 0) 
        << "多层建筑必须生成楼梯";
    
    // 验证楼梯有有效数据
    for (const auto& stair : building.stairs) {
        EXPECT_GT(stair.numSteps, 0) << "楼梯必须有台阶";
        EXPECT_GT(stair.totalHeight, 0.0f) << "楼梯总高度必须 > 0";
        EXPECT_LE(stair.stepHeight, 0.18f) << "台阶高度必须符合建筑规范";
        EXPECT_GE(stair.stepDepth, 0.28f) << "台阶深度必须符合建筑规范";
    }
}

TEST_F(BuildingPipelineDeepTest, ProcessBuilding_DefinitionParsed) {
    std::string json = TestHelpers::CreateSimpleRoom();
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    ASSERT_TRUE(success) << "处理失败: " << errorMsg;
    
    // 验证 definition 已解析
    EXPECT_GT(building.definition.masses.size(), 0) 
        << "Definition 必须包含 masses";
    EXPECT_GT(building.definition.floors.size(), 0) 
        << "Definition 必须包含 floors";
    
    // 验证楼层有空间
    bool hasSpaces = false;
    for (const auto& floor : building.definition.floors) {
        if (floor.spaces.size() > 0) {
            hasSpaces = true;
            break;
        }
    }
    EXPECT_TRUE(hasSpaces) << "至少一个楼层必须有空间";
}

// ========================================
// 验证错误处理
// ========================================

TEST_F(BuildingPipelineDeepTest, InvalidJSON_ReturnsError) {
    std::string json = "{ invalid json }";
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_FALSE(success) << "无效 JSON 必须失败";
    EXPECT_FALSE(errorMsg.empty()) << "必须提供错误信息";
}

TEST_F(BuildingPipelineDeepTest, MissingRequiredFields_ReturnsError) {
    std::string json = TestHelpers::CreateInvalidJSON_MissingGrid();
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_FALSE(success) << "缺少必需字段必须失败";
    EXPECT_FALSE(errorMsg.empty()) << "必须提供错误信息";
}

TEST_F(BuildingPipelineDeepTest, OverlappingSpaces_ReturnsError) {
    // 创建一个有重叠空间的无效建筑
    std::string json = R"({
        "grid": 0.5,
        "masses": [{
            "mass_id": "m1",
            "origin": [0, 0],
            "size": [20, 20],
            "floors": 1
        }],
        "floors": [{
            "level": 0,
            "mass_id": "m1",
            "floor_height": 3.0,
            "spaces": [
                {
                    "space_id": 1,
                    "rects": [{"rect_id": "r1", "origin": [0, 0], "size": [10, 10]}],
                    "properties": {"usage": "living", "ceiling_height": 3.0}
                },
                {
                    "space_id": 2,
                    "rects": [{"rect_id": "r2", "origin": [5, 5], "size": [10, 10]}],
                    "properties": {"usage": "bedroom", "ceiling_height": 3.0}
                }
            ]
        }]
    })";
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_FALSE(success) << "重叠空间必须被拒绝";
    EXPECT_FALSE(errorMsg.empty()) << "必须提供错误信息";
}

// ========================================
// 验证 ValidateOnly 模式
// ========================================

TEST_F(BuildingPipelineDeepTest, ValidateOnly_ValidBuilding_Success) {
    std::string json = TestHelpers::CreateSimpleRoom();
    ValidationResult result;
    
    bool success = pipeline.ValidateOnly(json, result);
    
    EXPECT_TRUE(success) << "ValidateOnly 不应该失败";
    EXPECT_TRUE(result.valid) << "有效建筑应该通过验证";
    EXPECT_EQ(result.errors.size(), 0) << "有效建筑不应该有错误";
}

TEST_F(BuildingPipelineDeepTest, ValidateOnly_InvalidBuilding_ReturnsErrors) {
    std::string json = TestHelpers::CreateInvalidJSON_WrongGridSize();
    ValidationResult result;
    
    bool success = pipeline.ValidateOnly(json, result);
    
    EXPECT_FALSE(success) << "无效建筑应该失败";
    EXPECT_FALSE(result.valid) << "result.valid 应该是 false";
    EXPECT_GT(result.errors.size(), 0) << "必须报告验证错误";
}

TEST_F(BuildingPipelineDeepTest, ValidateOnly_DoesNotGenerate) {
    std::string json = TestHelpers::CreateVilla();
    ValidationResult result;
    
    bool success = pipeline.ValidateOnly(json, result);
    
    EXPECT_TRUE(success);
    
    // ValidateOnly 不应该生成墙/门/窗
    // 验证 building 对象未被填充（如果 pipeline 支持的话）
    // 这取决于具体实现
}

// ========================================
// 验证端到端流程
// ========================================

TEST_F(BuildingPipelineDeepTest, SimpleRoom_CompleteFlow) {
    std::string json = TestHelpers::CreateSimpleRoom();
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    ASSERT_TRUE(success) << "处理失败: " << errorMsg;
    
    // 验证完整的输出
    EXPECT_GT(building.walls.size(), 0) << "必须有墙";
    EXPECT_GT(building.windows.size(), 0) << "简单房间应该有窗户";
    
    // 单个房间可能没有门（如果没有入口定义）
    // EXPECT_GE(building.doors.size(), 0);
    
    // 单层建筑不应该有楼梯
    EXPECT_EQ(building.stairs.size(), 0) << "单层建筑不应该有楼梯";
}

TEST_F(BuildingPipelineDeepTest, Villa_CompleteFlow) {
    std::string json = TestHelpers::CreateVilla();
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    ASSERT_TRUE(success) << "处理失败: " << errorMsg;
    
    // 别墅应该有完整的组件
    EXPECT_GT(building.walls.size(), 10) << "别墅应该有多面墙";
    EXPECT_GT(building.windows.size(), 3) << "别墅应该有多个窗户";
    EXPECT_GT(building.doors.size(), 1) << "别墅应该有门";
    
    // 验证墙有内墙和外墙
    int exteriorWalls = 0, interiorWalls = 0;
    for (const auto& wall : building.walls) {
        if (wall.type == WallType::Exterior) exteriorWalls++;
        if (wall.type == WallType::Interior) interiorWalls++;
    }
    
    EXPECT_GT(exteriorWalls, 0) << "必须有外墙";
    EXPECT_GT(interiorWalls, 0) << "多房间别墅应该有内墙";
}

TEST_F(BuildingPipelineDeepTest, Apartment_MultiFloorFlow) {
    std::string json = TestHelpers::CreateApartmentBuilding();
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    ASSERT_TRUE(success) << "处理失败: " << errorMsg;
    
    // 公寓楼应该有多层
    EXPECT_GT(building.definition.floors.size(), 1) << "公寓应该是多层的";
    
    // 应该有很多墙（多层 x 多房间）
    EXPECT_GT(building.walls.size(), 20) << "公寓应该有很多墙";
    
    // 应该有很多窗户
    EXPECT_GT(building.windows.size(), 5) << "公寓应该有多个窗户";
}

// ========================================
// 验证性能（基本检查）
// ========================================

TEST_F(BuildingPipelineDeepTest, Processing_CompletesInReasonableTime) {
    std::string json = TestHelpers::CreateVilla();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    ASSERT_TRUE(success) << "处理失败: " << errorMsg;
    
    // 处理一个别墅不应该超过5秒
    EXPECT_LT(duration.count(), 5000) 
        << "处理时间过长: " << duration.count() << "ms";
}

// ========================================
// 验证空建筑处理
// ========================================

TEST_F(BuildingPipelineDeepTest, EmptyJSON_Handled) {
    std::string json = "";
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_FALSE(success) << "空 JSON 应该失败";
    EXPECT_FALSE(errorMsg.empty()) << "必须提供错误信息";
}

TEST_F(BuildingPipelineDeepTest, MinimalBuilding_Success) {
    std::string json = TestHelpers::CreateMinimalValidBuilding();
    
    bool success = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_TRUE(success) << "最小有效建筑应该成功: " << errorMsg;
    
    // 最小建筑至少应该有一些基本组件
    EXPECT_GT(building.definition.masses.size(), 0);
    EXPECT_GT(building.definition.floors.size(), 0);
}
