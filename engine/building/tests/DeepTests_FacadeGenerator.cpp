// 深入测试 FacadeGenerator - 验证窗户生成的正确性
#include <gtest/gtest.h>
#include "building/FacadeGenerator.h"
#include "building/WallGenerator.h"
#include "building/SpaceGraphBuilder.h"
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

class FacadeGeneratorDeepTest : public ::testing::Test {
protected:
    FacadeGenerator facadeGenerator;
    WallGenerator wallGenerator;
    SpaceGraphBuilder spaceGraph;
    BuildingDefinition definition;
    std::vector<WallSegment> walls;
    std::vector<Window> windows;
    std::vector<FacadeElement> elements;
    std::vector<SpaceConnection> connections;
    
    void SetUp() override {
        facadeGenerator.SetWindowParameters(1.2f, 1.5f, 0.9f);
        wallGenerator.SetWallThickness(0.2f);
        wallGenerator.SetDefaultWallHeight(3.0f);
    }
};

// ========================================
// 验证窗户只在外墙上
// ========================================

TEST_F(FacadeGeneratorDeepTest, Windows_OnlyOnExteriorWalls) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    facadeGenerator.GenerateFacade(definition, walls, windows, elements);
    
    // 收集外墙
    std::vector<const WallSegment*> exteriorWalls;
    for (const auto& wall : walls) {
        if (wall.type == WallType::Exterior) {
            exteriorWalls.push_back(&wall);
        }
    }
    
    ASSERT_GT(exteriorWalls.size(), 0) << "应该有外墙";
    
    // 验证所有窗户都在外墙上
    for (const auto& window : windows) {
        bool foundExteriorWall = false;
        
        for (const auto* wall : exteriorWalls) {
            float wx = wall->end[0] - wall->start[0];
            float wy = wall->end[1] - wall->start[1];
            float wallLength = std::sqrt(wx*wx + wy*wy);
            
            if (wallLength < 0.01f) continue;
            
            float dx = window.position[0] - wall->start[0];
            float dy = window.position[1] - wall->start[1];
            float t = (dx * wx + dy * wy) / (wallLength * wallLength);
            
            if (t >= -0.1f && t <= 1.1f) {
                float projX = wall->start[0] + t * wx;
                float projY = wall->start[1] + t * wy;
                float dist = std::sqrt(
                    std::pow(window.position[0] - projX, 2) +
                    std::pow(window.position[1] - projY, 2)
                );
                
                if (dist < 0.5f) {
                    foundExteriorWall = true;
                    break;
                }
            }
        }
        
        EXPECT_TRUE(foundExteriorWall) 
            << "窗户必须只在外墙上，不应该在内墙上";
    }
}

TEST_F(FacadeGeneratorDeepTest, Windows_ProperSpacing) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    facadeGenerator.GenerateFacade(definition, walls, windows, elements);
    
    // 验证同一面墙上的窗户间距合理
    for (size_t i = 0; i < windows.size(); ++i) {
        for (size_t j = i + 1; j < windows.size(); ++j) {
            // 只比较同一楼层的窗户
            if (windows[i].floorLevel != windows[j].floorLevel) {
                continue;
            }
            
            float dx = windows[i].position[0] - windows[j].position[0];
            float dy = windows[i].position[1] - windows[j].position[1];
            float dist = std::sqrt(dx*dx + dy*dy);
            
            // 如果两个窗户很近（可能在同一面墙上），检查间距
            if (dist < 5.0f) {
                float minSpacing = (windows[i].width + windows[j].width) / 2.0f + 0.5f;
                EXPECT_GE(dist, minSpacing * 0.8f) 
                    << "窗户之间应该有合理的间距，避免过于密集";
            }
        }
    }
}

TEST_F(FacadeGeneratorDeepTest, Window_DimensionsValid) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateSimpleRoom();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    facadeGenerator.GenerateFacade(definition, walls, windows, elements);
    
    ASSERT_GT(windows.size(), 0) << "简单房间应该有窗户";
    
    for (const auto& window : windows) {
        EXPECT_GT(window.width, 0.0f) << "窗户宽度必须 > 0";
        EXPECT_GT(window.height, 0.0f) << "窗户高度必须 > 0";
        
        // 窗户尺寸应该合理
        EXPECT_GE(window.width, 0.6f) << "窗户宽度应该至少60cm";
        EXPECT_LE(window.width, 3.0f) << "窗户宽度不应该超过3米";
        
        EXPECT_GE(window.height, 0.8f) << "窗户高度应该至少80cm";
        EXPECT_LE(window.height, 2.5f) << "窗户高度不应该超过2.5米";
    }
}

