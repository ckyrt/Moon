#include <gtest/gtest.h>
#include "building/SchemaValidator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"
#include "../../external/nlohmann/json.hpp"

using namespace Moon::Building;
using namespace Moon::Building::Test;

/**
 * @brief Test fixture for SchemaValidator
 */
class SchemaValidatorTest : public ::testing::Test {
protected:
    SchemaValidator validator;
    BuildingDefinition definition;
    std::string errorMsg;
};

// ========================================
// Positive Tests - Valid Inputs
// ========================================

TEST_F(SchemaValidatorTest, ValidateMinimalBuilding_Success) {
    std::string json = TestHelpers::CreateMinimalValidBuilding();
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(definition.grid, 0.5f);
    EXPECT_EQ(definition.style.category, "modern");
    EXPECT_EQ(definition.masses.size(), 1);
    EXPECT_EQ(definition.floors.size(), 1);
}

TEST_F(SchemaValidatorTest, ValidateSimpleRoom_Success) {
    std::string json = TestHelpers::CreateSimpleRoom();
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_TRUE(result) << "Error: " << errorMsg;
    ASSERT_EQ(definition.floors.size(), 1);
    ASSERT_EQ(definition.floors[0].spaces.size(), 1);
    
    const Space& space = definition.floors[0].spaces[0];
    EXPECT_EQ(space.properties.usage, SpaceUsage::Living);
    EXPECT_GT(space.properties.ceilingHeight, 0.0f);
    EXPECT_LE(space.properties.ceilingHeight, definition.floors[0].floorHeight);
}

TEST_F(SchemaValidatorTest, ValidateMultiFloorBuilding_Success) {
    std::string json = TestHelpers::CreateMultiFloorBuilding();
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(definition.floors.size(), 2);
    
    // Check floor levels
    EXPECT_EQ(definition.floors[0].level, 0);
    EXPECT_EQ(definition.floors[1].level, 1);
    
    // Check stairs connection
    ASSERT_EQ(definition.floors[0].spaces.size(), 1);
    const Space& groundFloor = definition.floors[0].spaces[0];
    EXPECT_TRUE(groundFloor.properties.hasStairs);
    EXPECT_EQ(groundFloor.stairsConfig.connectToLevel, 1);
}

