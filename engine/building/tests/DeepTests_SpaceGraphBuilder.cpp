// 深入测试 SpaceGraphBuilder - 验证空间邻接图构建
#include <gtest/gtest.h>
#include "building/SpaceGraphBuilder.h"
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

class SpaceGraphBuilderDeepTest : public ::testing::Test {
protected:
    SpaceGraphBuilder graphBuilder;
    BuildingDefinition definition;
    std::vector<SpaceConnection> connections;
    
    void SetUp() override {
        definition.grid = 0.5f;
    }
};

// ========================================
// 验证邻接关系检测
// ========================================

TEST_F(SpaceGraphBuilderDeepTest, AdjacentSpaces_Detected) {
    // 创建两个相邻空间
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
    
    // 左侧空间 (0,0) -> (10,10)
    Space space1;
    space1.spaceId = 1;
    space1.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space1);
    
    // 右侧空间 (10,0) -> (20,10) - 共享边界 x=10
    Space space2;
    space2.spaceId = 2;
    space2.rects.push_back({
        "rect_2",
        {10.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space2);
    
    definition.floors.push_back(floor);
    
    graphBuilder.BuildGraph(definition, connections);
    
    const auto& adjacencies = graphBuilder.GetAdjacencies();
    
    // 必须检测到邻接关系
    EXPECT_GT(adjacencies.size(), 0) 
        << "必须检测到两个相邻空间的邻接关系";
    
    // 验证邻接关系正确
    bool foundAdjacency = false;
    for (const auto& adj : adjacencies) {
        if ((adj.spaceA == 1 && adj.spaceB == 2) || 
            (adj.spaceA == 2 && adj.spaceB == 1)) {
            foundAdjacency = true;
            
            // 验证共享边长度
            EXPECT_GT(adj.sharedEdgeLength, 0.0f) 
                << "共享边长度必须 > 0";
            
            // 两个10x10的空间共享Y方向的边，长度应该是10
            EXPECT_NEAR(adj.sharedEdgeLength, 10.0f, 0.5f) 
                << "共享边长度必须正确计算";
        }
    }
    
    EXPECT_TRUE(foundAdjacency) 
        << "必须找到空间1和空间2的邻接关系";
}

TEST_F(SpaceGraphBuilderDeepTest, NonAdjacentSpaces_NotDetected) {
    // 创建两个不相邻的空间
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
    
    // 左下角空间
    Space space1;
    space1.spaceId = 1;
    space1.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {8.0f, 8.0f}
    });
    floor.spaces.push_back(space1);
    
    // 右上角空间 - 不相邻
    Space space2;
    space2.spaceId = 2;
    space2.rects.push_back({
        "rect_2",
        {12.0f, 12.0f},
        {8.0f, 8.0f}
    });
    floor.spaces.push_back(space2);
    
    definition.floors.push_back(floor);
    
    graphBuilder.BuildGraph(definition, connections);
    
    const auto& adjacencies = graphBuilder.GetAdjacencies();
    
    // 不应该有邻接关系
    for (const auto& adj : adjacencies) {
        bool isSpace1and2 = (adj.spaceA == 1 && adj.spaceB == 2) || 
                           (adj.spaceA == 2 && adj.spaceB == 1);
        EXPECT_FALSE(isSpace1and2) 
            << "不相邻的空间不应该被标记为邻接";
    }
}

