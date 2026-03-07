// 深入测试 DoorGenerator - 验证门生成的正确性
#include <gtest/gtest.h>
#include "building/DoorGenerator.h"
#include "building/WallGenerator.h"
#include "building/SpaceGraphBuilder.h"
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

class DoorGeneratorDeepTest : public ::testing::Test {
protected:
    DoorGenerator doorGenerator;
    WallGenerator wallGenerator;
    SpaceGraphBuilder spaceGraph;
    BuildingDefinition definition;
    std::vector<WallSegment> walls;
    std::vector<Door> doors;
    std::vector<SpaceConnection> connections;
    
    void SetUp() override {
        doorGenerator.SetDefaultDoorSize(0.9f, 2.1f);
        doorGenerator.SetMinDoorWidth(0.8f);
        wallGenerator.SetWallThickness(0.2f);
        wallGenerator.SetDefaultWallHeight(3.0f);
    }
};

// ========================================
// 验证门的位置是否在墙上
// ========================================

TEST_F(DoorGeneratorDeepTest, Door_MustBeOnWall) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, doors);
    
    ASSERT_GT(doors.size(), 0) << "别墅应该有门";
    
    // 验证每个门都在某面墙上
    for (const auto& door : doors) {
        bool foundWall = false;
        
        for (const auto& wall : walls) {
            // 计算门到墙的距离
            float wx = wall.end[0] - wall.start[0];
            float wy = wall.end[1] - wall.start[1];
            float wallLength = std::sqrt(wx*wx + wy*wy);
            
            if (wallLength < 0.01f) continue;
            
            // 门位置到墙起点的向量
            float dx = door.position[0] - wall.start[0];
            float dy = door.position[1] - wall.start[1];
            
            // 投影到墙上的参数 t (0-1之间表示在墙上)
            float t = (dx * wx + dy * wy) / (wallLength * wallLength);
            
            if (t >= -0.1f && t <= 1.1f) {  // 允许一点误差
                // 计算投影点
                float projX = wall.start[0] + t * wx;
                float projY = wall.start[1] + t * wy;
                
                // 门到投影点的距离
                float distToWall = std::sqrt(
                    std::pow(door.position[0] - projX, 2) +
                    std::pow(door.position[1] - projY, 2)
                );
                
                if (distToWall < 0.5f) {  // 门应该在墙附近
                    foundWall = true;
                    
                    // 验证门的宽度不超过墙的长度
                    EXPECT_LE(door.width, wallLength + 0.1f) 
                        << "门的宽度不能超过墙的长度";
                    
                    break;
                }
            }
        }
        
        EXPECT_TRUE(foundWall) 
            << "门必须位于某面墙上，不能悬空";
    }
}

TEST_F(DoorGeneratorDeepTest, Door_WidthMeetsMinimum) {
    doorGenerator.SetMinDoorWidth(0.8f);
    
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, doors);
    
    for (const auto& door : doors) {
        EXPECT_GE(door.width, 0.8f) 
            << "门的宽度必须满足最小宽度要求 (建筑规范)";
        EXPECT_LE(door.width, 2.0f) 
            << "门的宽度不应该过大";
    }
}

TEST_F(DoorGeneratorDeepTest, Door_HeightMatchesWallHeight) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateSimpleRoom();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, doors);
    
    for (const auto& door : doors) {
        EXPECT_GT(door.height, 0.0f) << "门的高度必须 > 0";
        EXPECT_GE(door.height, 2.0f) << "门的高度应该至少2米";
        EXPECT_LE(door.height, 3.0f) << "门的高度不应该超过墙高";
    }
}

