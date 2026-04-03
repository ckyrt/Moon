#include <gtest/gtest.h>
#include "building/BuildingPipeline.h"
#include "building/BuildingGenerationInputs.h"
#include "building/LayoutResolver.h"
#include "building/MassFloorPlateGenerator.h"
#include "building/OfficeFloorLayoutSolver.h"
#include "building/StructuralPlanGenerator.h"
#include "building/BuildingTypes.h"
#include "TestHelpers.h"
#include <array>
#include <chrono>
#include <iostream>
#include <limits>

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

namespace {

bool RectContainsPoint(const Rect& rect, const GridPos2D& point) {
    return point[0] > rect.origin[0] + 0.001f &&
           point[0] < rect.origin[0] + rect.size[0] - 0.001f &&
           point[1] > rect.origin[1] + 0.001f &&
           point[1] < rect.origin[1] + rect.size[1] - 0.001f;
}

bool PointInOutline(const std::vector<GridPos2D>& outline, const GridPos2D& point) {
    if (outline.size() < 3) {
        return false;
    }

    bool inside = false;
    for (size_t i = 0, j = outline.size() - 1; i < outline.size(); j = i++) {
        const auto& a = outline[i];
        const auto& b = outline[j];
        const bool intersects = ((a[1] > point[1]) != (b[1] > point[1])) &&
            (point[0] < (b[0] - a[0]) * (point[1] - a[1]) / ((b[1] - a[1]) + 0.000001f) + a[0]);
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

bool RectFitsPlate(const FloorPlate& plate, const Rect& rect, float margin) {
    const std::array<GridPos2D, 5> samplePoints = {{
        {rect.origin[0] + margin, rect.origin[1] + margin},
        {rect.origin[0] + rect.size[0] - margin, rect.origin[1] + margin},
        {rect.origin[0] + rect.size[0] - margin, rect.origin[1] + rect.size[1] - margin},
        {rect.origin[0] + margin, rect.origin[1] + rect.size[1] - margin},
        {rect.origin[0] + rect.size[0] * 0.5f, rect.origin[1] + rect.size[1] * 0.5f}
    }};

    for (const auto& point : samplePoints) {
        for (const auto& voidRect : plate.voids) {
            if (RectContainsPoint(voidRect, point)) {
                return false;
            }
        }

        if (plate.outline.size() >= 3) {
            if (!PointInOutline(plate.outline, point)) {
                return false;
            }
        } else {
            const bool insideBounds =
                point[0] > plate.origin[0] + margin &&
                point[0] < plate.origin[0] + plate.size[0] - margin &&
                point[1] > plate.origin[1] + margin &&
                point[1] < plate.origin[1] + plate.size[1] - margin;
            if (!insideBounds) {
                return false;
            }
        }
    }

    return true;
}

const FloorPlate* FindFloorPlate(const GeneratedBuilding& building, int floorLevel) {
    for (const auto& plate : building.floorPlates) {
        if (plate.floorLevel == floorLevel) {
            return &plate;
        }
    }
    return nullptr;
}

int CountSpacesByUsage(const GeneratedBuilding& building, SpaceUsage usage) {
    int count = 0;
    for (const auto& floor : building.definition.floors) {
        for (const auto& space : floor.spaces) {
            if (space.properties.usage == usage) {
                ++count;
            }
        }
    }
    return count;
}

int CountSpacesOnFloorByUsage(const GeneratedBuilding& building, int floorLevel, SpaceUsage usage) {
    for (const auto& floor : building.definition.floors) {
        if (floor.level != floorLevel) {
            continue;
        }

        int count = 0;
        for (const auto& space : floor.spaces) {
            if (space.properties.usage == usage) {
                ++count;
            }
        }
        return count;
    }
    return 0;
}

int CountWindowsOnFloor(const GeneratedBuilding& building, int floorLevel) {
    return static_cast<int>(std::count_if(
        building.windows.begin(),
        building.windows.end(),
        [&](const Window& window) { return window.floorLevel == floorLevel; }));
}

int CountVerticalCoresByType(const GeneratedBuilding& building, VerticalCoreType type) {
    return static_cast<int>(std::count_if(
        building.verticalCores.begin(),
        building.verticalCores.end(),
        [&](const VerticalCore& core) { return core.type == type; }));
}

int CountWallsByType(const GeneratedBuilding& building, WallType type) {
    return static_cast<int>(std::count_if(
        building.walls.begin(),
        building.walls.end(),
        [&](const WallSegment& wall) { return wall.type == type; }));
}

float AverageAreaForType(const SemanticBuilding& building, const std::string& spaceType) {
    float total = 0.0f;
    int count = 0;
    for (const auto& floor : building.floors) {
        for (const auto& space : floor.spaces) {
            if (space.type == spaceType) {
                total += space.areaPreferred;
                ++count;
            }
        }
    }
    return count > 0 ? total / static_cast<float>(count) : 0.0f;
}

int CountSemanticSpaces(const SemanticBuilding& building, const std::string& spaceType) {
    int count = 0;
    for (const auto& floor : building.floors) {
        for (const auto& space : floor.spaces) {
            if (space.type == spaceType) {
                ++count;
            }
        }
    }
    return count;
}

std::vector<VerticalCore> CollectFloorCores(const std::vector<VerticalCore>& cores, int floorLevel) {
    std::vector<VerticalCore> result;
    for (const auto& core : cores) {
        if (floorLevel >= core.floorFrom && floorLevel <= core.floorTo) {
            result.push_back(core);
        }
    }
    return result;
}

void ExpectReferenceDatasetLooksPlausible(const std::string& json,
                                          const std::string& expectedType,
                                          int minFloors,
                                          int maxFloors) {
    SemanticBuilding semantic;
    std::string parseError;
    ASSERT_TRUE(SemanticBuildingParser::ParseFromString(json, semantic, parseError))
        << "Semantic parse failed: " << parseError;

    EXPECT_EQ(semantic.buildingType, expectedType);
    EXPECT_GE(static_cast<int>(semantic.floors.size()), minFloors);
    EXPECT_LE(static_cast<int>(semantic.floors.size()), maxFloors);
    EXPECT_GT(semantic.mass.footprintArea, 60.0f);

    for (const auto& floor : semantic.floors) {
        EXPECT_FALSE(floor.spaces.empty()) << "Each reference floor should declare explicit program";
        for (const auto& space : floor.spaces) {
            if (space.type == "terrace" || space.type == "balcony" || space.type == "void") {
                continue;
            }
            EXPECT_GE(space.constraints.ceilingHeight, 2.6f);
            EXPECT_LE(space.constraints.ceilingHeight, 6.0f);
            EXPECT_GE(space.constraints.minWidth, 1.5f);
            EXPECT_GT(space.areaPreferred, 3.0f);
        }
    }
}

void ExpectAllSpacesInsideFloorPlates(const GeneratedBuilding& building) {
    for (const auto& floor : building.definition.floors) {
        const FloorPlate* plate = FindFloorPlate(building, floor.level);
        ASSERT_NE(plate, nullptr) << "Missing floor plate for level " << floor.level;

        for (const auto& space : floor.spaces) {
            for (const auto& rect : space.rects) {
                EXPECT_TRUE(RectFitsPlate(*plate, rect, 0.1f))
                    << "Space rect '" << rect.rectId << "' escaped floor plate on level " << floor.level;
            }
        }
    }
}

} // namespace

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

TEST_F(BuildingPipelineTest, ProcessComplexShoppingMall_GeneratesStructuralSkeleton) {
    std::string json = TestHelpers::CreateComplexShoppingMall();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 3);
    EXPECT_EQ(building.floorPlates.size(), 3);
    EXPECT_GT(building.verticalCores.size(), 0);
    EXPECT_GT(building.supportColumns.size(), 0);
}

TEST_F(BuildingPipelineTest, ProcessOfficeTower_WithMassingRule_GeneratesVariableFloorPlates) {
    const std::string json = R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "office_tower",
    "style": {
        "category": "commercial",
        "facade": "glass",
        "roof": "flat",
        "window_style": "full_height",
        "material": "glass_metal"
    },
    "mass": {
        "footprint_area": 900,
        "floors": 8,
        "total_height": 34.0,
      "massing_rule": "planned_vase_midrise_office.json"
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "name": "podium_arrival",
                "spaces": [
                    {"space_id": "core_0", "type": "core", "zone": "service", "area_preferred": 80, "constraints": {"connects_to_floor": 1, "ceiling_height": 6.0, "min_width": 5.0}},
                    {"space_id": "lobby_0", "type": "lobby", "zone": "circulation", "area_preferred": 120, "constraints": {"ceiling_height": 6.0, "min_width": 7.0}},
                    {"space_id": "retail_0", "type": "shop", "zone": "public", "area_preferred": 90, "constraints": {"ceiling_height": 5.5, "min_width": 6.0}}
                ]
            },
            {
                "level": 1,
                "name": "podium_meeting",
                "spaces": [
                    {"space_id": "core_1", "type": "core", "zone": "service", "area_preferred": 72, "constraints": {"connects_from_floor": 0, "connects_to_floor": 2, "ceiling_height": 5.5, "min_width": 4.5}},
                    {"space_id": "meeting_1", "type": "meeting_room", "zone": "public", "area_preferred": 120, "constraints": {"ceiling_height": 5.0, "min_width": 6.0}},
                    {"space_id": "corridor_1", "type": "corridor", "zone": "circulation", "area_preferred": 60, "constraints": {"ceiling_height": 5.0, "min_width": 3.0}}
                ]
            },
            {
                "level": 2,
                "name": "office_2",
                "spaces": [
                    {"space_id": "core_2", "type": "core", "zone": "service", "area_preferred": 70, "constraints": {"connects_from_floor": 1, "connects_to_floor": 3, "ceiling_height": 4.2, "min_width": 4.5}},
                    {"space_id": "corridor_2", "type": "corridor", "zone": "circulation", "area_preferred": 52, "constraints": {"ceiling_height": 4.0, "min_width": 3.0}},
                    {"space_id": "office_2_main", "type": "office", "zone": "public", "area_preferred": 220, "constraints": {"ceiling_height": 4.0, "min_width": 8.0}}
                ]
            },
            {
                "level": 3,
                "name": "office_3",
                "spaces": [
                    {"space_id": "core_3", "type": "core", "zone": "service", "area_preferred": 68, "constraints": {"connects_from_floor": 2, "connects_to_floor": 4, "ceiling_height": 4.0, "min_width": 4.5}},
                    {"space_id": "corridor_3", "type": "corridor", "zone": "circulation", "area_preferred": 52, "constraints": {"ceiling_height": 4.0, "min_width": 3.0}},
                    {"space_id": "office_3_main", "type": "office", "zone": "public", "area_preferred": 220, "constraints": {"ceiling_height": 4.0, "min_width": 8.0}}
                ]
            },
            {
                "level": 4,
                "name": "office_4",
                "spaces": [
                    {"space_id": "core_4", "type": "core", "zone": "service", "area_preferred": 66, "constraints": {"connects_from_floor": 3, "connects_to_floor": 5, "ceiling_height": 4.0, "min_width": 4.5}},
                    {"space_id": "corridor_4", "type": "corridor", "zone": "circulation", "area_preferred": 52, "constraints": {"ceiling_height": 4.0, "min_width": 3.0}},
                    {"space_id": "office_4_main", "type": "office", "zone": "public", "area_preferred": 210, "constraints": {"ceiling_height": 4.0, "min_width": 8.0}}
                ]
            },
            {
                "level": 5,
                "name": "office_5",
                "spaces": [
                    {"space_id": "core_5", "type": "core", "zone": "service", "area_preferred": 64, "constraints": {"connects_from_floor": 4, "connects_to_floor": 6, "ceiling_height": 4.0, "min_width": 4.5}},
                    {"space_id": "corridor_5", "type": "corridor", "zone": "circulation", "area_preferred": 50, "constraints": {"ceiling_height": 4.0, "min_width": 3.0}},
                    {"space_id": "office_5_main", "type": "office", "zone": "public", "area_preferred": 200, "constraints": {"ceiling_height": 4.0, "min_width": 8.0}}
                ]
            },
            {
                "level": 6,
                "name": "office_6",
                "spaces": [
                    {"space_id": "core_6", "type": "core", "zone": "service", "area_preferred": 62, "constraints": {"connects_from_floor": 5, "connects_to_floor": 7, "ceiling_height": 4.0, "min_width": 4.0}},
                    {"space_id": "corridor_6", "type": "corridor", "zone": "circulation", "area_preferred": 48, "constraints": {"ceiling_height": 4.0, "min_width": 3.0}},
                    {"space_id": "office_6_main", "type": "office", "zone": "public", "area_preferred": 180, "constraints": {"ceiling_height": 4.0, "min_width": 7.5}}
                ]
            },
            {
                "level": 7,
                "name": "office_7",
                "spaces": [
                    {"space_id": "core_7", "type": "core", "zone": "service", "area_preferred": 60, "constraints": {"connects_from_floor": 6, "ceiling_height": 4.0, "min_width": 4.0}},
                    {"space_id": "corridor_7", "type": "corridor", "zone": "circulation", "area_preferred": 44, "constraints": {"ceiling_height": 4.0, "min_width": 3.0}},
                    {"space_id": "office_7_main", "type": "office", "zone": "public", "area_preferred": 165, "constraints": {"ceiling_height": 4.0, "min_width": 7.0}}
                ]
            }
        ]
    }
})";

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    ASSERT_EQ(building.floorPlates.size(), 8);

    float minArea = std::numeric_limits<float>::max();
    float maxArea = 0.0f;
    for (const auto& plate : building.floorPlates) {
        const float area = plate.size[0] * plate.size[1];
        minArea = std::min(minArea, area);
        maxArea = std::max(maxArea, area);
    }

    EXPECT_GT(maxArea - minArea, 20.0f);
    EXPECT_GT(building.verticalCores.size(), 0);
}

