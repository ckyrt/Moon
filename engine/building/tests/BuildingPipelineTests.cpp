#include <gtest/gtest.h>
#include "building/BuildingPipeline.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"
#include <chrono>

using namespace Moon::Building;
using namespace Moon::Building::Test;

/**
 * @brief Integration test fixture for BuildingPipeline
 */
class BuildingPipelineTest : public ::testing::Test {
protected:
    BuildingPipeline pipeline;
    GeneratedBuilding building;
    std::string errorMsg;
};

// ========================================
// Integration Tests - Full Pipeline
// ========================================

TEST_F(BuildingPipelineTest, ProcessMinimalBuilding_Success) {
    std::string json = TestHelpers::CreateMinimalValidBuilding();
    
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.masses.size(), 1);
    EXPECT_EQ(building.definition.floors.size(), 1);
}

TEST_F(BuildingPipelineTest, ProcessSimpleRoom_Success) {
    std::string json = TestHelpers::CreateSimpleRoom();
    
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_TRUE(result) << "Error: " << errorMsg;
    ASSERT_EQ(building.definition.floors.size(), 1);
    EXPECT_EQ(building.definition.floors[0].spaces.size(), 1);
}

TEST_F(BuildingPipelineTest, ProcessMultiFloorBuilding_Success) {
    std::string json = TestHelpers::CreateMultiFloorBuilding();
    
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 2);
    
    // TODO: When generators are fully implemented, add checks for:
    // - Generated walls
    // - Generated doors
    // - Generated stairs
}

TEST_F(BuildingPipelineTest, ProcessOfficeTower_Success) {
    std::string json = TestHelpers::CreateOfficeTower();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 4);
    EXPECT_GT(building.walls.size(), 0);
}

TEST_F(BuildingPipelineTest, ProcessOfficeTower_PodiumTowerPattern_Success) {
    std::string json = TestHelpers::CreateOfficeTower();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_GT(building.definition.floors.size(), 3);
    EXPECT_GT(building.stairs.size(), 0);
}

TEST_F(BuildingPipelineTest, ProcessShoppingMall_Success) {
    std::string json = TestHelpers::CreateShoppingMall();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 2);
    EXPECT_GT(building.walls.size(), 0);
}

TEST_F(BuildingPipelineTest, ProcessShoppingMall_RingCorridorPattern_Success) {
    std::string json = TestHelpers::CreateShoppingMall();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_GT(building.definition.floors.size(), 1);
    EXPECT_GT(building.windows.size(), 0);
}

TEST_F(BuildingPipelineTest, ProcessCBDResidential_Success) {
    std::string json = TestHelpers::CreateCBDResidential();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 4);
    EXPECT_GT(building.stairs.size(), 0);
}

TEST_F(BuildingPipelineTest, ProcessCBDResidential_AmenityFloor_Success) {
    std::string json = TestHelpers::CreateCBDResidential();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 4);
    EXPECT_GT(building.walls.size(), 0);
}

TEST_F(BuildingPipelineTest, ProcessInvalidJSON_Fails) {
    std::string json = TestHelpers::CreateInvalidJSON_MissingGrid();
    
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(BuildingPipelineTest, ProcessBuildingBestEffort_RepairsMissingRootFields) {
        const std::string json = R"({
    "mass": {
        "footprint_area": 64,
        "floors": 1
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "spaces": [
                    {
                        "space_id": "living_1",
                        "type": "living",
                        "area_preferred": 20
                    }
                ]
            }
        ]
    }
})";

        bool strictResult = pipeline.ProcessBuilding(json, building, errorMsg);
        EXPECT_FALSE(strictResult);

        GeneratedBuilding bestEffortBuilding;
        BestEffortGenerationReport report;
        std::string bestEffortError;
        bool bestEffortResult = pipeline.ProcessBuildingBestEffort(json, bestEffortBuilding, report, bestEffortError);

        EXPECT_TRUE(bestEffortResult) << "Error: " << bestEffortError;
        EXPECT_TRUE(report.usedBestEffort);
        EXPECT_FALSE(report.repairNotes.empty());
        ASSERT_EQ(bestEffortBuilding.definition.floors.size(), 1);
        ASSERT_EQ(bestEffortBuilding.definition.floors[0].spaces.size(), 1);
        EXPECT_GT(bestEffortBuilding.walls.size(), 0);
}

