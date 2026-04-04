#include <gtest/gtest.h>

#include "building/BuildingAuthoringReadiness.h"
#include "building/BuildingPipeline.h"
#include "building/BuildingQualityChecks.h"
#include "building/SemanticBuildingTypes.h"
#include "core/Assets/AssetPaths.h"

#include <fstream>
#include <sstream>
#include <vector>

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

Moon::Building::GeneratedBuilding BuildBuilding(const std::string& path) {
    const std::string json = ReadUtf8TextFile(path);
    EXPECT_FALSE(json.empty()) << path;

    Moon::Building::BuildingPipeline pipeline;
    Moon::Building::GeneratedBuilding building;
    std::string error;
    EXPECT_TRUE(pipeline.ProcessBuilding(json, building, error)) << error;
    return building;
}

} // namespace

TEST(BuildingWorkflowQualityCoverageTests, ResidentialLiteProgressionMovesThroughExpectedStages) {
    const std::string formJson = R"({
      "schema": "moon_building",
      "grid": 0.5,
      "building_type": "villa",
      "mass": {
        "footprint_area": 180,
        "floors": 2,
        "total_height": 7.2
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

    const std::string verticalJson = R"({
      "schema": "moon_building",
      "grid": 0.5,
      "building_type": "villa",
      "mass": {
        "footprint_area": 180,
        "floors": 2,
        "total_height": 7.2
      },
      "style": {
        "category": "",
        "facade": "",
        "roof": "",
        "window_style": "",
        "material": ""
      },
      "vertical_systems": [
        {
          "type": "stair",
          "id": "main_stair",
          "serve_floors": [0, 1],
          "preferred_zone": "center"
        }
      ],
      "program": {
        "floors": [
          { "level": 0, "spaces": [] },
          { "level": 1, "spaces": [] }
        ]
      }
    })";

    const std::string programJson = R"({
      "schema": "moon_building",
      "grid": 0.5,
      "building_type": "villa",
      "mass": {
        "footprint_area": 180,
        "floors": 2,
        "total_height": 7.2
      },
      "style": {
        "category": "",
        "facade": "",
        "roof": "",
        "window_style": "",
        "material": ""
      },
      "vertical_systems": [
        {
          "type": "stair",
          "id": "main_stair",
          "serve_floors": [0, 1],
          "preferred_zone": "center"
        }
      ],
      "program": {
        "floors": [
          {
            "level": 0,
            "spaces": [
              { "space_id": "living", "type": "living", "zone": "day", "area_preferred": 35 },
              { "space_id": "kitchen", "type": "kitchen", "zone": "day", "area_preferred": 18 }
            ]
          },
          {
            "level": 1,
            "spaces": [
              { "space_id": "bedroom_1", "type": "bedroom", "zone": "night", "area_preferred": 18 },
              { "space_id": "bathroom_1", "type": "bathroom", "zone": "service", "area_preferred": 8 }
            ]
          }
        ]
      }
    })";

    const std::string facadeJson = R"({
      "schema": "moon_building",
      "grid": 0.5,
      "building_type": "villa",
      "mass": {
        "footprint_area": 180,
        "floors": 2,
        "total_height": 7.2
      },
      "style": {
        "category": "modern_residential",
        "facade": "stucco_and_wood",
        "roof": "pitched",
        "window_style": "large_residential",
        "material": "stucco"
      },
      "vertical_systems": [
        {
          "type": "stair",
          "id": "main_stair",
          "serve_floors": [0, 1],
          "preferred_zone": "center"
        }
      ],
      "program": {
        "floors": [
          {
            "level": 0,
            "spaces": [
              { "space_id": "living", "type": "living", "zone": "day", "area_preferred": 35 },
              { "space_id": "kitchen", "type": "kitchen", "zone": "day", "area_preferred": 18 }
            ]
          },
          {
            "level": 1,
            "spaces": [
              { "space_id": "bedroom_1", "type": "bedroom", "zone": "night", "area_preferred": 18 },
              { "space_id": "bathroom_1", "type": "bathroom", "zone": "service", "area_preferred": 8 }
            ]
          }
        ]
      }
    })";

    {
        const auto report = Moon::Building::EvaluateAuthoringReadiness(ParseBuilding(formJson));
        EXPECT_EQ(report.workflowTemplate, Moon::Building::BuildingWorkflowTemplate::ResidentialLite);
        EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::Vertical);
    }

    {
        const auto report = Moon::Building::EvaluateAuthoringReadiness(ParseBuilding(verticalJson));
        EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Vertical));
        EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::Program);
    }

    {
        const auto report = Moon::Building::EvaluateAuthoringReadiness(ParseBuilding(programJson));
        EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Program));
        EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::Facade);
    }

    {
        const auto report = Moon::Building::EvaluateAuthoringReadiness(ParseBuilding(facadeJson));
        EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Facade));
        EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::SceneComposition);
    }
}