TEST_F(BuildingPipelineTest, ProcessOfficeTower_WithMassingRule_GeneratesMassDrivenSemanticLayout) {
    const std::string json = TestHelpers::LoadFromFile("massing_vase_office_demo.json");
    ASSERT_FALSE(json.empty());

    const bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    EXPECT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.floorPlates.size(), 8);
    EXPECT_GE(building.verticalCores.size(), 3);
    EXPECT_GT(building.programBlocks.size(), 12);
    EXPECT_FALSE(building.resolvedLayoutJson.empty());
    EXPECT_GT(building.walls.size(), 0);
    EXPECT_GT(building.connections.size(), 0);

    int officeSpaceCount = 0;
    int corridorSpaceCount = 0;
    for (const auto& floor : building.definition.floors) {
        EXPECT_FALSE(floor.spaces.empty());
        for (const auto& space : floor.spaces) {
            if (space.properties.usage == SpaceUsage::Office) {
                ++officeSpaceCount;
            }
            if (space.properties.usage == SpaceUsage::Corridor ||
                space.properties.usage == SpaceUsage::Entrance) {
                ++corridorSpaceCount;
            }
        }
    }

    EXPECT_GE(officeSpaceCount, 8);
    EXPECT_GT(corridorSpaceCount, 4);

    for (const auto& floor : building.definition.floors) {
        EXPECT_LT(floor.floorHeight, 8.0f)
            << "Resolved floor heights should follow per-floor program intent instead of raw total_height averaging";
    }

    EXPECT_LT(building.supportColumns.size(), 24u)
        << "Mass-driven towers should not emit a dense full-plate interior column grid";
}