TEST_F(BuildingPipelineTest, ProcessBuildingBestEffort_RecoversEmptyProgram) {
        const std::string json = R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "villa",
    "style": {
        "category": "minimal",
        "facade": "white",
        "roof": "flat",
        "window_style": "standard",
        "material": "plaster"
    },
    "mass": {
        "footprint_area": 64,
        "floors": 1,
        "total_height": 3
    },
    "program": {
        "floors": []
    }
})";

        bool strictResult = pipeline.ProcessBuilding(json, building, errorMsg);
        EXPECT_FALSE(strictResult);

        GeneratedBuilding bestEffortBuilding;
        BestEffortGenerationReport report;
        std::string bestEffortError;
        bool bestEffortResult = pipeline.ProcessBuildingBestEffort(json, bestEffortBuilding, report, bestEffortError);

        EXPECT_TRUE(bestEffortResult) << "Error: " << bestEffortError;
        EXPECT_TRUE(report.usedBestEffort);
        EXPECT_FALSE(report.repairNotes.empty());
        ASSERT_EQ(bestEffortBuilding.definition.floors.size(), 1);
        ASSERT_EQ(bestEffortBuilding.definition.floors[0].spaces.size(), 1);
        EXPECT_GT(bestEffortBuilding.walls.size(), 0);
}

TEST_F(BuildingPipelineTest, ProcessBuildingBestEffort_RepairsDuplicateSpaceIdsAndInvalidZone) {
        const std::string json = R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "villa",
    "style": {
        "category": "minimal",
        "facade": "white",
        "roof": "flat",
        "window_style": "standard",
        "material": "plaster"
    },
    "mass": {
        "footprint_area": 64,
        "floors": 1,
        "total_height": 3
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "spaces": [
                    {
                        "space_id": "dup_room",
                        "type": "living",
                        "zone": "bad_zone",
                        "area_preferred": 18
                    },
                    {
                        "space_id": "dup_room",
                        "type": "kitchen",
                        "zone": "bad_zone",
                        "area_preferred": 12
                    }
                ]
            }
        ]
    }
})";

        bool strictResult = pipeline.ProcessBuilding(json, building, errorMsg);
        EXPECT_FALSE(strictResult);
        EXPECT_FALSE(errorMsg.empty());

        GeneratedBuilding bestEffortBuilding;
        BestEffortGenerationReport report;
        std::string bestEffortError;
        bool bestEffortResult = pipeline.ProcessBuildingBestEffort(json, bestEffortBuilding, report, bestEffortError);

        EXPECT_TRUE(bestEffortResult) << "Error: " << bestEffortError;
        EXPECT_TRUE(report.usedBestEffort);
        EXPECT_FALSE(report.adjustedSpaces.empty());
        ASSERT_EQ(bestEffortBuilding.definition.floors.size(), 1);
        ASSERT_EQ(bestEffortBuilding.definition.floors[0].spaces.size(), 2);
        EXPECT_GT(bestEffortBuilding.walls.size(), 0);
}