TEST_F(SchemaValidatorTest, ValidateOfficeTower_Success) {
    std::string json = TestHelpers::CreateOfficeTower();

    bool result = validator.ValidateAndParse(json, definition, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(definition.floors.size(), 4);
}

TEST_F(SchemaValidatorTest, ValidateOfficeTower_PodiumTowerPattern_Success) {
    std::string json = TestHelpers::CreateOfficeTower();

    bool result = validator.ValidateAndParse(json, definition, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(definition.floors.size(), 4);
}

TEST_F(SchemaValidatorTest, ValidateShoppingMall_Success) {
    std::string json = TestHelpers::CreateShoppingMall();

    bool result = validator.ValidateAndParse(json, definition, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(definition.floors.size(), 2);
}

TEST_F(SchemaValidatorTest, ValidateCBDResidential_Success) {
    std::string json = TestHelpers::CreateCBDResidential();

    bool result = validator.ValidateAndParse(json, definition, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(definition.floors.size(), 4);
}

TEST_F(SchemaValidatorTest, ValidateCBDResidential_AmenityFloor_Success) {
    std::string json = TestHelpers::CreateCBDResidential();

    bool result = validator.ValidateAndParse(json, definition, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(definition.floors.size(), 4);
}

// ========================================
// Negative Tests - Invalid Inputs
// ========================================

TEST_F(SchemaValidatorTest, InvalidJSON_MissingGrid_Fails) {
    std::string json = TestHelpers::CreateInvalidJSON_MissingGrid();
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(SchemaValidatorTest, InvalidJSON_WrongGridSize_Fails) {
    std::string json = TestHelpers::CreateInvalidJSON_WrongGridSize();
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(errorMsg.empty());
    EXPECT_NE(errorMsg.find("grid"), std::string::npos) << "Error should mention 'grid'";
}

TEST_F(SchemaValidatorTest, InvalidJSON_NonAlignedCoordinates_Fails) {
    std::string json = TestHelpers::CreateInvalidJSON_NonAlignedCoordinates();
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(SchemaValidatorTest, InvalidJSON_MalformedJSON_Fails) {
    std::string json = "{ this is not valid json }";
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_NE(errorMsg.find("parse"), std::string::npos);
}

TEST_F(SchemaValidatorTest, InvalidJSON_EmptyString_Fails) {
    std::string json = "";
    
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    
    EXPECT_FALSE(result);
}

TEST_F(SchemaValidatorTest, OfficeTower_WithoutGroundLobby_Fails) {
    nlohmann::json json = nlohmann::json::parse(TestHelpers::CreateOfficeTower());
    auto& spaces = json["program"]["floors"][0]["spaces"];
    for (auto it = spaces.begin(); it != spaces.end(); ++it) {
        if ((*it)["type"] == "lobby") {
            spaces.erase(it);
            break;
        }
    }

    bool result = validator.ValidateAndParse(json.dump(), definition, errorMsg);

    EXPECT_FALSE(result);
    EXPECT_NE(errorMsg.find("lobby"), std::string::npos);
}

TEST_F(SchemaValidatorTest, OfficeTower_RetailAbovePodium_Fails) {
    nlohmann::json json = nlohmann::json::parse(TestHelpers::CreateOfficeTower());
    json["program"]["floors"][3]["spaces"].push_back({
        {"space_id", "tower_shop_l3"},
        {"unit_id", "tower_shop_l3"},
        {"type", "shop"},
        {"zone", "public"},
        {"area_preferred", 24},
        {"constraints", {{"natural_light", "required"}, {"ceiling_height", 3.8}, {"min_width", 3.5}}}
    });

    bool result = validator.ValidateAndParse(json.dump(), definition, errorMsg);

    EXPECT_FALSE(result);
    EXPECT_NE(errorMsg.find("podium"), std::string::npos);
}

TEST_F(SchemaValidatorTest, ShoppingMall_WithoutVoid_Fails) {
    nlohmann::json json = nlohmann::json::parse(TestHelpers::CreateShoppingMall());
    for (auto& floor : json["program"]["floors"]) {
        auto& spaces = floor["spaces"];
        for (auto it = spaces.begin(); it != spaces.end();) {
            if ((*it)["type"] == "void") {
                it = spaces.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool result = validator.ValidateAndParse(json.dump(), definition, errorMsg);

    EXPECT_FALSE(result);
    EXPECT_NE(errorMsg.find("void"), std::string::npos);
}

TEST_F(SchemaValidatorTest, ShoppingMall_WithoutRingCorridor_Fails) {
    nlohmann::json json = nlohmann::json::parse(TestHelpers::CreateShoppingMall());
    for (auto& floor : json["program"]["floors"]) {
        for (auto& space : floor["spaces"]) {
            if (space["type"] == "corridor" && space.contains("adjacency")) {
                space["adjacency"] = nlohmann::json::array();
            }
        }
    }

    bool result = validator.ValidateAndParse(json.dump(), definition, errorMsg);

    EXPECT_FALSE(result);
    EXPECT_NE(errorMsg.find("around"), std::string::npos);
}

TEST_F(SchemaValidatorTest, ShoppingMall_ShopWithoutConcourseConnection_Fails) {
    nlohmann::json json = nlohmann::json::parse(TestHelpers::CreateShoppingMall());
    for (auto& floor : json["program"]["floors"]) {
        for (auto& space : floor["spaces"]) {
            if (space["type"] == "shop") {
                space["adjacency"] = nlohmann::json::array();
                break;
            }
        }
        break;
    }

    bool result = validator.ValidateAndParse(json.dump(), definition, errorMsg);

    EXPECT_FALSE(result);
    EXPECT_NE(errorMsg.find("shop"), std::string::npos);
}

TEST_F(SchemaValidatorTest, CBDResidential_WithoutAmenityFloor_Fails) {
    nlohmann::json json = nlohmann::json::parse(TestHelpers::CreateCBDResidential());
    auto& amenityFloorSpaces = json["program"]["floors"][1]["spaces"];
    for (auto& space : amenityFloorSpaces) {
        if (space["type"] == "living" || space["type"] == "office") {
            space["type"] = "storage";
            space["zone"] = "service";
        }
    }

    bool result = validator.ValidateAndParse(json.dump(), definition, errorMsg);

    EXPECT_FALSE(result);
    EXPECT_NE(errorMsg.find("amenity floor"), std::string::npos);
}

// ========================================
// Edge Cases
// ========================================

TEST_F(SchemaValidatorTest, GridAlignment_ValidValues) {
    std::string json = TestHelpers::CreateMinimalValidBuilding();
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    EXPECT_TRUE(result) << "Error: " << errorMsg;
}

TEST_F(SchemaValidatorTest, GridAlignment_InvalidValues) {
    std::string json = TestHelpers::CreateInvalidJSON_WrongGridSize();
    bool result = validator.ValidateAndParse(json, definition, errorMsg);
    EXPECT_FALSE(result);
}
