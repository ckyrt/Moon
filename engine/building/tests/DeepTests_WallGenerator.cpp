// 深入测试 WallGenerator - 验证墙体生成的正确性
#include <gtest/gtest.h>
#include "building/WallGenerator.h"
#include "building/SpaceGraphBuilder.h"
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

class WallGeneratorDeepTest : public ::testing::Test {
protected:
    WallGenerator wallGenerator;
    SpaceGraphBuilder spaceGraph;
    BuildingDefinition definition;
    std::vector<WallSegment> walls;
    std::vector<SpaceConnection> connections;
    
    void SetUp() override {
        wallGenerator.SetWallThickness(0.2f);
        wallGenerator.SetDefaultWallHeight(3.0f);
    }
};

// ========================================
// 验证墙体坐标是否正确
// ========================================

TEST_F(WallGeneratorDeepTest, SimpleRoom_WallCoordinatesMatchBoundary) {
    // 创建一个精确的 10x8 房间
    definition.grid = 0.5f;
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {10.0f, 8.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    Space space;
    space.spaceId = 1;
    space.properties.ceilingHeight = 3.0f;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 8.0f}
    });
    floor.spaces.push_back(space);
    definition.floors.push_back(floor);
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    ASSERT_EQ(walls.size(), 4) << "矩形房间应该有4面墙";
    
    // 验证墙的总长度 = 周长
    float totalLength = 0.0f;
    for (const auto& wall : walls) {
        float dx = wall.end[0] - wall.start[0];
        float dy = wall.end[1] - wall.start[1];
        float length = std::sqrt(dx*dx + dy*dy);
        totalLength += length;
        
        EXPECT_GT(length, 0.0f) << "墙的长度必须大于0";
        
        // 验证墙是水平或垂直的
        bool isHorizontal = std::abs(dy) < 0.01f;
        bool isVertical = std::abs(dx) < 0.01f;
        EXPECT_TRUE(isHorizontal || isVertical) << "墙必须是轴对齐的";
    }
    
    // 总周长应该是 2*(10+8) = 36米
    float expectedPerimeter = 2 * (10.0f + 8.0f);
    EXPECT_NEAR(totalLength, expectedPerimeter, 0.5f) 
        << "墙的总长度必须等于房间周长";
}

TEST_F(WallGeneratorDeepTest, Walls_FormClosedLoop) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateSimpleRoom();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    // 验证每面墙的终点连接到另一面墙的起点或终点（形成闭环）
    for (const auto& wall : walls) {
        bool foundConnection = false;
        for (const auto& other : walls) {
            if (&wall != &other) {
                // 检查wall.end是否连接到other.start或other.end
                float dx1 = wall.end[0] - other.start[0];
                float dy1 = wall.end[1] - other.start[1];
                float dist1 = std::sqrt(dx1*dx1 + dy1*dy1);
                
                float dx2 = wall.end[0] - other.end[0];
                float dy2 = wall.end[1] - other.end[1];
                float dist2 = std::sqrt(dx2*dx2 + dy2*dy2);
                
                if (dist1 < 0.1f || dist2 < 0.1f) {
                    foundConnection = true;
                    break;
                }
            }
        }
        EXPECT_TRUE(foundConnection) 
            << "墙的终点必须连接到另一面墙的起点或终点，形成闭环";
    }
}

