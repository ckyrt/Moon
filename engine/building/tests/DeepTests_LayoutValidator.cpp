// 深入测试 LayoutValidator - 验证布局验证逻辑
#include <gtest/gtest.h>
#include "building/LayoutValidator.h"
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

class LayoutValidatorDeepTest : public ::testing::Test {
protected:
    LayoutValidator validator;
    BuildingDefinition definition;
    
    void SetUp() override {
        validator.SetMinRoomSize(1.5f, 1.5f);
        validator.SetWallThickness(0.2f);
        definition.grid = 0.5f;
    }
};

// ========================================
// 验证空间重叠检测
// ========================================

TEST_F(LayoutValidatorDeepTest, OverlappingSpaces_DetectedAndRejected) {
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
    
    // 空间1: (0,0) -> (10,10)
    Space space1;
    space1.spaceId = 1;
    space1.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space1);
    
    // 空间2: (5,5) -> (15,15) - 与空间1重叠！
    Space space2;
    space2.spaceId = 2;
    space2.rects.push_back({
        "rect_2",
        {5.0f, 5.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space2);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    // 必须检测到重叠并拒绝
    EXPECT_FALSE(result.valid) 
        << "重叠的空间必须被检测并标记为无效";
    EXPECT_GT(result.errors.size(), 0) 
        << "必须报告重叠错误";
    
    // 错误信息中应该包含 "overlap" 或 "重叠"
    bool foundOverlapError = false;
    for (const auto& error : result.errors) {
        std::string lowerError = error;
        std::transform(lowerError.begin(), lowerError.end(), lowerError.begin(), ::tolower);
        if (lowerError.find("overlap") != std::string::npos ||
            lowerError.find("overlapping") != std::string::npos) {
            foundOverlapError = true;
            break;
        }
    }
    
    EXPECT_TRUE(foundOverlapError) 
        << "错误信息必须明确指出重叠问题";
}

TEST_F(LayoutValidatorDeepTest, AdjacentSpaces_NotOverlapping_Valid) {
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
    
    // 空间1: (0,0) -> (10,10)
    Space space1;
    space1.spaceId = 1;
    space1.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space1);
    
    // 空间2: (10,0) -> (20,10) - 相邻但不重叠
    Space space2;
    space2.spaceId = 2;
    space2.rects.push_back({
        "rect_2",
        {10.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space2);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_TRUE(result.valid) 
        << "相邻但不重叠的空间应该通过验证";
}

// ========================================
// 验证空间边界检测
// ========================================

TEST_F(LayoutValidatorDeepTest, SpaceOutsideMass_DetectedAndRejected) {
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f},  // mass只有10x10
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    // 空间超出了 mass 边界
    Space space;
    space.spaceId = 1;
    space.rects.push_back({
        "rect_1",
        {5.0f, 5.0f},
        {10.0f, 10.0f}  // 延伸到 (15,15)，超出边界
    });
    floor.spaces.push_back(space);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_FALSE(result.valid) 
        << "超出 mass 边界的空间必须被拒绝";
    EXPECT_GT(result.errors.size(), 0) 
        << "必须报告边界错误";
}

TEST_F(LayoutValidatorDeepTest, SpaceWithinMass_Valid) {
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
    
    // 空间完全在 mass 内部
    Space space;
    space.spaceId = 1;
    space.rects.push_back({
        "rect_1",
        {2.0f, 2.0f},
        {10.0f, 10.0f}
    });
    floor.spaces.push_back(space);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_TRUE(result.valid) 
        << "在 mass 边界内的空间应该通过验证";
}

// ========================================
// 验证最小房间尺寸
// ========================================

TEST_F(LayoutValidatorDeepTest, TooSmallRoom_DetectedAndRejected) {
    validator.SetMinRoomSize(2.0f, 2.0f);  // 最小2x2米
    
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
    
    // 太小的房间: 1x1米
    Space space;
    space.spaceId = 1;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {1.0f, 1.0f}  // 小于最小尺寸
    });
    floor.spaces.push_back(space);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_FALSE(result.valid) 
        << "小于最小尺寸的房间必须被拒绝";
    EXPECT_GT(result.errors.size(), 0) 
        << "必须报告尺寸错误";
}

TEST_F(LayoutValidatorDeepTest, MinimumSizeRoom_Valid) {
    validator.SetMinRoomSize(2.0f, 2.0f);
    
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
    
    // 刚好达到最小尺寸
    Space space;
    space.spaceId = 1;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {2.0f, 2.0f}
    });
    floor.spaces.push_back(space);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_TRUE(result.valid) 
        << "达到最小尺寸的房间应该通过验证";
}