TEST_F(BuildingPipelineTest, ProcessOfficeTower_WithMassingRule_GeneratesCoreAndOfficeFloorsFromFormalData) {
    const std::string json = TestHelpers::LoadFromFile("massing_vase_office_demo.json");
    ASSERT_FALSE(json.empty());

    SemanticBuilding semantic;
    ASSERT_TRUE(SemanticBuildingParser::ParseFromString(json, semantic, errorMsg))
        << "Semantic parse failed: " << errorMsg;

    LayoutResolver resolver;
    BuildingDefinition definition;
    ASSERT_TRUE(resolver.Resolve(semantic, definition, errorMsg))
        << "Layout resolve failed: " << errorMsg;

    MassFloorPlateGenerator plateGenerator;
    std::vector<FloorPlate> slicedFloorPlates;
    ASSERT_TRUE(plateGenerator.Generate(definition, slicedFloorPlates, errorMsg))
        << "Mass floor plate generation failed: " << errorMsg;
    ASSERT_EQ(slicedFloorPlates.size(), 8u);

    StructuralPlanGenerator structuralGenerator;
    GeneratedBuilding generated;
    ASSERT_TRUE(structuralGenerator.Generate(definition, slicedFloorPlates, generated, errorMsg))
        << "Structural plan generation failed: " << errorMsg;
    EXPECT_EQ(definition.style.category, "commercial");
    EXPECT_EQ(definition.floors.size(), 8u);
    EXPECT_EQ(generated.supportColumns.size(), 0u);
    std::ostringstream plateSummary;
    for (const auto& plate : generated.floorPlates) {
        plateSummary << "[L" << plate.floorLevel
                     << " bbox=" << plate.size[0] << "x" << plate.size[1]
                     << " outline=" << plate.outline.size()
                     << " envelope=" << plate.envelopeOutline.size()
                     << "] ";
    }
    ASSERT_FALSE(generated.verticalCores.empty()) << plateSummary.str();

    for (const auto& core : generated.verticalCores) {
        for (const auto& plate : generated.floorPlates) {
            if (plate.floorLevel < core.floorFrom || plate.floorLevel > core.floorTo) {
                continue;
            }
            EXPECT_TRUE(RectFitsPlate(plate, core.rect, 0.1f))
                << "Core '" << core.coreId << "' escapes floor plate on level " << plate.floorLevel;
        }
    }

    const BuildingLayoutInput layoutInput = ExtractBuildingLayoutInput(semantic);
    OfficeFloorLayoutSolver solver;
    for (const auto& floor : layoutInput.floors) {
        auto plateIt = std::find_if(generated.floorPlates.begin(), generated.floorPlates.end(),
            [&](const FloorPlate& plate) { return plate.floorLevel == floor.level; });
        ASSERT_NE(plateIt, generated.floorPlates.end()) << "Missing floor plate for level " << floor.level;

        ResolvedFloorLayout resolvedFloor;
        std::string floorError;
        const auto floorCores = CollectFloorCores(generated.verticalCores, floor.level);
        EXPECT_TRUE(solver.GenerateFloor(definition, floor, *plateIt, floorCores, resolvedFloor, floorError))
            << "Office solver failed on level " << floor.level << ": " << floorError;
        EXPECT_FALSE(resolvedFloor.spaces.empty())
            << "Office solver emitted no spaces on level " << floor.level;
    }
}

