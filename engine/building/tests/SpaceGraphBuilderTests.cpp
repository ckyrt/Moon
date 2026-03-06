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
        totalSpaces += floor.spaces.size();
    }
    EXPECT_GT(totalSpaces, 5) << "Apartment should have multiple spaces";
}

TEST_F(SpaceGraphBuilderTest, LShapedBuilding_CrossMassConnections) {
    SchemaValidator validator;
    std::string errorMsg;
    std::string json = TestHelpers::CreateLShapedBuilding();
    ASSERT_TRUE(validator.ValidateAndParse(json, definition, errorMsg));
    
    graphBuilder.BuildGraph(definition, connections);
    
    // L-shaped building has 2 spaces in different masses
    // They might or might not be adjacent depending on layout
    EXPECT_EQ(definition.floors.size(), 2);
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