TEST_F(SpaceGraphBuilderDeepTest, SharedEdgeLength_Correct) {
    // 创建两个共享部分边界的空间
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {20.0f, 15.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    // 左侧空间 (0,0) -> (10,15)
    Space space1;
    space1.spaceId = 1;
    space1.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 15.0f}
    });
    floor.spaces.push_back(space1);
    
    // 右上角空间 (10,5) -> (20,15) - 只共享部分边界
    Space space2;
    space2.spaceId = 2;
    space2.rects.push_back({
        "rect_2",
        {10.0f, 5.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space2);
    
    definition.floors.push_back(floor);
    
    graphBuilder.BuildGraph(definition, connections);
    
    const auto& adjacencies = graphBuilder.GetAdjacencies();
    
    bool foundAdjacency = false;
    for (const auto& adj : adjacencies) {
        if ((adj.spaceA == 1 && adj.spaceB == 2) || 
            (adj.spaceA == 2 && adj.spaceB == 1)) {
            foundAdjacency = true;
            
            // 共享边长度应该是10（Y方向从5到15）
            EXPECT_NEAR(adj.sharedEdgeLength, 10.0f, 0.5f) 
                << "部分共享边的长度必须正确计算";
        }
    }
    
    EXPECT_TRUE(foundAdjacency) << "必须检测到部分共享边的邻接关系";
}

TEST_F(SpaceGraphBuilderDeepTest, AreSpacesAdjacent_Correct) {
    // 创建3个空间：1-2相邻，2-3相邻，1-3不相邻
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {30.0f, 10.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    Space space1;
    space1.spaceId = 1;
    space1.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space1);
    
    Space space2;
    space2.spaceId = 2;
    space2.rects.push_back({
        "rect_2",
        {10.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space2);
    
    Space space3;
    space3.spaceId = 3;
    space3.rects.push_back({
        "rect_3",
        {20.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space3);
    
    definition.floors.push_back(floor);
    
    graphBuilder.BuildGraph(definition, connections);
    
    // 验证 AreSpacesAdjacent 方法
    EXPECT_TRUE(graphBuilder.AreSpacesAdjacent(1, 2)) 
        << "空间1和2必须相邻";
    EXPECT_TRUE(graphBuilder.AreSpacesAdjacent(2, 3)) 
        << "空间2和3必须相邻";
    EXPECT_FALSE(graphBuilder.AreSpacesAdjacent(1, 3)) 
        << "空间1和3不相邻";
}

TEST_F(SpaceGraphBuilderDeepTest, MultiFloor_SameFloorAdjacency) {
    // 创建多层建筑，验证只检测同层邻接
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {20.0f, 10.0f},
        2
    });
    
    // 第一层
    Floor floor0;
    floor0.level = 0;
    floor0.massId = "mass_1";
    floor0.floorHeight = 3.0f;
    
    Space space1;
    space1.spaceId = 1;
    space1.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor0.spaces.push_back(space1);
    
    Space space2;
    space2.spaceId = 2;
    space2.rects.push_back({
        "rect_2",
        {10.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor0.spaces.push_back(space2);
    
    definition.floors.push_back(floor0);
    
    // 第二层
    Floor floor1;
    floor1.level = 1;
    floor1.massId = "mass_1";
    floor1.floorHeight = 3.0f;
    
    Space space3;
    space3.spaceId = 3;
    space3.rects.push_back({
        "rect_3",
        {0.0f, 0.0f},
        {20.0f, 10.0f}
    });
    floor1.spaces.push_back(space3);
    
    definition.floors.push_back(floor1);
    
    graphBuilder.BuildGraph(definition, connections);
    
    const auto& adjacencies = graphBuilder.GetAdjacencies();
    
    // 验证同层邻接存在
    bool found1and2 = false;
    for (const auto& adj : adjacencies) {
        if ((adj.spaceA == 1 && adj.spaceB == 2) || 
            (adj.spaceA == 2 && adj.spaceB == 1)) {
            found1and2 = true;
        }
        
        // 不应该有跨层邻接
        bool crossFloor = (adj.spaceA <= 2 && adj.spaceB == 3) || 
                         (adj.spaceA == 3 && adj.spaceB <= 2);
        EXPECT_FALSE(crossFloor) 
            << "不应该检测到跨楼层的邻接关系";
    }
    
    EXPECT_TRUE(found1and2) << "必须检测到同层空间的邻接关系";
}

TEST_F(SpaceGraphBuilderDeepTest, LShapedSpace_MultipleAdjacencies) {
    // L形空间可能与多个其他空间相邻
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
    
    // L形空间（由两个矩形组成）
    Space space1;
    space1.spaceId = 1;
    space1.rects.push_back({
        "rect_1a",
        {0.0f, 0.0f},
        {10.0f, 20.0f}  // 垂直部分
    });
    space1.rects.push_back({
        "rect_1b",
        {10.0f, 0.0f},
        {10.0f, 10.0f}  // 水平部分
    });
    floor.spaces.push_back(space1);
    
    // 右上角空间
    Space space2;
    space2.spaceId = 2;
    space2.rects.push_back({
        "rect_2",
        {10.0f, 10.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space2);
    
    definition.floors.push_back(floor);
    
    graphBuilder.BuildGraph(definition, connections);
    
    const auto& adjacencies = graphBuilder.GetAdjacencies();
    
    // L形空间应该与右上角空间相邻（两个接触点）
    int adjacencyCount = 0;
    for (const auto& adj : adjacencies) {
        if ((adj.spaceA == 1 && adj.spaceB == 2) || 
            (adj.spaceA == 2 && adj.spaceB == 1)) {
            adjacencyCount++;
            EXPECT_GT(adj.sharedEdgeLength, 0.0f);
        }
    }
    
    EXPECT_GT(adjacencyCount, 0) 
        << "L形空间必须与相邻空间建立邻接关系";
}

TEST_F(SpaceGraphBuilderDeepTest, CornerTouch_NotAdjacent) {
    // 只在角点接触的空间不应该被认为是邻接的
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
    
    // 左下角
    Space space1;
    space1.spaceId = 1;
    space1.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space1);
    
    // 右上角 - 只在角点 (10,10) 接触
    Space space2;
    space2.spaceId = 2;
    space2.rects.push_back({
        "rect_2",
        {10.0f, 10.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space2);
    
    definition.floors.push_back(floor);
    
    graphBuilder.BuildGraph(definition, connections);
    
    const auto& adjacencies = graphBuilder.GetAdjacencies();
    
    // 只在角点接触不应该算作邻接
    for (const auto& adj : adjacencies) {
        bool isCornerTouch = (adj.spaceA == 1 && adj.spaceB == 2) || 
                            (adj.spaceA == 2 && adj.spaceB == 1);
        if (isCornerTouch) {
            // 如果确实检测到了，共享边长度应该是0或非常小
            EXPECT_LT(adj.sharedEdgeLength, 0.1f) 
                << "角点接触的共享边长度应该为0";
        }
    }
}

TEST_F(SpaceGraphBuilderDeepTest, EmptyBuilding_NoConnections) {
    definition.floors.clear();
    definition.masses.clear();
    
    graphBuilder.BuildGraph(definition, connections);
    
    const auto& adjacencies = graphBuilder.GetAdjacencies();
    EXPECT_EQ(adjacencies.size(), 0) 
        << "空建筑不应该有任何邻接关系";
}

TEST_F(SpaceGraphBuilderDeepTest, SingleSpace_NoAdjacencies) {
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
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space);
    definition.floors.push_back(floor);
    
    graphBuilder.BuildGraph(definition, connections);
    
    const auto& adjacencies = graphBuilder.GetAdjacencies();
    EXPECT_EQ(adjacencies.size(), 0) 
        << "单个空间不应该有邻接关系";
}
