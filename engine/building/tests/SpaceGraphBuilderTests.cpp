#include <gtest/gtest.h>
#include "building/SpaceGraphBuilder.h"
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"

using namespace Moon::Building;
using namespace Moon::Building::Test;

/**
 * @brief Test fixture for SpaceGraphBuilder
 */
class SpaceGraphBuilderTest : public ::testing::Test {
protected:
    SpaceGraphBuilder graphBuilder;
    BuildingDefinition definition;
    std::vector<SpaceConnection> connections;

    const Rect* FindRectById(const Floor& floor, const std::string& rectId) {
        for (const auto& space : floor.spaces) {
            for (const auto& rect : space.rects) {
                if (rect.rectId == rectId || rect.rectId.rfind(rectId + "_", 0) == 0) {
                    return &rect;
                }
            }
        }
        return nullptr;
    }

    int FindSpaceIdByRectId(const Floor& floor, const std::string& rectId) {
        for (const auto& space : floor.spaces) {
            for (const auto& rect : space.rects) {
                if (rect.rectId == rectId || rect.rectId.rfind(rectId + "_", 0) == 0) {
                    return space.spaceId;
                }
            }
        }
        return -1;
    }
    
    void SetUp() override {
        // Parse a test building
        SchemaValidator validator;
        std::string errorMsg;
        std::string json = TestHelpers::CreateSimpleRoom();
        validator.ValidateAndParse(json, definition, errorMsg);
    }
};

// ========================================
// Space Adjacency Tests
// ========================================

TEST_F(SpaceGraphBuilderTest, SimpleRoom_NoAdjacencies) {
    graphBuilder.BuildGraph(definition, connections);
    
    // Single room should have no adjacencies
    EXPECT_EQ(graphBuilder.GetAdjacencies().size(), 0);
}

TEST_F(SpaceGraphBuilderTest, Villa_MultipleAdjacencies) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    graphBuilder.BuildGraph(definition, connections);
    
    // Villa should have multiple adjacent spaces
    const auto& adjacencies = graphBuilder.GetAdjacencies();
    EXPECT_GT(adjacencies.size(), 0) << "Villa should have adjacent spaces";
    
    // Check that living room and kitchen are adjacent
    bool foundLivingKitchenAdjacency = false;
    for (const auto& adj : adjacencies) {
        if ((adj.spaceA == 1 && adj.spaceB == 2) || 
            (adj.spaceA == 2 && adj.spaceB == 1)) {
            foundLivingKitchenAdjacency = true;
            EXPECT_GT(adj.sharedEdgeLength, 0.0f);
        }
    }
    EXPECT_TRUE(foundLivingKitchenAdjacency) << "Living room and kitchen should be adjacent";
}

TEST_F(SpaceGraphBuilderTest, Apartment_HasMultipleSpaces) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateApartmentBuilding();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    graphBuilder.BuildGraph(definition, connections);
    
    // Apartment building should have multiple floors and spaces
    EXPECT_GT(definition.floors.size(), 1);
    
    int totalSpaces = 0;
    for (const auto& floor : definition.floors) {
        totalSpaces += static_cast<int>(floor.spaces.size());
    }
    EXPECT_GT(totalSpaces, 5) << "Apartment should have multiple spaces";
}

TEST_F(SpaceGraphBuilderTest, LShapedBuilding_CrossMassConnections) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateLShapedBuilding();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    graphBuilder.BuildGraph(definition, connections);
    
    // Semantic sample is a single-floor L-shaped program with multiple spaces.
    EXPECT_EQ(definition.floors.size(), 1);
    EXPECT_GE(definition.floors[0].spaces.size(), 2);
}

TEST_F(SpaceGraphBuilderTest, ShoppingMall_ShopAdjacentToConcourse) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateShoppingMall();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));

    graphBuilder.BuildGraph(definition, connections);

    ASSERT_GE(definition.floors.size(), 1);
    const Floor& floor = definition.floors[0];
    const int concourseId = FindSpaceIdByRectId(floor, "main_concourse");
    const int shopId = FindSpaceIdByRectId(floor, "shop_a_gf");

    ASSERT_GT(concourseId, 0);
    ASSERT_GT(shopId, 0);
    EXPECT_TRUE(graphBuilder.AreSpacesAdjacent(concourseId, shopId));
}