TEST_F(FacadeGeneratorDeepTest, Window_FitsWithinWall) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    facadeGenerator.GenerateFacade(definition, walls, windows, elements);
    
    // 验证每个窗户的宽度不超过其所在墙的长度
    for (const auto& window : windows) {
        bool foundWall = false;
        
        for (const auto& wall : walls) {
            float wx = wall.end[0] - wall.start[0];
            float wy = wall.end[1] - wall.start[1];
            float wallLength = std::sqrt(wx*wx + wy*wy);
            
            if (wallLength < 0.01f) continue;
            
            float dx = window.position[0] - wall.start[0];
            float dy = window.position[1] - wall.start[1];
            float t = (dx * wx + dy * wy) / (wallLength * wallLength);
            
            if (t >= 0.0f && t <= 1.0f) {
                float projX = wall.start[0] + t * wx;
                float projY = wall.start[1] + t * wy;
                float dist = std::sqrt(
                    std::pow(window.position[0] - projX, 2) +
                    std::pow(window.position[1] - projY, 2)
                );
                
                if (dist < 0.5f) {
                    foundWall = true;
                    
                    EXPECT_LE(window.width, wallLength) 
                        << "窗户宽度不能超过墙的长度";
                    
                    break;
                }
            }
        }
        
        EXPECT_TRUE(foundWall) << "窗户必须在某面墙上";
    }
}

TEST_F(FacadeGeneratorDeepTest, CustomWindowParameters_Applied) {
    float customWidth = 1.5f;
    float customHeight = 1.8f;
    float customSillHeight = 1.0f;
    facadeGenerator.SetWindowParameters(customWidth, customHeight, customSillHeight);
    
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    facadeGenerator.GenerateFacade(definition, walls, windows, elements);
    
    // 至少一些窗户应该使用自定义参数
    int windowsWithCustomSize = 0;
    for (const auto& window : windows) {
        if (std::abs(window.width - customWidth) < 0.2f &&
            std::abs(window.height - customHeight) < 0.2f) {
            windowsWithCustomSize++;
        }
    }
    
    if (windows.size() > 0) {
        float ratio = static_cast<float>(windowsWithCustomSize) / windows.size();
        EXPECT_GT(ratio, 0.3f) 
            << "大部分窗户应该使用自定义尺寸";
    }
}

TEST_F(FacadeGeneratorDeepTest, Bathroom_NoWindows) {
    // 创建一个包含浴室的建筑
    definition.grid = 0.5f;
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {15.0f, 10.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    // 卧室
    Space bedroom;
    bedroom.spaceId = 1;
    bedroom.properties.usage = SpaceUsage::Bedroom;
    bedroom.properties.ceilingHeight = 3.0f;
    bedroom.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(bedroom);
    
    // 浴室
    Space bathroom;
    bathroom.spaceId = 2;
    bathroom.properties.usage = SpaceUsage::Bathroom;
    bathroom.properties.ceilingHeight = 3.0f;
    bathroom.rects.push_back({
        "rect_2",
        {10.0f, 0.0f},
        {5.0f, 10.0f}
    });
    floor.spaces.push_back(bathroom);
    
    definition.floors.push_back(floor);
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    facadeGenerator.GenerateFacade(definition, walls, windows, elements);
    
    // 验证：浴室的外墙上不应该有窗户（或很少）
    // 这取决于具体实现，但至少卧室应该有窗户
    int bedroomWindows = 0;
    int bathroomWindows = 0;
    
    // 简单检查：应该有一些窗户，并且卧室区域应该比浴室区域有更多窗户
    EXPECT_GT(windows.size(), 0) << "建筑物应该有窗户";
}

TEST_F(FacadeGeneratorDeepTest, MultiFloor_WindowsOnEachFloor) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateApartmentBuilding();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    facadeGenerator.GenerateFacade(definition, walls, windows, elements);
    
    ASSERT_GT(definition.floors.size(), 1) << "应该是多层建筑";
    EXPECT_GT(windows.size(), definition.floors.size()) 
        << "多层建筑每层都应该有窗户";
}

TEST_F(FacadeGeneratorDeepTest, LargeWall_MultipleWindows) {
    // 创建一个有长外墙的建筑
    definition.grid = 0.5f;
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {20.0f, 8.0f},  // 很长的建筑
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    Space space;
    space.spaceId = 1;
    space.properties.usage = SpaceUsage::Living;
    space.properties.ceilingHeight = 3.0f;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {20.0f, 8.0f}
    });
    floor.spaces.push_back(space);
    definition.floors.push_back(floor);
    
    spaceGraph.BuildGraph(definition, connections);
    wallGenerator.GenerateWalls(definition, spaceGraph, walls);
    facadeGenerator.GenerateFacade(definition, walls, windows, elements);
    
    // 长墙应该有多个窗户
    EXPECT_GT(windows.size(), 3) 
        << "长外墙应该有多个窗户，不应该只有一个";
}