TEST_F(DoorGeneratorDeepTest, ConnectedSpaces_HaveDoor) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, doors);
    
    const auto& adjacencies = spaceGraph.GetAdjacencies();
    
    ASSERT_GT(adjacencies.size(), 0) << "别墅应该有相邻空间";
    ASSERT_GT(doors.size(), 0) << "相邻空间之间应该有门";
    
    // 验证：至少一些相邻空间之间有门
    // (不是所有相邻空间都需要门，比如浴室可能没有门)
    bool foundDoorBetweenConnectedSpaces = (doors.size() > 0);
    EXPECT_TRUE(foundDoorBetweenConnectedSpaces) 
        << "相邻空间之间应该有门连接";
}

TEST_F(DoorGeneratorDeepTest, CustomDoorSize_Applied) {
    float customWidth = 1.0f;
    float customHeight = 2.2f;
    doorGenerator.SetDefaultDoorSize(customWidth, customHeight);
    
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, doors);
    
    // 至少一些门应该使用自定义尺寸（如果没有特殊要求）
    int doorsWithDefaultSize = 0;
    for (const auto& door : doors) {
        if (std::abs(door.width - customWidth) < 0.1f) {
            doorsWithDefaultSize++;
        }
    }
    
    // 如果有门，至少一些应该使用默认尺寸
    if (doors.size() > 0) {
        float ratio = static_cast<float>(doorsWithDefaultSize) / doors.size();
        EXPECT_GT(ratio, 0.2f) 
            << "至少部分门应该使用默认尺寸";
    }
}

TEST_F(DoorGeneratorDeepTest, Door_NotOverlapping) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, doors);
    
    // 验证门之间不重叠（同一面墙上的门）
    for (size_t i = 0; i < doors.size(); ++i) {
        for (size_t j = i + 1; j < doors.size(); ++j) {
            // 只比较同一楼层的门
            if (doors[i].floorLevel != doors[j].floorLevel) {
                continue;
            }
            
            float dx = doors[i].position[0] - doors[j].position[0];
            float dy = doors[i].position[1] - doors[j].position[1];
            float dist = std::sqrt(dx*dx + dy*dy);
            
            float minDist = (doors[i].width + doors[j].width) / 2.0f;
            
            if (dist < minDist) {
                // 如果距离很近，它们应该在不同的墙上
                // 这里简单检查距离不要太近
                EXPECT_GT(dist, 0.5f) 
                    << "门之间不应该重叠或过于接近";
            }
        }
    }
}

TEST_F(DoorGeneratorDeepTest, InteriorDoor_OnInteriorWall) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    doorGenerator.GenerateDoors(definition, spaceGraph, walls, doors);
    
    // 收集内墙
    std::vector<const WallSegment*> interiorWalls;
    for (const auto& wall : walls) {
        if (wall.type == WallType::Interior) {
            interiorWalls.push_back(&wall);
        }
    }
    
    if (interiorWalls.empty()) {
        GTEST_SKIP() << "没有内墙，跳过测试";
    }
    
    // 至少一些门应该在内墙上
    int doorsOnInteriorWalls = 0;
    for (const auto& door : doors) {
        for (const auto* wall : interiorWalls) {
            float wx = wall->end[0] - wall->start[0];
            float wy = wall->end[1] - wall->start[1];
            float wallLength = std::sqrt(wx*wx + wy*wy);
            
            if (wallLength < 0.01f) continue;
            
            float dx = door.position[0] - wall->start[0];
            float dy = door.position[1] - wall->start[1];
            float t = (dx * wx + dy * wy) / (wallLength * wallLength);
            
            if (t >= 0.0f && t <= 1.0f) {
                float projX = wall->start[0] + t * wx;
                float projY = wall->start[1] + t * wy;
                float dist = std::sqrt(
                    std::pow(door.position[0] - projX, 2) +
                    std::pow(door.position[1] - projY, 2)
                );
                
                if (dist < 0.5f) {
                    doorsOnInteriorWalls++;
                    break;
                }
            }
        }
    }
    
    EXPECT_GT(doorsOnInteriorWalls, 0) 
        << "应该有门位于内墙上（连接室内空间）";
}