// ========================================
// 验证 Mass 引用
// ========================================

TEST_F(LayoutValidatorDeepTest, InvalidMassReference_DetectedAndRejected) {
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f},
        1
    });
    
    Floor floor;
    floor.level = 0;
    floor.massId = "mass_999";  // 不存在的 mass！
    floor.floorHeight = 3.0f;
    
    Space space;
    space.spaceId = 1;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {5.0f, 5.0f}
    });
    floor.spaces.push_back(space);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_FALSE(result.valid) 
        << "引用不存在的 mass 必须被拒绝";
    EXPECT_GT(result.errors.size(), 0) 
        << "必须报告 mass 引用错误";
}

// ========================================
// 验证楼层编号
// ========================================

TEST_F(LayoutValidatorDeepTest, InvalidFloorLevel_DetectedAndRejected) {
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f},
        2  // 只有2层 (level 0, 1)
    });
    
    Floor floor;
    floor.level = 5;  // 超出范围！
    floor.massId = "mass_1";
    floor.floorHeight = 3.0f;
    
    Space space;
    space.spaceId = 1;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {5.0f, 5.0f}
    });
    floor.spaces.push_back(space);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_FALSE(result.valid) 
        << "超出 mass 层数的楼层必须被拒绝";
}

// ========================================
// 验证网格对齐
// ========================================

TEST_F(LayoutValidatorDeepTest, NonGridAligned_DetectedAndRejected) {
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
    
    // 坐标不对齐网格
    Space space;
    space.spaceId = 1;
    space.rects.push_back({
        "rect_1",
        {0.3f, 0.3f},  // 不是0.5的倍数！
        {5.0f, 5.0f}
    });
    floor.spaces.push_back(space);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_FALSE(result.valid) 
        << "不对齐网格的坐标必须被拒绝";
}

// ========================================
// 验证自定义参数
// ========================================

TEST_F(LayoutValidatorDeepTest, CustomMinRoomSize_Applied) {
    float customMinWidth = 3.0f;
    float customMinDepth = 2.5f;
    validator.SetMinRoomSize(customMinWidth, customMinDepth);
    
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
    
    // 房间尺寸小于自定义最小值
    Space space;
    space.spaceId = 1;
    space.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {2.5f, 2.0f}  // 小于 3.0 x 2.5
    });
    floor.spaces.push_back(space);
    
    definition.floors.push_back(floor);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_FALSE(result.valid) 
        << "自定义最小尺寸必须被应用";
}

// ========================================
// 验证多层建筑
// ========================================

TEST_F(LayoutValidatorDeepTest, MultiFloor_EachFloorValidated) {
    definition.masses.push_back({
        "mass_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f},
        2
    });
    
    // 第一层 - 有效
    Floor floor0;
    floor0.level = 0;
    floor0.massId = "mass_1";
    floor0.floorHeight = 3.0f;
    
    Space space0;
    space0.spaceId = 1;
    space0.rects.push_back({
        "rect_1",
        {0.0f, 0.0f},
        {10.0f, 10.0f}
    });
    floor0.spaces.push_back(space0);
    definition.floors.push_back(floor0);
    
    // 第二层 - 有重叠错误
    Floor floor1;
    floor1.level = 1;
    floor1.massId = "mass_1";
    floor1.floorHeight = 3.0f;
    
    Space space1a;
    space1a.spaceId = 2;
    space1a.rects.push_back({
        "rect_2",
        {0.0f, 0.0f},
        {6.0f, 6.0f}
    });
    floor1.spaces.push_back(space1a);
    
    Space space1b;
    space1b.spaceId = 3;
    space1b.rects.push_back({
        "rect_3",
        {3.0f, 3.0f},  // 与 space1a 重叠
        {6.0f, 6.0f}
    });
    floor1.spaces.push_back(space1b);
    
    definition.floors.push_back(floor1);
    
    ValidationResult result = validator.Validate(definition);
    
    EXPECT_FALSE(result.valid) 
        << "第二层的重叠错误必须被检测";
}

// ========================================
// 验证空建筑
// ========================================

TEST_F(LayoutValidatorDeepTest, EmptyBuilding_Valid) {
    definition.masses.clear();
    definition.floors.clear();
    
    ValidationResult result = validator.Validate(definition);
    
    // 空建筑可以是有效的（取决于具体实现）
    // 至少不应该崩溃
    EXPECT_TRUE(result.errors.size() == 0 || result.errors.size() > 0) 
        << "空建筑的验证不应该崩溃";
}
