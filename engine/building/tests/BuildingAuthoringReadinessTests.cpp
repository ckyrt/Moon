#include <gtest/gtest.h>

#include "building/BuildingAuthoringReadiness.h"
#include "building/SemanticBuildingTypes.h"
#include "core/Assets/AssetPaths.h"

#include <fstream>
#include <sstream>

namespace {

std::string ReadUtf8TextFile(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }
    return content;
}

Moon::Building::SemanticBuilding ParseBuilding(const std::string& jsonText) {
    Moon::Building::SemanticBuilding building;
    std::string error;
    EXPECT_TRUE(Moon::Building::SemanticBuildingParser::ParseFromString(jsonText, building, error)) << error;
    return building;
}

} // namespace

TEST(BuildingAuthoringReadinessTests, FormOnlyResidentialAssetSuggestsProgramAfterVerticalForSingleFloorHouse) {
    const std::string json = R"({
      "schema": "moon_building",
      "grid": 0.5,
      "building_type": "villa",
      "mass": {
        "footprint_area": 120,
        "floors": 1,
        "total_height": 3.2
      },
      "style": {
        "category": "",
        "facade": "",
        "roof": "",
        "window_style": "",
        "material": ""
      },
      "program": {
        "floors": []
      }
    })";

    const Moon::Building::SemanticBuilding building = ParseBuilding(json);
    const auto report = Moon::Building::EvaluateAuthoringReadiness(building);

    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Form));
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Vertical));
    EXPECT_FALSE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Program));
    EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::Program);
}

TEST(BuildingAuthoringReadinessTests, MultiFloorResidentialWithoutCoreStillNeedsVerticalStage) {
    const std::string json = R"({
      "schema": "moon_building",
      "grid": 0.5,
      "building_type": "apartment",
      "mass": {
        "footprint_area": 220,
        "floors": 3,
        "total_height": 9.0
      },
      "style": {
        "category": "urban",
        "facade": "",
        "roof": "",
        "window_style": "",
        "material": ""
      },
      "program": {
        "floors": [
          {
            "level": 0,
            "spaces": [
              { "space_id": "lobby", "type": "lobby", "zone": "circulation", "area_preferred": 20 }
            ]
          }
        ]
      }
    })";

    const Moon::Building::SemanticBuilding building = ParseBuilding(json);
    const auto report = Moon::Building::EvaluateAuthoringReadiness(building);

    EXPECT_FALSE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Vertical));
    EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::Vertical);
}

TEST(BuildingAuthoringReadinessTests, RepresentativeVillaAssetCompletesBuildingStages) {
    const std::string json = ReadUtf8TextFile(Moon::Assets::BuildAssetPath("building/fixtures/villa.json"));
    ASSERT_FALSE(json.empty());

    const Moon::Building::SemanticBuilding building = ParseBuilding(json);
    const auto report = Moon::Building::EvaluateAuthoringReadiness(building);

    EXPECT_EQ(report.workflowTemplate, Moon::Building::BuildingWorkflowTemplate::ResidentialLite);
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Form));
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Vertical));
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Program));
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Facade));
    EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::SceneComposition);
}

TEST(BuildingAuthoringReadinessTests, RepresentativeOfficeAssetCompletesPlateStage) {
    const std::string json = ReadUtf8TextFile(Moon::Assets::BuildAssetPath("building/fixtures/office_tower.json"));
    ASSERT_FALSE(json.empty());

    const Moon::Building::SemanticBuilding building = ParseBuilding(json);
    const auto report = Moon::Building::EvaluateAuthoringReadiness(building);

    EXPECT_EQ(report.workflowTemplate, Moon::Building::BuildingWorkflowTemplate::OfficeTower);
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Vertical));
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Plate));
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Program));
}

TEST(BuildingAuthoringReadinessTests, RepresentativeMallAssetCompletesPlateStage) {
    const std::string json = ReadUtf8TextFile(Moon::Assets::BuildAssetPath("building/fixtures/shopping_mall.json"));
    ASSERT_FALSE(json.empty());

    const Moon::Building::SemanticBuilding building = ParseBuilding(json);
    const auto report = Moon::Building::EvaluateAuthoringReadiness(building);

    EXPECT_EQ(report.workflowTemplate, Moon::Building::BuildingWorkflowTemplate::RetailMall);
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Plate));
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Program));
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Facade));
}

TEST(BuildingAuthoringReadinessTests, MidriseApartmentAssetCompletesResidentialMidriseStages) {
    const std::string json = ReadUtf8TextFile(Moon::Assets::BuildAssetPath("building/reference/midrise_apartment.json"));
    ASSERT_FALSE(json.empty());

    const Moon::Building::SemanticBuilding building = ParseBuilding(json);
    const auto report = Moon::Building::EvaluateAuthoringReadiness(building);

    EXPECT_EQ(report.workflowTemplate, Moon::Building::BuildingWorkflowTemplate::ResidentialMidrise);
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Vertical));
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Program));
    EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Facade));
    EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::SceneComposition);
}