TEST(BuildingWorkflowQualityCoverageTests, OfficeTowerProgressionRequiresPlateBeforeProgram) {
    const std::string verticalJson = R"({
      "schema": "moon_building",
      "grid": 0.5,
      "building_type": "office_tower",
      "mass": {
        "footprint_area": 1450,
        "floors": 24,
        "total_height": 96
      },
      "style": {
        "category": "",
        "facade": "",
        "roof": "",
        "window_style": "",
        "material": ""
      },
      "vertical_systems": [
        {
          "type": "elevator",
          "id": "passenger_bank_a",
          "serve_floors": [0, 1, 2, 3, 4, 5, 6],
          "preferred_zone": "center"
        },
        {
          "type": "stair",
          "id": "egress_a",
          "serve_floors": [0, 1, 2, 3, 4, 5, 6],
          "preferred_zone": "edge"
        }
      ],
      "program": {
        "floors": [
          { "level": 0, "spaces": [] },
          { "level": 1, "spaces": [] }
        ]
      }
    })";

    const std::string plateJson = R"({
      "schema": "moon_building",
      "grid": 0.5,
      "building_type": "office_tower",
      "mass": {
        "footprint_area": 1450,
        "floors": 24,
        "total_height": 96
      },
      "style": {
        "category": "",
        "facade": "",
        "roof": "",
        "window_style": "",
        "material": ""
      },
      "vertical_systems": [
        {
          "type": "elevator",
          "id": "passenger_bank_a",
          "serve_floors": [0, 1, 2, 3, 4, 5, 6],
          "preferred_zone": "center"
        },
        {
          "type": "stair",
          "id": "egress_a",
          "serve_floors": [0, 1, 2, 3, 4, 5, 6],
          "preferred_zone": "edge"
        }
      ],
      "program": {
        "floors": [
          {
            "level": 0,
            "spaces": [
              { "space_id": "core_0", "type": "core", "zone": "service", "area_preferred": 110 }
            ]
          },
          {
            "level": 1,
            "spaces": [
              { "space_id": "void_1", "type": "void", "zone": "public", "area_preferred": 140 },
              { "space_id": "corridor_1", "type": "corridor", "zone": "circulation", "area_preferred": 90,
                "adjacency": [{ "to": "void_1", "relationship": "around" }] },
              { "space_id": "core_1", "type": "core", "zone": "service", "area_preferred": 110 }
            ]
          }
        ]
      }
    })";

    const std::string programJson = R"({
      "schema": "moon_building",
      "grid": 0.5,
      "building_type": "office_tower",
      "mass": {
        "footprint_area": 1450,
        "floors": 24,
        "total_height": 96
      },
      "style": {
        "category": "",
        "facade": "",
        "roof": "",
        "window_style": "",
        "material": ""
      },
      "vertical_systems": [
        {
          "type": "elevator",
          "id": "passenger_bank_a",
          "serve_floors": [0, 1, 2, 3, 4, 5, 6],
          "preferred_zone": "center"
        },
        {
          "type": "stair",
          "id": "egress_a",
          "serve_floors": [0, 1, 2, 3, 4, 5, 6],
          "preferred_zone": "edge"
        }
      ],
      "program": {
        "floors": [
          {
            "level": 0,
            "spaces": [
              { "space_id": "lobby", "type": "lobby", "zone": "public", "area_preferred": 220 },
              { "space_id": "retail", "type": "retail", "zone": "public", "area_preferred": 160 },
              { "space_id": "core_0", "type": "core", "zone": "service", "area_preferred": 110 }
            ]
          },
          {
            "level": 1,
            "spaces": [
              { "space_id": "void_1", "type": "void", "zone": "public", "area_preferred": 140 },
              { "space_id": "corridor_1", "type": "corridor", "zone": "circulation", "area_preferred": 90,
                "adjacency": [{ "to": "void_1", "relationship": "around" }] },
              { "space_id": "office_1", "type": "office", "zone": "work", "area_preferred": 480 },
              { "space_id": "meeting_1", "type": "meeting", "zone": "work", "area_preferred": 90 },
              { "space_id": "core_1", "type": "core", "zone": "service", "area_preferred": 110 }
            ]
          }
        ]
      }
    })";

    {
        const auto report = Moon::Building::EvaluateAuthoringReadiness(ParseBuilding(verticalJson));
        EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Vertical));
        EXPECT_FALSE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Plate));
        EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::Plate);
    }

    {
        const auto report = Moon::Building::EvaluateAuthoringReadiness(ParseBuilding(plateJson));
        EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Plate));
        EXPECT_FALSE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Program));
        EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::Program);
    }

    {
        const auto report = Moon::Building::EvaluateAuthoringReadiness(ParseBuilding(programJson));
        EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Program));
        EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::Facade);
    }
}