TEST_F(BuildingPipelineTest, ProcessOfficeTower_WithMassingRule_AllResolvedSpacesStayInsideFloorPlates) {
    const std::string json = TestHelpers::LoadFromFile("massing_vase_office_demo.json");
    ASSERT_FALSE(json.empty());

    const bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    ASSERT_FALSE(building.floorPlates.empty());
    ExpectAllSpacesInsideFloorPlates(building);
}

TEST_F(BuildingPipelineTest, ProcessOfficeTower_WithMassingRule_StoresEnvelopeGeometryInGeneratedBuilding) {
    const std::string json = TestHelpers::LoadFromFile("massing_vase_office_demo.json");
    ASSERT_FALSE(json.empty());

    const bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_FALSE(building.envelopeMeshes.empty());
    EXPECT_TRUE(std::all_of(
        building.floorPlates.begin(),
        building.floorPlates.end(),
        [](const FloorPlate& plate) {
            return plate.envelopeOutline.size() >= 3;
        }));
}

TEST_F(BuildingPipelineTest, ProcessBuilding_WithConcaveMassingRule_PreservesConcaveFloorPlateOutline) {
    const std::string json = R"({
    "schema": "moon_building",
    "grid": 0.5,
    "building_type": "villa",
    "style": {
        "category": "minimal",
        "facade": "white",
        "roof": "flat",
        "window_style": "standard",
        "material": "concrete",
        "facade_offset": 0.5
    },
    "mass": {
        "footprint_area": 40,
        "floors": 1,
        "total_height": 3.0,
        "massing_rule": "test_concave_l_shape.json"
    },
    "program": {
        "floors": [
            {
                "level": 0,
                "name": "ground_floor",
                "spaces": [
                    {"space_id": "living_0", "type": "living", "zone": "public", "area_preferred": 16, "constraints": {"ceiling_height": 3.0, "min_width": 3.0}},
                    {"space_id": "stairs_0", "type": "stairs", "zone": "service", "area_preferred": 4, "constraints": {"ceiling_height": 3.0, "min_width": 1.5}}
                ]
            }
        ]
    }
})";

    const bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    ASSERT_EQ(building.floorPlates.size(), 1u);

    const FloorPlate* groundPlate = FindFloorPlate(building, 0);
    ASSERT_NE(groundPlate, nullptr);
    EXPECT_GE(groundPlate->envelopeOutline.size(), 6u);
    EXPECT_FALSE(PointInOutline(groundPlate->envelopeOutline, {4.0f, 4.0f}))
        << "Concave notch should stay empty instead of collapsing back to a convex hull";
    EXPECT_FALSE(PointInOutline(groundPlate->outline, {4.0f, 4.0f}))
        << "Usable outline should also preserve the concave cutout";
}