TEST_F(BuildingPipelineTest, ProcessBuildingBestEffort_RepairsUnsupportedAdjacencyValues) {
        const std::string json = R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "villa",
    "style": {
        "category": "minimal",
        "facade": "white",
        "roof": "flat",
        "window_style": "standard",
        "material": "plaster"
    },
    "mass": {
        "footprint_area": 64,
        "floors": 1,
        "total_height": 3
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "spaces": [
                    {
                        "space_id": "room_a",
                        "type": "living",
                        "zone": "public",
                        "area_preferred": 18,
                        "adjacency": [
                            { "to": "room_b", "relationship": "teleport", "importance": "mandatory" }
                        ]
                    },
                    {
                        "space_id": "room_b",
                        "type": "kitchen",
                        "zone": "public",
                        "area_preferred": 12
                    }
                ]
            }
        ]
    }
})";

        bool strictResult = pipeline.ProcessBuilding(json, building, errorMsg);
        EXPECT_FALSE(strictResult);
        EXPECT_NE(errorMsg.find("Unsupported adjacency"), std::string::npos);

        GeneratedBuilding bestEffortBuilding;
        BestEffortGenerationReport report;
        std::string bestEffortError;
        bool bestEffortResult = pipeline.ProcessBuildingBestEffort(json, bestEffortBuilding, report, bestEffortError);

        EXPECT_TRUE(bestEffortResult) << "Error: " << bestEffortError;
        EXPECT_TRUE(report.usedBestEffort);
        EXPECT_FALSE(report.adjustedSpaces.empty());
        ASSERT_EQ(bestEffortBuilding.definition.floors.size(), 1);
        ASSERT_EQ(bestEffortBuilding.definition.floors[0].spaces.size(), 2);
}

TEST_F(BuildingPipelineTest, ProcessBuildingBestEffort_RepairsInvalidFloorLevels) {
        const std::string json = R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "villa",
    "style": {
        "category": "minimal",
        "facade": "white",
        "roof": "flat",
        "window_style": "standard",
        "material": "plaster"
    },
    "mass": {
        "footprint_area": 100,
        "floors": 2,
        "total_height": 6
    },
    "program": {
        "floors": [
            {
                "level": 3,
                "spaces": [
                    {
                        "space_id": "core_bad",
                        "type": "core",
                        "zone": "service",
                        "area_preferred": 20,
                        "constraints": {
                            "connects_to_floor": 9,
                            "min_width": 3.0,
                            "ceiling_height": 3.0
                        }
                    }
                ]
            },
            {
                "level": 3,
                "spaces": [
                    {
                        "space_id": "bedroom_bad",
                        "type": "bedroom",
                        "zone": "private",
                        "area_preferred": 24,
                        "constraints": {
                            "min_width": 3.0,
                            "ceiling_height": 3.0
                        }
                    }
                ]
            }
        ]
    }
})";

        bool strictResult = pipeline.ProcessBuilding(json, building, errorMsg);
        EXPECT_FALSE(strictResult);
        EXPECT_NE(errorMsg.find("Floor level"), std::string::npos);

        GeneratedBuilding bestEffortBuilding;
        BestEffortGenerationReport report;
        std::string bestEffortError;
        bool bestEffortResult = pipeline.ProcessBuildingBestEffort(json, bestEffortBuilding, report, bestEffortError);

        EXPECT_TRUE(bestEffortResult) << "Error: " << bestEffortError;
        EXPECT_TRUE(report.usedBestEffort);
        ASSERT_EQ(bestEffortBuilding.definition.floors.size(), 2);
        EXPECT_EQ(bestEffortBuilding.definition.floors[0].level, 0);
        EXPECT_EQ(bestEffortBuilding.definition.floors[1].level, 1);
}