TEST(BuildingWorkflowQualityCoverageTests, RetailMallProgressionRequiresPlateSignalsBeforeProgram) {
    const std::string verticalJson = R"({
      "schema": "moon_building",
      "grid": 0.5,
      "building_type": "mall",
      "mass": {
        "footprint_area": 4800,
        "floors": 3,
        "total_height": 18
      },
      "style": {
        "category": "",
        "facade": "",
        "roof": "",
        "window_style": "",
        "material": ""
      },
      "vertical_systems": [
        {
          "type": "elevator",
          "id": "mall_elevator",
          "serve_floors": [0, 1, 2],
          "preferred_zone": "center"
        },
        {
          "type": "escalator",
          "id": "mall_escalator",
          "serve_floors": [0, 1, 2],
          "preferred_zone": "center"
        }
      ],
      "program": {
        "floors": [
          { "level": 0, "spaces": [] },
          { "level": 1, "spaces": [] },
          { "level": 2, "spaces": [] }
        ]
      }
    })";

    const std::string plateJson = R"({
      "schema": "moon_building",
      "grid": 0.5,
      "building_type": "mall",
      "mass": {
        "footprint_area": 4800,
        "floors": 3,
        "total_height": 18
      },
      "style": {
        "category": "",
        "facade": "",
        "roof": "",
        "window_style": "",
        "material": ""
      },
      "vertical_systems": [
        {
          "type": "elevator",
          "id": "mall_elevator",
          "serve_floors": [0, 1, 2],
          "preferred_zone": "center"
        },
        {
          "type": "escalator",
          "id": "mall_escalator",
          "serve_floors": [0, 1, 2],
          "preferred_zone": "center"
        }
      ],
      "program": {
        "floors": [
          {
            "level": 0,
            "spaces": [
              { "space_id": "atrium", "type": "void", "zone": "public", "area_preferred": 600 },
              { "space_id": "ring", "type": "corridor", "zone": "circulation", "area_preferred": 500,
                "adjacency": [{ "to": "atrium", "relationship": "around" }] }
            ]
          },
          {
            "level": 1,
            "spaces": [
              { "space_id": "atrium_1", "type": "void", "zone": "public", "area_preferred": 600 },
              { "space_id": "ring_1", "type": "corridor", "zone": "circulation", "area_preferred": 500,
                "adjacency": [{ "to": "atrium_1", "relationship": "around" }] }
            ]
          }
        ]
      }
    })";

    {
        const auto report = Moon::Building::EvaluateAuthoringReadiness(ParseBuilding(verticalJson));
        EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Vertical));
        EXPECT_FALSE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Plate));
        EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::Plate);
    }

    {
        const auto report = Moon::Building::EvaluateAuthoringReadiness(ParseBuilding(plateJson));
        EXPECT_TRUE(report.IsStageSatisfied(Moon::Building::BuildingAuthoringStage::Plate));
        EXPECT_EQ(report.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::Program);
    }
}

TEST(BuildingWorkflowQualityCoverageTests, RepresentativeWorkflowAssetsAreProductionReadyForVisualReview) {
    const std::vector<std::string> assetPaths = {
        Moon::Assets::BuildAssetPath("building/fixtures/villa.json"),
        Moon::Assets::BuildAssetPath("building/reference/midrise_apartment.json"),
        Moon::Assets::BuildAssetPath("building/fixtures/office_tower.json"),
        Moon::Assets::BuildAssetPath("building/fixtures/shopping_mall.json")
    };

    for (const std::string& assetPath : assetPaths) {
        SCOPED_TRACE(assetPath);

        const std::string json = ReadUtf8TextFile(assetPath);
        ASSERT_FALSE(json.empty());

        const auto readiness = Moon::Building::EvaluateAuthoringReadiness(ParseBuilding(json));
        EXPECT_EQ(readiness.nextSuggestedStage, Moon::Building::BuildingAuthoringStage::SceneComposition);

        const auto building = BuildBuilding(assetPath);
        const auto quality = Moon::Building::EvaluateBuildingQuality(building);
        EXPECT_TRUE(quality.passed);
        EXPECT_TRUE(quality.errors.empty());
    }
}