TEST_F(BuildingPipelineTest, ProcessOfficeTower_WithVerticalSystems_UsesEnclosedStairAndElevator) {
    const std::string json = TestHelpers::LoadFromFile("office_enclosed_core_demo.json");
    ASSERT_FALSE(json.empty());

    const bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.verticalTransports.size(), 2u);
    EXPECT_EQ(
        std::count_if(building.verticalTransports.begin(), building.verticalTransports.end(),
            [](const VerticalTransport& transport) { return transport.type == VerticalTransportType::Elevator; }),
        1);
    EXPECT_EQ(building.stairs.size(), 9u);
    EXPECT_EQ(building.floorPlates.size(), 10u);
}

TEST_F(BuildingPipelineTest, ProcessVilla_OpenAndExternalStairCases_ParseVerticalSystems) {
    for (const char* filename : {
             "villa_open_l_stair_demo.json",
             "villa_external_stair_demo.json",
             "townhouse_segmented_open_stair_demo.json" }) {
        const std::string json = TestHelpers::LoadFromFile(filename);
        ASSERT_FALSE(json.empty());

        GeneratedBuilding sampleBuilding;
        std::string sampleError;
        const bool result = pipeline.ProcessBuilding(json, sampleBuilding, sampleError);

        ASSERT_TRUE(result) << filename << " error: " << sampleError;
        EXPECT_GE(sampleBuilding.verticalTransports.size(), 1u);
        EXPECT_GE(sampleBuilding.stairs.size(), 1u);
    }
}