TEST_F(BuildingPipelineTest, ProcessBuildingBestEffort_RepairsTooSmallResolvedSpace) {
        const std::string json = R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "villa",
    "style": {
        "category": "minimal",
        "facade": "white",
        "roof": "flat",
        "window_style": "standard",
        "material": "plaster"
    },
    "mass": {
        "footprint_area": 30,
        "floors": 1,
        "total_height": 3
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "name": "ground_floor",
                "spaces": [
                    {
                        "space_id": "tiny_entry",
                        "type": "entrance",
                        "zone": "circulation",
                        "area_preferred": 1,
                        "constraints": {
                            "min_width": 0.5,
                            "ceiling_height": 3.0
                        }
                    },
                    {
                        "space_id": "living_room",
                        "type": "living",
                        "zone": "public",
                        "area_preferred": 24,
                        "constraints": {
                            "min_width": 3.0,
                            "ceiling_height": 3.0
                        }
                    }
                ]
            }
        ]
    }
})";

        bool strictResult = pipeline.ProcessBuilding(json, building, errorMsg);
        EXPECT_FALSE(strictResult);
        EXPECT_NE(errorMsg.find("smaller than minimum size"), std::string::npos);

        GeneratedBuilding bestEffortBuilding;
        BestEffortGenerationReport report;
        std::string bestEffortError;
        bool bestEffortResult = pipeline.ProcessBuildingBestEffort(json, bestEffortBuilding, report, bestEffortError);

        EXPECT_TRUE(bestEffortResult) << "Error: " << bestEffortError;
        EXPECT_TRUE(report.usedBestEffort);
        const bool tinyEntryAdjusted = std::any_of(report.adjustedSpaces.begin(), report.adjustedSpaces.end(),
            [](const BestEffortAdjustedSpace& adjustedSpace) {
                return adjustedSpace.spaceId == "tiny_entry";
            });
        const bool tinyEntrySkipped = std::any_of(report.skippedSpaces.begin(), report.skippedSpaces.end(),
            [](const BestEffortSkippedSpace& skippedSpace) {
                return skippedSpace.spaceId == "tiny_entry";
            });

        EXPECT_TRUE(tinyEntryAdjusted || tinyEntrySkipped);
        ASSERT_EQ(bestEffortBuilding.definition.floors.size(), 1);
        ASSERT_GE(bestEffortBuilding.definition.floors[0].spaces.size(), 1);
        bool foundTinyEntry = false;
        for (const auto& space : bestEffortBuilding.definition.floors[0].spaces) {
            if (!space.rects.empty() && space.rects[0].rectId == "tiny_entry") {
                foundTinyEntry = true;
                EXPECT_GE(space.rects[0].size[0], 1.5f);
                EXPECT_GE(space.rects[0].size[1], 1.5f);
            }
        }
        if (tinyEntryAdjusted) {
            EXPECT_TRUE(foundTinyEntry);
        }
        EXPECT_GT(bestEffortBuilding.walls.size(), 0);
}

TEST_F(BuildingPipelineTest, ProcessBuildingBestEffort_RepairsInvalidStairTarget) {
        const std::string json = R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "villa",
    "style": {
        "category": "minimal",
        "facade": "white",
        "roof": "flat",
        "window_style": "standard",
        "material": "plaster"
    },
    "mass": {
        "footprint_area": 64,
        "floors": 1,
        "total_height": 3
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "name": "ground_floor",
                "spaces": [
                    {
                        "space_id": "bad_core",
                        "type": "core",
                        "zone": "service",
                        "area_preferred": 20,
                        "constraints": {
                            "connects_to_floor": 5,
                            "min_width": 3.0,
                            "ceiling_height": 3.0
                        }
                    }
                ]
            }
        ]
    }
})";

        bool strictResult = pipeline.ProcessBuilding(json, building, errorMsg);
        EXPECT_FALSE(strictResult);
        EXPECT_NE(errorMsg.find("connect to non-existent floor"), std::string::npos);

        GeneratedBuilding bestEffortBuilding;
        BestEffortGenerationReport report;
        std::string bestEffortError;
        bool bestEffortResult = pipeline.ProcessBuildingBestEffort(json, bestEffortBuilding, report, bestEffortError);

        EXPECT_TRUE(bestEffortResult) << "Error: " << bestEffortError;
        EXPECT_TRUE(report.usedBestEffort);
        EXPECT_FALSE(report.adjustedSpaces.empty());
        EXPECT_EQ(report.adjustedSpaces[0].spaceId, "bad_core");
        ASSERT_EQ(bestEffortBuilding.definition.floors.size(), 1);
        ASSERT_EQ(bestEffortBuilding.definition.floors[0].spaces.size(), 1);
        EXPECT_EQ(bestEffortBuilding.definition.floors[0].spaces[0].stairsConfig.connectToLevel, 0);
}