TEST_F(WallGeneratorDeepTest, InteriorWall_PositionedBetweenSpaces) {
    // 创建两个相邻房间 - 左右布局
    definition.grid = 0.5f;
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {20.0f, 10.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    // 左侧房间 (0,0) -> (10,10)
    Space space1;
    space1.spaceId = 1;
    space1.properties.ceilingHeight = 3.0f;
    space1.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space1);
    
    // 右侧房间 (10,0) -> (20,10)
    Space space2;
    space2.spaceId = 2;
    space2.properties.ceilingHeight = 3.0f;
    space2.rects.push_back({
        "rect_2",
        {10.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space2);
    
    definition.floors.push_back(floor);
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    // 必须有内墙
    int interiorWallCount = 0;
    for (const auto& wall : walls) {
        if (wall.type == WallType::Interior) {
            interiorWallCount++;
            
            // 内墙应该在 x=10 的位置（两个空间的分界线）
            bool isAtBoundary = (std::abs(wall.start[0] - 10.0f) < 0.2f && 
                               std::abs(wall.end[0] - 10.0f) < 0.2f);
            EXPECT_TRUE(isAtBoundary) 
                << "内墙必须位于两个空间的边界上 x=10";
        }
    }
    
    EXPECT_GT(interiorWallCount, 0) << "两个相邻空间之间必须有内墙";
}

TEST_F(WallGeneratorDeepTest, WallThickness_CorrectlyApplied) {
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
    
    Space space;
    space.spaceId = 1;
    space.properties.ceilingHeight = 3.0f;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space);
    definition.floors.push_back(floor);
    
    float testThickness = 0.25f;
    wallGenerator.SetWallThickness(testThickness);
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    // 验证所有墙的厚度
    for (const auto& wall : walls) {
        EXPECT_FLOAT_EQ(wall.thickness, testThickness) 
            << "所有墙的厚度必须是设置的值";
    }
}

TEST_F(WallGeneratorDeepTest, WallHeight_UsesSpaceCeilingHeight) {
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
    
    Space space;
    space.spaceId = 1;
    space.properties.ceilingHeight = 3.5f;  // 自定义高度
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space);
    definition.floors.push_back(floor);
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    // 验证墙高度使用空间的天花板高度
    for (const auto& wall : walls) {
        EXPECT_FLOAT_EQ(wall.height, 3.5f) 
            << "墙的高度必须使用空间的 ceiling_height";
    }
}

TEST_F(WallGeneratorDeepTest, SharedWall_OnlyGeneratedOnce) {
    // 创建两个共享墙的相邻房间
    definition.grid = 0.5f;
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {20.0f, 10.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    Space space1;
    space1.spaceId = 1;
    space1.properties.ceilingHeight = 3.0f;
    space1.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space1);
    
    Space space2;
    space2.spaceId = 2;
    space2.properties.ceilingHeight = 3.0f;
    space2.rects.push_back({
        "rect_2",
        {10.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space2);
    
    definition.floors.push_back(floor);
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    // 计算在 x=10 位置的墙（共享边界）
    int wallsAtBoundary = 0;
    for (const auto& wall : walls) {
        if (std::abs(wall.start[0] - 10.0f) < 0.2f && 
            std::abs(wall.end[0] - 10.0f) < 0.2f) {
            wallsAtBoundary++;
        }
    }
    
    // 共享墙应该只生成一次（作为内墙），不应该重复
    EXPECT_EQ(wallsAtBoundary, 1) 
        << "共享墙应该只生成一次，不应该为每个空间都生成";
}

TEST_F(WallGeneratorDeepTest, LShapedBuilding_CorrectExteriorWalls) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateLShapedBuilding();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    
    // L形建筑应该有 > 4 面外墙（不是简单矩形）
    int exteriorWallCount = 0;
    for (const auto& wall : walls) {
        if (wall.type == WallType::Exterior) {
            exteriorWallCount++;
        }
    }
    
    EXPECT_GT(exteriorWallCount, 4) 
        << "L形建筑应该有超过4面外墙（不是矩形）";
    
    // 验证外墙形成闭环
    for (const auto& wall : walls) {
        if (wall.type == WallType::Exterior) {
            bool foundConnection = false;
            for (const auto& other : walls) {
                if (&wall != &other && other.type == WallType::Exterior) {
                    // 检查wall.end是否连接到other.start或other.end
                    float dx1 = wall.end[0] - other.start[0];
                    float dy1 = wall.end[1] - other.start[1];
                    float dist1 = std::sqrt(dx1*dx1 + dy1*dy1);
                    
                    float dx2 = wall.end[0] - other.end[0];
                    float dy2 = wall.end[1] - other.end[1];
                    float dist2 = std::sqrt(dx2*dx2 + dy2*dy2);
                    
                    if (dist1 < 0.2f || dist2 < 0.2f) {
                        foundConnection = true;
                        break;
                    }
                }
            }
            EXPECT_TRUE(foundConnection) 
                << "外墙必须形成闭环";
        }
    }
}
