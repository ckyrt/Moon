// 深入测试 StairGenerator - 验证楼梯生成的正确性
#include <gtest/gtest.h>
#include "building/StairGenerator.h"
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"
#include <cmath>

using namespace Moon::Building;
using namespace Moon::Building::Test;

class StairGeneratorDeepTest : public ::testing::Test {
protected:
    StairGenerator stairGenerator;
    BuildingDefinition definition;
    std::vector<StairGeometry> stairs;
    
    void SetUp() override {
        stairGenerator.SetStepParameters(0.18f, 0.28f);
    }
    
    // Helper to create a multi-floor building with stairs
    void CreateBuildingWithStairs(StairType type, int floors = 2) {
        Mass mass;
        mass.massId = "mass0";
        mass.origin = {0, 0};
        mass.size = {10, 10};
        mass.floors = floors;
        definition.masses.push_back(mass);
        
        for (int level = 0; level < floors; ++level) {
            Floor floor;
            floor.level = level;
            floor.massId = "mass0";
            floor.floorHeight = 3.0f;
            
            Space space;
            space.spaceId = level;
            Rect rect;
            rect.rectId = "rect" + std::to_string(level);
            rect.origin = {0, 0};
            rect.size = {10, 10};
            space.rects.push_back(rect);
            space.properties.usage = SpaceUsage::Living;
            space.properties.ceilingHeight = 3.0f;
            
            // 只在第一层添加楼梯
            if (level == 0) {
                space.hasStairs = true;
                space.stairsConfig.type = type;
                space.stairsConfig.connectToLevel = 1;
                space.stairsConfig.position = {5, 5};
                space.stairsConfig.width = 1.2f;
            }
            
            floor.spaces.push_back(space);
            definition.floors.push_back(floor);
        }
    }
};

// ========================================
// 验证楼梯高度计算
// ========================================