TEST_F(BuildingPipelineTest, ProcessBuildingBestEffort_SkipsOnlyInvalidSpace) {
        const std::string json = R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "villa",
    "style": {
        "category": "minimal",
        "facade": "white",
        "roof": "flat",
        "window_style": "standard",
        "material": "plaster"
    },
    "mass": {
        "footprint_area": 36,
        "floors": 1,
        "total_height": 3
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "name": "ground_floor",
                "spaces": [
                    {
                        "space_id": "oversized_room",
                        "type": "living",
                        "zone": "public",
                        "area_preferred": 120,
                        "area_min": 120,
                        "constraints": {
                            "min_width": 12.0,
                            "ceiling_height": 3.0
                        }
                    },
                    {
                        "space_id": "valid_room",
                        "type": "kitchen",
                        "zone": "public",
                        "area_preferred": 12,
                        "constraints": {
                            "min_width": 3.0,
                            "ceiling_height": 3.0
                        }
                    }
                ]
            }
        ]
    }
})";

        bool strictResult = pipeline.ProcessBuilding(json, building, errorMsg);
        EXPECT_FALSE(strictResult);

        GeneratedBuilding bestEffortBuilding;
        BestEffortGenerationReport report;
        std::string bestEffortError;
        bool bestEffortResult = pipeline.ProcessBuildingBestEffort(json, bestEffortBuilding, report, bestEffortError);

        EXPECT_TRUE(bestEffortResult) << "Error: " << bestEffortError;
        EXPECT_TRUE(report.usedBestEffort);
        ASSERT_GE(report.skippedSpaces.size(), 1);
        EXPECT_TRUE(std::any_of(report.skippedSpaces.begin(), report.skippedSpaces.end(),
            [](const BestEffortSkippedSpace& skippedSpace) {
                return skippedSpace.spaceId == "oversized_room";
            }));
        ASSERT_EQ(bestEffortBuilding.definition.floors.size(), 1);
        ASSERT_EQ(bestEffortBuilding.definition.floors[0].spaces.size(), 1);
        EXPECT_GT(bestEffortBuilding.walls.size(), 0);
}

// ========================================
// Validation-Only Tests
// ========================================

TEST_F(BuildingPipelineTest, ValidateOnly_ValidBuilding_Success) {
    std::string json = TestHelpers::CreateSimpleRoom();
    ValidationResult result;
    
    bool success = pipeline.ValidateOnly(json, result);
    
    EXPECT_TRUE(success);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.errors.size(), 0);
}

TEST_F(BuildingPipelineTest, ValidateOnly_InvalidBuilding_Fails) {
    std::string json = TestHelpers::CreateInvalidJSON_WrongGridSize();
    ValidationResult result;
    
    bool success = pipeline.ValidateOnly(json, result);
    
    EXPECT_FALSE(success);
    EXPECT_FALSE(result.valid);
    EXPECT_GT(result.errors.size(), 0);
}

// ========================================
// Error Handling Tests
// ========================================

TEST_F(BuildingPipelineTest, EmptyJSON_Fails) {
    std::string json = "";
    
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(errorMsg.empty());
}

TEST_F(BuildingPipelineTest, MalformedJSON_Fails) {
    std::string json = "{ not valid json at all }";
    
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    
    EXPECT_FALSE(result);
    EXPECT_NE(errorMsg.find("parse"), std::string::npos);
}

// ========================================
// Performance Tests (Optional)
// ========================================

TEST_F(BuildingPipelineTest, DISABLED_PerformanceTest_LargeBuilding) {
    // Create a large building with many rooms
    std::string json = R"({
      "schema": "moon_building_v8",
      "grid": 0.5,
      "style": {
        "category": "modern",
        "facade": "glass_white",
        "roof": "flat",
        "windowStyle": "standard",
        "material": "concrete_white"
      },
      "masses": [
        {
          "massId": "mass_1",
          "origin": [0.0, 0.0],
          "size": [100.0, 100.0],
          "floors": 10
        }
      ],
      "floors": []
    })";
    
    // Add floors programmatically would go here
    // For now, just test with basic structure
    
    auto start = std::chrono::high_resolution_clock::now();
    bool result = pipeline.ProcessBuilding(json, building, errorMsg);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_TRUE(result);
    std::cout << "Processing time: " << duration.count() << "ms" << std::endl;
}