TEST_F(BuildingPipelineTest, ProcessOfficeTower_DualEgressVerticalSystems_KeepDistinctShafts) {
    const std::string json = TestHelpers::LoadFromFile("office_dual_egress_tower_demo.json");
    ASSERT_FALSE(json.empty());

    const bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.verticalTransports.size(), 3u);
    EXPECT_EQ(
        std::count_if(building.verticalTransports.begin(), building.verticalTransports.end(),
            [](const VerticalTransport& transport) { return transport.type == VerticalTransportType::Stair; }),
        2);
    EXPECT_EQ(
        std::count_if(building.verticalTransports.begin(), building.verticalTransports.end(),
            [](const VerticalTransport& transport) { return transport.type == VerticalTransportType::Elevator; }),
        1);
    EXPECT_EQ(building.stairs.size(), 22u);
}

TEST_F(BuildingPipelineTest, ProcessApartment_SingleStairDemo_DoesNotInventElevator) {
    const std::string json = TestHelpers::LoadFromFile("apartment_single_stair_demo.json");
    ASSERT_FALSE(json.empty());

    const bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(
        std::count_if(building.verticalTransports.begin(), building.verticalTransports.end(),
            [](const VerticalTransport& transport) { return transport.type == VerticalTransportType::Elevator; }),
        0);
    EXPECT_EQ(
        std::count_if(building.verticalTransports.begin(), building.verticalTransports.end(),
            [](const VerticalTransport& transport) { return transport.type == VerticalTransportType::Stair; }),
        1);
    EXPECT_EQ(building.stairs.size(), 4u);
}

TEST_F(BuildingPipelineTest, ProcessOfficeTower_DistinctSemanticDemos_PreserveProgramIntent) {
    GeneratedBuilding collaborationBuilding;
    GeneratedBuilding enterpriseBuilding;
    std::string collaborationError;
    std::string enterpriseError;

    const std::string collaborationJson = TestHelpers::LoadFromFile("office_collaboration_tower_demo.json");
    const std::string enterpriseJson = TestHelpers::LoadFromFile("office_enterprise_tower_demo.json");
    ASSERT_FALSE(collaborationJson.empty());
    ASSERT_FALSE(enterpriseJson.empty());

    const bool collaborationResult = pipeline.ProcessBuilding(collaborationJson, collaborationBuilding, collaborationError);
    const bool enterpriseResult = pipeline.ProcessBuilding(enterpriseJson, enterpriseBuilding, enterpriseError);

    EXPECT_TRUE(collaborationResult) << "Error: " << collaborationError;
    EXPECT_TRUE(enterpriseResult) << "Error: " << enterpriseError;

    auto hasProgramBlock = [](const GeneratedBuilding& generated, const char* blockId) {
        return std::any_of(generated.programBlocks.begin(), generated.programBlocks.end(),
            [&](const ProgramBlock& block) { return block.blockId == blockId; });
    };

    EXPECT_TRUE(hasProgramBlock(collaborationBuilding, "showroom_0"));
    EXPECT_TRUE(hasProgramBlock(collaborationBuilding, "client_lounge_1"));
    EXPECT_TRUE(hasProgramBlock(collaborationBuilding, "sky_meeting_7"));

    EXPECT_TRUE(hasProgramBlock(enterpriseBuilding, "retail_0"));
    EXPECT_TRUE(hasProgramBlock(enterpriseBuilding, "conference_center_1"));
    EXPECT_TRUE(hasProgramBlock(enterpriseBuilding, "sky_boardroom_7"));

    EXPECT_FALSE(hasProgramBlock(collaborationBuilding, "conference_center_1"));
    EXPECT_FALSE(hasProgramBlock(enterpriseBuilding, "showroom_0"));
}