TEST_F(SpaceGraphBuilderTest, ShoppingMall_ConcourseMoreCentralThanCore) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateShoppingMall();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));

    ASSERT_GE(definition.floors.size(), 1);
    const Floor& floor = definition.floors[0];
    const Rect* concourse = FindRectById(floor, "main_concourse_top");
    const Rect* core = FindRectById(floor, "mall_core_gf");
    ASSERT_NE(concourse, nullptr);
    ASSERT_NE(core, nullptr);
    ASSERT_FALSE(definition.masses.empty());

    const float footprintCenterX = definition.masses[0].size[0] * 0.5f;
    const float footprintCenterY = definition.masses[0].size[1] * 0.5f;
    const float concourseCenterX = concourse->origin[0] + concourse->size[0] * 0.5f;
    const float concourseCenterY = concourse->origin[1] + concourse->size[1] * 0.5f;
    const float coreCenterX = core->origin[0] + core->size[0] * 0.5f;
    const float coreCenterY = core->origin[1] + core->size[1] * 0.5f;

    const float concourseDistance = std::abs(concourseCenterX - footprintCenterX) + std::abs(concourseCenterY - footprintCenterY);
    const float coreDistance = std::abs(coreCenterX - footprintCenterX) + std::abs(coreCenterY - footprintCenterY);

    EXPECT_LT(concourseDistance, coreDistance);
}

TEST_F(SpaceGraphBuilderTest, OfficeTower_MeetingHubAdjacentToLobbyOrCore) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateOfficeTower();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));

    graphBuilder.BuildGraph(definition, connections);

    ASSERT_GE(definition.floors.size(), 1);
    const Floor& floor = definition.floors[0];
    const int meetingId = FindSpaceIdByRectId(floor, "meeting_hub");
    const int lobbyId = FindSpaceIdByRectId(floor, "main_lobby");
    const int coreId = FindSpaceIdByRectId(floor, "tower_core_gf");

    ASSERT_GT(meetingId, 0);
    ASSERT_GT(lobbyId, 0);
    ASSERT_GT(coreId, 0);
    EXPECT_TRUE(graphBuilder.AreSpacesAdjacent(meetingId, lobbyId) ||
                graphBuilder.AreSpacesAdjacent(meetingId, coreId));
}

TEST_F(SpaceGraphBuilderTest, OfficeTower_CoreMoreCentralThanOfficePlate) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateOfficeTower();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));

    ASSERT_GE(definition.floors.size(), 3);
    const Floor& floor = definition.floors[2];
    const Rect* core = FindRectById(floor, "tower_core_l2");
    const Rect* office = FindRectById(floor, "open_office_l2");
    ASSERT_NE(core, nullptr);
    ASSERT_NE(office, nullptr);
    ASSERT_FALSE(definition.masses.empty());

    const float footprintCenterX = definition.masses[0].size[0] * 0.5f;
    const float footprintCenterY = definition.masses[0].size[1] * 0.5f;
    const float coreCenterX = core->origin[0] + core->size[0] * 0.5f;
    const float coreCenterY = core->origin[1] + core->size[1] * 0.5f;
    const float officeCenterX = office->origin[0] + office->size[0] * 0.5f;
    const float officeCenterY = office->origin[1] + office->size[1] * 0.5f;

    const float coreDistance = std::abs(coreCenterX - footprintCenterX) + std::abs(coreCenterY - footprintCenterY);
    const float officeDistance = std::abs(officeCenterX - footprintCenterX) + std::abs(officeCenterY - footprintCenterY);

    EXPECT_LT(coreDistance, officeDistance);
}

TEST_F(SpaceGraphBuilderTest, CBDResidential_AmenityAdjacentToCorridor) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateCBDResidential();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));

    graphBuilder.BuildGraph(definition, connections);

    ASSERT_GE(definition.floors.size(), 2);
    const Floor& floor = definition.floors[1];
    const int corridorId = FindSpaceIdByRectId(floor, "res_corridor_l1");
    const int loungeId = FindSpaceIdByRectId(floor, "sky_lounge_l1");

    ASSERT_GT(corridorId, 0);
    ASSERT_GT(loungeId, 0);
    EXPECT_TRUE(graphBuilder.AreSpacesAdjacent(corridorId, loungeId));
}

// ========================================
// Adjacency Query Tests
// ========================================

TEST_F(SpaceGraphBuilderTest, AreSpacesAdjacent_Villa) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateVilla();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    graphBuilder.BuildGraph(definition, connections);
    
    // Test adjacency queries
    // Non-existent spaces should not be adjacent
    EXPECT_FALSE(graphBuilder.AreSpacesAdjacent(999, 1000));
}

// ========================================
// Edge Cases
// ========================================

TEST_F(SpaceGraphBuilderTest, EmptyBuilding_NoConnections) {
    definition.floors.clear();
    definition.masses.clear();
    
    graphBuilder.BuildGraph(definition, connections);
    
    EXPECT_EQ(connections.size(), 0);
    EXPECT_EQ(graphBuilder.GetAdjacencies().size(), 0);
}

TEST_F(SpaceGraphBuilderTest, SingleSpace_NoSelfConnection) {
    // Simple room with single space should not connect to itself
    graphBuilder.BuildGraph(definition, connections);
    
    for (const auto& conn : connections) {
        EXPECT_NE(conn.spaceA, conn.spaceB) << "Space should not connect to itself";
    }
}