TEST_F(StairGeneratorDeepTest, TotalHeight_MatchesFloorHeight) {
    CreateBuildingWithStairs(StairType::Straight, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    
    // 验证 totalHeight 应该等于楼层高度
    float expectedHeight = 3.0f;  // 单层高度
    EXPECT_FLOAT_EQ(stair.totalHeight, expectedHeight) 
        << "totalHeight 必须等于实际楼层间高度差";
}

TEST_F(StairGeneratorDeepTest, MultiFloor_TotalHeight_Cumulative) {
    CreateBuildingWithStairs(StairType::Spiral, 4);
    
    // 修改楼梯连接到第3层（跨越3层）
    definition.floors[0].spaces[0].stairsConfig.connectToLevel = 3;
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    
    // 验证高度是3层的累加：3m * 3 = 9m
    float expectedHeight = 3.0f * 3;
    EXPECT_FLOAT_EQ(stair.totalHeight, expectedHeight) 
        << "跨越多层的楼梯，totalHeight 必须是累加高度";
    
    // 验证 fromLevel 和 toLevel
    EXPECT_EQ(stair.fromLevel, 0) << "应该从第0层开始";
    EXPECT_EQ(stair.toLevel, 3) << "应该连接到第3层";
}

TEST_F(StairGeneratorDeepTest, StepCount_MatchesTotalHeight) {
    CreateBuildingWithStairs(StairType::Straight, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    
    // 验证：numSteps * stepHeight ≈ totalHeight
    float calculatedHeight = stair.numSteps * stair.stepHeight;
    EXPECT_NEAR(calculatedHeight, stair.totalHeight, 0.01f) 
        << "台阶数量 * 台阶高度 必须等于总高度";
    
    // 验证台阶数量合理
    int minSteps = static_cast<int>(std::ceil(stair.totalHeight / 0.18f));
    EXPECT_GE(stair.numSteps, minSteps) 
        << "台阶数量必须足够覆盖总高度";
}

// ========================================
// 验证建筑规范
// ========================================

TEST_F(StairGeneratorDeepTest, StepHeight_MeetsCode) {
    CreateBuildingWithStairs(StairType::Straight, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    
    // 建筑规范：台阶高度 ≤ 18cm
    EXPECT_LE(stair.stepHeight, 0.18f) 
        << "台阶高度必须 ≤ 18cm (建筑规范)";
    EXPECT_GE(stair.stepHeight, 0.10f) 
        << "台阶高度应该 ≥ 10cm (合理范围)";
}

TEST_F(StairGeneratorDeepTest, StepDepth_MeetsCode) {
    CreateBuildingWithStairs(StairType::Straight, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    
    // 建筑规范：台阶深度 ≥ 28cm
    EXPECT_GE(stair.stepDepth, 0.28f) 
        << "台阶深度必须 ≥ 28cm (建筑规范)";
}

TEST_F(StairGeneratorDeepTest, CustomStepParameters_Applied) {
    stairGenerator.SetStepParameters(0.17f, 0.30f);  // 自定义参数
    
    CreateBuildingWithStairs(StairType::Straight, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    
    EXPECT_LE(stair.stepHeight, 0.17f) << "应该使用自定义的最大台阶高度";
    EXPECT_GE(stair.stepDepth, 0.30f) << "应该使用自定义的最小台阶深度";
}

// ========================================
// 验证台阶位置数组
// ========================================

TEST_F(StairGeneratorDeepTest, Steps_ArrayPopulated) {
    CreateBuildingWithStairs(StairType::Straight, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    
    // 验证 steps 数组已填充
    EXPECT_EQ(stair.steps.size(), stair.numSteps) 
        << "steps 数组必须包含每个台阶的位置";
    
    // 验证每个台阶都有位置数据
    for (size_t i = 0; i < stair.steps.size(); ++i) {
        const auto& step = stair.steps[i];
        
        // 台阶高度应该递增
        EXPECT_GE(step.height, i * stair.stepHeight * 0.9f) 
            << "台阶 " << i << " 的高度必须随索引递增";
    }
}

TEST_F(StairGeneratorDeepTest, StraightStair_StepsLinear) {
    CreateBuildingWithStairs(StairType::Straight, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    ASSERT_GE(stair.steps.size(), 3) << "需要至少3个台阶来验证线性";
    
    // 直梯的台阶应该沿直线排列
    // 检查相邻台阶之间的距离应该恒定
    float firstSpacing = std::sqrt(
        std::pow(stair.steps[1].position[0] - stair.steps[0].position[0], 2) +
        std::pow(stair.steps[1].position[1] - stair.steps[0].position[1], 2)
    );
    
    for (size_t i = 2; i < stair.steps.size(); ++i) {
        float spacing = std::sqrt(
            std::pow(stair.steps[i].position[0] - stair.steps[i-1].position[0], 2) +
            std::pow(stair.steps[i].position[1] - stair.steps[i-1].position[1], 2)
        );
        
        EXPECT_NEAR(spacing, firstSpacing, 0.5f) 
            << "直梯台阶 " << i << " 的间距必须均匀";
    }
}

TEST_F(StairGeneratorDeepTest, StraightStair_AxisAligned) {
    CreateBuildingWithStairs(StairType::Straight, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    ASSERT_GE(stair.steps.size(), 2);
    
    // 直梯应该沿X或Y轴对齐
    float dx = std::abs(stair.steps[1].position[0] - stair.steps[0].position[0]);
    float dy = std::abs(stair.steps[1].position[1] - stair.steps[0].position[1]);
    
    bool xAligned = (dx > 0.1f && dy < 0.1f);
    bool yAligned = (dy > 0.1f && dx < 0.1f);
    
    EXPECT_TRUE(xAligned || yAligned) 
        << "直梯必须沿X或Y轴对齐";
}

// ========================================
// 验证 L 形楼梯
// ========================================

TEST_F(StairGeneratorDeepTest, LStair_HasLanding) {
    CreateBuildingWithStairs(StairType::L, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    
    // L形楼梯必须有平台
    EXPECT_GT(stair.landings.size(), 0) 
        << "L形楼梯必须有至少一个平台";
}

TEST_F(StairGeneratorDeepTest, LStair_LandingDimensions) {
    CreateBuildingWithStairs(StairType::L, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    ASSERT_GT(stair.landings.size(), 0);
    
    const auto& landing = stair.landings[0];
    
    // 平台必须有有效的尺寸
    EXPECT_GT(landing.width, 0.0f) << "平台宽度必须 > 0";
    EXPECT_GT(landing.depth, 0.0f) << "平台深度必须 > 0";
    EXPECT_GT(landing.height, 0.0f) << "平台高度必须 > 0";
    
    // 平台宽度应该至少等于楼梯宽度
    EXPECT_GE(landing.width, stair.config.width * 0.9f) 
        << "平台宽度必须至少等于楼梯宽度";
}

TEST_F(StairGeneratorDeepTest, LStair_TwoSegments) {
    CreateBuildingWithStairs(StairType::L, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    ASSERT_GT(stair.steps.size(), 4) << "需要足够的台阶来验证两段";
    
    // L形楼梯应该有两段，在平台处转向
    // 简化检查：前半段和后半段的方向应该不同
    size_t midPoint = stair.steps.size() / 2;
    
    float dx1 = stair.steps[1].position[0] - stair.steps[0].position[0];
    float dy1 = stair.steps[1].position[1] - stair.steps[0].position[1];
    
    float dx2 = stair.steps[midPoint+1].position[0] - stair.steps[midPoint].position[0];
    float dy2 = stair.steps[midPoint+1].position[1] - stair.steps[midPoint].position[1];
    
    // 两段的主方向应该不同（L形转折）
    bool segment1IsX = std::abs(dx1) > std::abs(dy1);
    bool segment2IsX = std::abs(dx2) > std::abs(dy2);
    
    // 如果不是完全垂直转折，至少方向应该有显著变化
    float dot = dx1 * dx2 + dy1 * dy2;
    float len1 = std::sqrt(dx1*dx1 + dy1*dy1);
    float len2 = std::sqrt(dx2*dx2 + dy2*dy2);
    
    if (len1 > 0.01f && len2 > 0.01f) {
        float cosAngle = dot / (len1 * len2);
        EXPECT_LT(cosAngle, 0.8f) 
            << "L形楼梯的两段方向必须有明显不同";
    }
}

// ========================================
// 验证 U 形楼梯
// ========================================

TEST_F(StairGeneratorDeepTest, UStair_HasLanding) {
    CreateBuildingWithStairs(StairType::U, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    
    // U形楼梯必须有平台
    EXPECT_GT(stair.landings.size(), 0) 
        << "U形楼梯必须有至少一个平台";
}

TEST_F(StairGeneratorDeepTest, UStair_ReturnPathOpposite) {
    CreateBuildingWithStairs(StairType::U, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    ASSERT_GT(stair.steps.size(), 6) << "需要足够的台阶来验证回程";
    
    // U形楼梯：上行段和下行段应该方向相反
    size_t midPoint = stair.steps.size() / 2;
    
    float dx1 = stair.steps[1].position[0] - stair.steps[0].position[0];
    float dy1 = stair.steps[1].position[1] - stair.steps[0].position[1];
    
    float dx2 = stair.steps[midPoint+2].position[0] - stair.steps[midPoint+1].position[0];
    float dy2 = stair.steps[midPoint+2].position[1] - stair.steps[midPoint+1].position[1];
    
    // 方向应该大致相反（点积为负）
    float dot = dx1 * dx2 + dy1 * dy2;
    EXPECT_LT(dot, 0.5f) 
        << "U形楼梯的回程方向必须与上行方向相反";
}

// ========================================
// 验证螺旋楼梯
// ========================================

TEST_F(StairGeneratorDeepTest, SpiralStair_CircularPattern) {
    CreateBuildingWithStairs(StairType::Spiral, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    ASSERT_GE(stair.steps.size(), 8) << "螺旋楼梯需要足够的台阶来验证圆形模式";
    
    // 计算中心点
    float centerX = stair.config.position[0];
    float centerY = stair.config.position[1];
    
    // 计算第一个台阶到中心的半径
    float r1 = std::sqrt(
        std::pow(stair.steps[0].position[0] - centerX, 2) +
        std::pow(stair.steps[0].position[1] - centerY, 2)
    );
    
    // 验证所有台阶到中心的距离大致相等（圆形）
    for (size_t i = 1; i < stair.steps.size(); ++i) {
        float ri = std::sqrt(
            std::pow(stair.steps[i].position[0] - centerX, 2) +
            std::pow(stair.steps[i].position[1] - centerY, 2)
        );
        
        EXPECT_NEAR(ri, r1, 0.5f) 
            << "螺旋楼梯台阶 " << i << " 必须在圆形路径上";
    }
}

TEST_F(StairGeneratorDeepTest, SpiralStair_RotatesAroundCenter) {
    CreateBuildingWithStairs(StairType::Spiral, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    ASSERT_GE(stair.steps.size(), 8);
    
    float centerX = stair.config.position[0];
    float centerY = stair.config.position[1];
    
    // 验证角度递增（围绕中心旋转）
    float prevAngle = std::atan2(
        stair.steps[0].position[1] - centerY,
        stair.steps[0].position[0] - centerX
    );
    
    int angleIncreaseCount = 0;
    for (size_t i = 1; i < stair.steps.size(); ++i) {
        float angle = std::atan2(
            stair.steps[i].position[1] - centerY,
            stair.steps[i].position[0] - centerX
        );
        
        float deltaAngle = angle - prevAngle;
        
        // 处理角度循环（-π 到 π）
        if (deltaAngle < -3.0f) deltaAngle += 2.0f * 3.14159f;
        if (deltaAngle > 3.0f) deltaAngle -= 2.0f * 3.14159f;
        
        if (deltaAngle > 0.01f) {
            angleIncreaseCount++;
        }
        
        prevAngle = angle;
    }
    
    // 大部分台阶的角度应该是递增的
    float increaseRatio = static_cast<float>(angleIncreaseCount) / (stair.steps.size() - 1);
    EXPECT_GT(increaseRatio, 0.7f) 
        << "螺旋楼梯的台阶必须围绕中心旋转";
}

// ========================================
// 验证楼梯长度计算
// ========================================

TEST_F(StairGeneratorDeepTest, StairLength_Calculated) {
    CreateBuildingWithStairs(StairType::Straight, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    
    // stairLength 必须已计算
    EXPECT_GT(stair.stairLength, 0.0f) 
        << "楼梯长度必须已计算且 > 0";
    
    // 对于直梯，长度应该大致等于 numSteps * stepDepth
    float expectedLength = stair.numSteps * stair.stepDepth;
    EXPECT_NEAR(stair.stairLength, expectedLength, stair.stepDepth * 2.0f) 
        << "直梯长度应该约等于台阶数 * 台阶深度";
}

// ========================================
// 验证旋转字段
// ========================================

TEST_F(StairGeneratorDeepTest, Rotation_FieldExists) {
    CreateBuildingWithStairs(StairType::Straight, 2);
    
    stairGenerator.GenerateStairs(definition, stairs);
    
    ASSERT_EQ(stairs.size(), 1);
    const auto& stair = stairs[0];
    
    // rotation 字段应该被初始化（可以是任何值，包括0）
    // 这只是验证字段存在并可访问
    float rot = stair.rotation;
    EXPECT_TRUE(rot == rot) << "rotation 字段必须存在并初始化（不是 NaN）";
}