TEST_F(BuildingPipelineTest, ProcessComplexShoppingMall_AllResolvedSpacesStayInsideFloorPlatesAndAvoidVoids) {
    const std::string json = TestHelpers::LoadFromFile("complex_shopping_mall_demo.json");
    ASSERT_FALSE(json.empty());

    const bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    ASSERT_FALSE(building.floorPlates.empty());
    ExpectAllSpacesInsideFloorPlates(building);
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

TEST_F(BuildingPipelineTest, ProcessCBDResidential_StairsOnlyConnectToExistingUpperFloors) {
    std::string json = TestHelpers::CreateCBDResidential();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    ASSERT_GT(building.stairs.size(), 0u);

    int maxFloorLevel = 0;
    for (const auto& floor : building.definition.floors) {
        maxFloorLevel = std::max(maxFloorLevel, floor.level);
    }

    for (const auto& stair : building.stairs) {
        EXPECT_GT(stair.toLevel, stair.fromLevel)
            << "Stairs should only be emitted for upward links";
        EXPECT_LE(stair.toLevel, maxFloorLevel)
            << "Stair target level must exist in the resolved building";
    }
}

TEST_F(BuildingPipelineTest, ProcessApartmentBuilding_TypicalFloorsKeepCirculationAndDaylight) {
    std::string json = TestHelpers::CreateApartmentBuilding();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    ASSERT_EQ(building.definition.floors.size(), 3u);
    EXPECT_GT(building.stairs.size(), 0u);
    EXPECT_GT(building.doors.size(), 6u);
    EXPECT_GT(building.windows.size(), 8u);
    EXPECT_GE(CountSpacesByUsage(building, SpaceUsage::Corridor), 2);

    for (int floorLevel : {1, 2}) {
        EXPECT_GE(CountSpacesOnFloorByUsage(building, floorLevel, SpaceUsage::Corridor), 1)
            << "Typical apartment floors should keep a shared circulation spine";
        EXPECT_GE(CountSpacesOnFloorByUsage(building, floorLevel, SpaceUsage::Living), 2)
            << "Typical apartment floors should still read as multiple units";
        EXPECT_GE(CountSpacesOnFloorByUsage(building, floorLevel, SpaceUsage::Bedroom), 2)
            << "Typical apartment floors should preserve private rooms";
        EXPECT_GE(CountWindowsOnFloor(building, floorLevel), 3)
            << "Residential upper floors should keep enough facade openings to avoid dead-looking units";
    }
}

TEST_F(BuildingPipelineTest, ProcessComplexShoppingMall_PreservesRetailAtriumAndVerticalTransport) {
    std::string json = TestHelpers::CreateComplexShoppingMall();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    ASSERT_EQ(building.definition.floors.size(), 3u);
    ASSERT_EQ(building.floorPlates.size(), 3u);
    EXPECT_GT(building.doors.size(), 2u);
    EXPECT_GT(building.windows.size(), 12u);
    EXPECT_GE(building.stairs.size(), 2u);
    EXPECT_GE(building.programBlocks.size(), 12u);
    EXPECT_FALSE(building.resolvedLayoutJson.empty());
    EXPECT_GE(CountVerticalCoresByType(building, VerticalCoreType::Elevator), 1)
        << "Mall should preserve an elevator core, not only stairs";
    EXPECT_GE(CountVerticalCoresByType(building, VerticalCoreType::Stair), 1)
        << "Mall should preserve dedicated stair circulation";

    for (const auto& plate : building.floorPlates) {
        EXPECT_FALSE(plate.voids.empty())
            << "Mall floor plate should keep atrium voids so the mass does not collapse into a solid block";
    }
}

TEST_F(BuildingPipelineTest, ProcessTownhouseVilla_RealisticReferenceDataRemainsPlausible) {
    std::string json = TestHelpers::CreateTownhouseVilla();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 2u);
    EXPECT_GT(building.stairs.size(), 0u);
    EXPECT_GT(building.doors.size(), 6u);
    EXPECT_GT(building.windows.size(), 8u);
    EXPECT_GT(CountWallsByType(building, WallType::Interior), 8);
}

TEST_F(BuildingPipelineTest, ProcessMidriseApartment_ReferenceDataKeepsResidentialPattern) {
    std::string json = TestHelpers::CreateMidriseApartment();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 4u);
    EXPECT_GT(building.stairs.size(), 1u);
    EXPECT_GE(building.doors.size(), 8u);
    EXPECT_GT(building.windows.size(), 12u);
    EXPECT_GE(CountSpacesByUsage(building, SpaceUsage::Bedroom), 4);
    EXPECT_GE(building.connections.size(), 8u);
}

TEST_F(BuildingPipelineTest, ProcessNeighborhoodOffice_ReferenceDataKeepsOfficeProgram) {
    std::string json = TestHelpers::CreateNeighborhoodOffice();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 3u);
    EXPECT_GE(building.doors.size(), 2u);
    EXPECT_GT(building.windows.size(), 10u);
    EXPECT_GE(building.programBlocks.size(), 8u);
    EXPECT_GE(CountSpacesByUsage(building, SpaceUsage::Office), 3);
}

TEST_F(BuildingPipelineTest, ProcessRetailCenter_ReferenceDataKeepsAtriumAndCores) {
    std::string json = TestHelpers::CreateRetailCenter();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 2u);
    EXPECT_GT(building.windows.size(), 10u);
    EXPECT_GT(building.stairs.size(), 0u);
    EXPECT_GE(CountVerticalCoresByType(building, VerticalCoreType::Elevator), 1);
    EXPECT_GE(CountVerticalCoresByType(building, VerticalCoreType::Stair), 1);
    ASSERT_EQ(building.floorPlates.size(), 2u);
    for (const auto& plate : building.floorPlates) {
        EXPECT_FALSE(plate.voids.empty());
    }
}

TEST_F(BuildingPipelineTest, ReferenceDatasets_UsePlausibleResidentialInputs) {
    ExpectReferenceDatasetLooksPlausible(TestHelpers::CreateTownhouseVilla(), "villa", 2, 2);
    ExpectReferenceDatasetLooksPlausible(TestHelpers::CreateCourtyardVilla(), "villa", 2, 2);
    ExpectReferenceDatasetLooksPlausible(TestHelpers::CreateMidriseApartment(), "apartment", 4, 4);
    ExpectReferenceDatasetLooksPlausible(TestHelpers::CreateSlenderCBDResidential(), "cbd_residential", 5, 5);

    SemanticBuilding townhouse;
    std::string parseError;
    ASSERT_TRUE(SemanticBuildingParser::ParseFromString(TestHelpers::CreateTownhouseVilla(), townhouse, parseError));
    EXPECT_GE(CountSemanticSpaces(townhouse, "bedroom"), 3);
    EXPECT_GE(AverageAreaForType(townhouse, "bedroom"), 15.0f);

    SemanticBuilding slender;
    ASSERT_TRUE(SemanticBuildingParser::ParseFromString(TestHelpers::CreateSlenderCBDResidential(), slender, parseError));
    EXPECT_GE(CountSemanticSpaces(slender, "corridor"), 4);
    EXPECT_EQ(CountSemanticSpaces(slender, "core"), 5);
}

TEST_F(BuildingPipelineTest, ReferenceDatasets_UsePlausibleCommercialInputs) {
    ExpectReferenceDatasetLooksPlausible(TestHelpers::CreateNeighborhoodOffice(), "office", 3, 3);
    ExpectReferenceDatasetLooksPlausible(TestHelpers::CreateCorporateOfficeTower(), "office_tower", 6, 6);
    ExpectReferenceDatasetLooksPlausible(TestHelpers::CreateRetailCenter(), "retail_center", 2, 2);
    ExpectReferenceDatasetLooksPlausible(TestHelpers::CreateShoppingCenter(), "shopping_center", 2, 2);

    SemanticBuilding corporate;
    std::string parseError;
    ASSERT_TRUE(SemanticBuildingParser::ParseFromString(TestHelpers::CreateCorporateOfficeTower(), corporate, parseError));
    EXPECT_GE(CountSemanticSpaces(corporate, "office"), 4);
    EXPECT_GE(CountSemanticSpaces(corporate, "meeting_room"), 5);
    EXPECT_GE(AverageAreaForType(corporate, "office"), 80.0f);

    SemanticBuilding retail;
    ASSERT_TRUE(SemanticBuildingParser::ParseFromString(TestHelpers::CreateShoppingCenter(), retail, parseError));
    EXPECT_GE(CountSemanticSpaces(retail, "void"), 2);
    EXPECT_GE(CountSemanticSpaces(retail, "shop"), 4);
    EXPECT_GE(AverageAreaForType(retail, "shop"), 90.0f);
}

TEST_F(BuildingPipelineTest, ProcessCourtyardVilla_ReferenceDataRemainsPlausible) {
    std::string json = TestHelpers::CreateCourtyardVilla();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 2u);
    EXPECT_GT(building.stairs.size(), 0u);
    EXPECT_GT(building.doors.size(), 8u);
    EXPECT_GT(CountWallsByType(building, WallType::Interior), 10);
}

TEST_F(BuildingPipelineTest, ProcessSlenderCBDResidential_ReferenceDataKeepsTowerPattern) {
    std::string json = TestHelpers::CreateSlenderCBDResidential();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 5u);
    EXPECT_GT(building.stairs.size(), 2u);
    EXPECT_GE(CountSpacesByUsage(building, SpaceUsage::Bedroom), 3);
    EXPECT_GT(building.windows.size(), 14u);
}

TEST_F(BuildingPipelineTest, ProcessCorporateOfficeTower_ReferenceDataKeepsPodiumTowerSplit) {
    std::string json = TestHelpers::CreateCorporateOfficeTower();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 6u);
    EXPECT_GT(building.stairs.size(), 2u);
    EXPECT_GE(CountSpacesByUsage(building, SpaceUsage::Office), 4);
    EXPECT_GE(CountSpacesByUsage(building, SpaceUsage::Entrance), 1);
    EXPECT_GT(building.programBlocks.size(), 12u);
}

TEST_F(BuildingPipelineTest, ProcessShoppingCenter_ReferenceDataKeepsRetailPattern) {
    std::string json = TestHelpers::CreateShoppingCenter();

    bool result = pipeline.ProcessBuilding(json, building, errorMsg);

    ASSERT_TRUE(result) << "Error: " << errorMsg;
    EXPECT_EQ(building.definition.floors.size(), 2u);
    EXPECT_GE(CountVerticalCoresByType(building, VerticalCoreType::Elevator), 1);
    EXPECT_GE(CountVerticalCoresByType(building, VerticalCoreType::Stair), 1);
    EXPECT_GT(building.windows.size(), 12u);
    EXPECT_GT(building.doors.size(), 2u);
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
