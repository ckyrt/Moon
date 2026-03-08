#include "BuildingPipeline.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_set>

namespace {

float CeilToGrid(float value, float grid) {
    if (grid <= 0.0f) {
        return value;
    }
    return std::ceil((value - 0.0001f) / grid) * grid;
}

float SnapToGrid(float value, float grid) {
    if (grid <= 0.0f) {
        return value;
    }
    return std::round(value / grid) * grid;
}

bool RectanglesOverlap(const Moon::Building::Rect& a, const Moon::Building::Rect& b) {
    const float epsilon = 0.001f;
    const float aMaxX = a.origin[0] + a.size[0];
    const float aMaxY = a.origin[1] + a.size[1];
    const float bMaxX = b.origin[0] + b.size[0];
    const float bMaxY = b.origin[1] + b.size[1];

    return a.origin[0] < bMaxX - epsilon && aMaxX > b.origin[0] + epsilon &&
           a.origin[1] < bMaxY - epsilon && aMaxY > b.origin[1] + epsilon;
}

const Moon::Building::Mass* FindMassForFloor(const Moon::Building::BuildingDefinition& definition,
                                             const Moon::Building::Floor& floor) {
    for (const auto& mass : definition.masses) {
        if (mass.massId == floor.massId) {
            return &mass;
        }
    }
    return nullptr;
}

std::string DescribeSpace(const Moon::Building::Space& space) {
    if (!space.rects.empty()) {
        return space.rects.front().rectId;
    }
    return std::to_string(space.spaceId);
}

std::string DescribeSpaceType(const Moon::Building::Space& space) {
    return Moon::Building::SpaceUsageToString(space.properties.usage);
}

} // namespace

namespace Moon {
namespace Building {

BuildingPipeline::BuildingPipeline() {
    // Set default parameters for generators
    m_wallGenerator.SetWallThickness(0.2f);
    m_wallGenerator.SetDefaultWallHeight(3.0f);
    
    m_doorGenerator.SetDefaultDoorSize(0.9f, 2.1f);
    
    m_stairGenerator.SetStepParameters(0.18f, 0.28f);
    
    m_facadeGenerator.SetWindowParameters(1.2f, 1.5f, 0.9f);
    
    m_layoutValidator.SetMinRoomSize(1.5f, 1.5f);
    m_layoutValidator.SetWallThickness(0.2f);
}

BuildingPipeline::~BuildingPipeline() {}

bool BuildingPipeline::ProcessBuilding(const std::string& jsonStr,
                                       GeneratedBuilding& outBuilding,
                                       std::string& outError) {
    // Step 1: Validate and parse JSON schema
    BuildingDefinition definition;
    if (!m_schemaValidator.ValidateAndParse(jsonStr, definition, outError)) {
        return false;
    }
    
    // Process the parsed definition
    return ProcessBuildingInternal(definition, outBuilding, nullptr, false, outError);
}

bool BuildingPipeline::ProcessBuildingBestEffort(const std::string& jsonStr,
                                                 GeneratedBuilding& outBuilding,
                                                 BestEffortGenerationReport& outReport,
                                                 std::string& outError) {
    BuildingDefinition definition;
    outReport.usedBestEffort = false;
    outReport.adjustedSpaces.clear();
    outReport.skippedSpaces.clear();
    outReport.repairNotes.clear();
    if (!m_schemaValidator.ValidateAndParseBestEffort(jsonStr, definition, outReport, outError)) {
        return false;
    }

    return ProcessBuildingInternal(definition, outBuilding, &outReport, true, outError);
}

bool BuildingPipeline::ProcessBuilding(const BuildingDefinition& definition,
                                       GeneratedBuilding& outBuilding,
                                       std::string& outError) {
    return ProcessBuildingInternal(definition, outBuilding, nullptr, false, outError);
}

bool BuildingPipeline::ProcessBuildingInternal(const BuildingDefinition& definition,
                                              GeneratedBuilding& outBuilding,
                                              BestEffortGenerationReport* outReport,
                                              bool bestEffort,
                                              std::string& outError) {
    BuildingDefinition workingDefinition = definition;

    if (bestEffort && outReport) {
        if (!RepairDefinitionForBestEffort(workingDefinition, *outReport, outError)) {
            return false;
        }
    }

    // Step 2: Validate layout
    ValidationResult layoutResult = m_layoutValidator.Validate(workingDefinition);
    if (!layoutResult.valid) {
        outError = "Layout validation failed:\n";
        for (const auto& error : layoutResult.errors) {
            outError += "  - " + error + "\n";
        }
        return false;
    }
    
    // Log warnings if any
    if (!layoutResult.warnings.empty()) {
        // In a full implementation, would log these warnings
        // For now, continue processing
    }
    
    // Store definition
    outBuilding.definition = workingDefinition;
    
    // Step 3: Process masses and floors (implicit, data already in definition)
    if (!ProcessMassAndFloors(workingDefinition)) {
        outError = "Mass/Floor processing failed";
        return false;
    }
    
    // Step 4: Build space connectivity graph
    if (!BuildSpaceGraph(workingDefinition, outBuilding)) {
        outError = "Space graph construction failed";
        return false;
    }
    
    // Step 5: Generate walls
    if (!GenerateWalls(workingDefinition, outBuilding)) {
        outError = "Wall generation failed";
        return false;
    }
    
    // Step 6: Generate doors
    if (!GenerateDoors(workingDefinition, outBuilding)) {
        outError = "Door generation failed";
        return false;
    }
    
    // Step 7: Generate stairs
    if (!GenerateStairs(workingDefinition, outBuilding)) {
        outError = "Stair generation failed";
        return false;
    }
    
    // Step 8: Generate facade
    if (!GenerateFacade(workingDefinition, outBuilding)) {
        outError = "Facade generation failed";
        return false;
    }
    
    return true;
}

bool BuildingPipeline::RepairDefinitionForBestEffort(BuildingDefinition& definition,
                                                     BestEffortGenerationReport& outReport,
                                                     std::string& outError) {
    const float minRoomWidth = 1.5f;
    const float minRoomDepth = 1.5f;
    int remainingSpaceCount = 0;

    auto recordAdjustment = [&](int floorLevel,
                                const Space& space,
                                const std::string& reason) {
        BestEffortAdjustedSpace issue;
        issue.floorLevel = floorLevel;
        issue.spaceId = DescribeSpace(space);
        issue.spaceType = DescribeSpaceType(space);
        issue.reason = reason;
        outReport.usedBestEffort = true;
        outReport.adjustedSpaces.push_back(issue);
    };

    auto recordSkip = [&](int floorLevel,
                          const Space& space,
                          const std::string& reason) {
        BestEffortSkippedSpace issue;
        issue.floorLevel = floorLevel;
        issue.spaceId = DescribeSpace(space);
        issue.spaceType = DescribeSpaceType(space);
        issue.reason = reason;
        outReport.usedBestEffort = true;
        outReport.skippedSpaces.push_back(issue);
    };

    for (auto& floor : definition.floors) {
        const Mass* mass = FindMassForFloor(definition, floor);
        if (!mass) {
            continue;
        }

        std::vector<Space> repairedSpaces;
        for (auto& space : floor.spaces) {
            bool dropSpace = false;

            for (auto& rect : space.rects) {
                if (rect.size[0] <= 0.0f || rect.size[1] <= 0.0f) {
                    recordSkip(floor.level, space, "rect has non-positive size after resolution");
                    dropSpace = true;
                    break;
                }

                const float originalX = rect.origin[0];
                const float originalY = rect.origin[1];
                const float originalWidth = rect.size[0];
                const float originalHeight = rect.size[1];

                rect.size[0] = std::max(rect.size[0], CeilToGrid(minRoomWidth, definition.grid));
                rect.size[1] = std::max(rect.size[1], CeilToGrid(minRoomDepth, definition.grid));

                if (rect.size[0] > mass->size[0] || rect.size[1] > mass->size[1]) {
                    recordSkip(floor.level, space, "rect cannot fit inside building mass after repair");
                    dropSpace = true;
                    break;
                }

                rect.origin[0] = std::max(mass->origin[0], rect.origin[0]);
                rect.origin[1] = std::max(mass->origin[1], rect.origin[1]);
                rect.origin[0] = std::min(rect.origin[0], mass->origin[0] + mass->size[0] - rect.size[0]);
                rect.origin[1] = std::min(rect.origin[1], mass->origin[1] + mass->size[1] - rect.size[1]);
                rect.origin[0] = SnapToGrid(rect.origin[0], definition.grid);
                rect.origin[1] = SnapToGrid(rect.origin[1], definition.grid);
                rect.size[0] = CeilToGrid(rect.size[0], definition.grid);
                rect.size[1] = CeilToGrid(rect.size[1], definition.grid);

                if (rect.origin[0] + rect.size[0] > mass->origin[0] + mass->size[0] + 0.001f) {
                    rect.origin[0] = mass->origin[0] + mass->size[0] - rect.size[0];
                }
                if (rect.origin[1] + rect.size[1] > mass->origin[1] + mass->size[1] + 0.001f) {
                    rect.origin[1] = mass->origin[1] + mass->size[1] - rect.size[1];
                }

                if (std::abs(rect.origin[0] - originalX) > 0.001f ||
                    std::abs(rect.origin[1] - originalY) > 0.001f ||
                    std::abs(rect.size[0] - originalWidth) > 0.001f ||
                    std::abs(rect.size[1] - originalHeight) > 0.001f) {
                    std::ostringstream oss;
                    oss << "repaired rect '" << rect.rectId << "' to origin ("
                        << rect.origin[0] << ", " << rect.origin[1] << ") size ("
                        << rect.size[0] << " x " << rect.size[1] << ")";
                    recordAdjustment(floor.level, space, oss.str());
                }
            }

            if (dropSpace) {
                continue;
            }

            bool overlapsExisting = false;
            for (const auto& existingSpace : repairedSpaces) {
                for (const auto& existingRect : existingSpace.rects) {
                    for (const auto& rect : space.rects) {
                        if (RectanglesOverlap(existingRect, rect)) {
                            overlapsExisting = true;
                            break;
                        }
                    }
                    if (overlapsExisting) {
                        break;
                    }
                }
                if (overlapsExisting) {
                    break;
                }
            }

            if (overlapsExisting) {
                recordSkip(floor.level, space, "space overlaps repaired layout and was removed");
                continue;
            }

            if (space.properties.hasStairs) {
                bool targetExists = false;
                for (const auto& targetFloor : definition.floors) {
                    if (targetFloor.level == space.stairsConfig.connectToLevel) {
                        targetExists = true;
                        break;
                    }
                }
                if (!targetExists) {
                    space.stairsConfig.connectToLevel = floor.level;
                    recordAdjustment(floor.level, space, "repaired stair target to an existing floor level");
                }
            }

            repairedSpaces.push_back(space);
        }

        floor.spaces = std::move(repairedSpaces);
        remainingSpaceCount += static_cast<int>(floor.spaces.size());
    }

    if (remainingSpaceCount == 0) {
        outError = "Best-effort repair removed all spaces; nothing renderable remains";
        return false;
    }

    return true;
}

bool BuildingPipeline::ValidateOnly(const std::string& jsonStr,
                                    ValidationResult& outResult) {
    // Parse schema
    BuildingDefinition definition;
    std::string parseError;
    if (!m_schemaValidator.ValidateAndParse(jsonStr, definition, parseError)) {
        outResult.valid = false;
        outResult.errors.push_back(parseError);
        return false;
    }
    
    // Validate layout
    outResult = m_layoutValidator.Validate(definition);
    return outResult.valid;
}

bool BuildingPipeline::ProcessMassAndFloors(const BuildingDefinition& definition) {
    // Mass and floor data is already in the definition
    // This step would perform any additional processing if needed
    // For now, just validate that masses and floors exist
    if (definition.masses.empty()) {
        return false;
    }
    if (definition.floors.empty()) {
        return false;
    }
    return true;
}

bool BuildingPipeline::BuildSpaceGraph(const BuildingDefinition& definition,
                                       GeneratedBuilding& outBuilding) {
    m_spaceGraphBuilder.BuildGraph(definition, outBuilding.connections);
    return true;
}

bool BuildingPipeline::GenerateWalls(const BuildingDefinition& definition,
                                     GeneratedBuilding& outBuilding) {
    m_wallGenerator.GenerateWalls(definition, m_spaceGraphBuilder, outBuilding.walls);
    return true;
}

bool BuildingPipeline::GenerateDoors(const BuildingDefinition& definition,
                                     GeneratedBuilding& outBuilding) {
    // Build index for fast lookups (O(1) instead of O(n) scans)
    BuildingIndex index;
    index.Build(definition, &m_spaceGraphBuilder, &outBuilding.walls);
    
    m_doorGenerator.GenerateDoors(definition, m_spaceGraphBuilder, 
                                  outBuilding.walls, index, outBuilding.doors);
    return true;
}

bool BuildingPipeline::GenerateStairs(const BuildingDefinition& definition,
                                      GeneratedBuilding& outBuilding) {
    m_stairGenerator.GenerateStairs(definition, m_stairs);
    // Copy stairs to output building
    outBuilding.stairs = m_stairs;
    return true;
}

bool BuildingPipeline::GenerateFacade(const BuildingDefinition& definition,
                                      GeneratedBuilding& outBuilding) {
    // Build index for fast lookups
    BuildingIndex index;
    index.Build(definition, &m_spaceGraphBuilder, &outBuilding.walls);
    
    m_facadeGenerator.GenerateFacade(definition, outBuilding.walls, index,
                                     outBuilding.windows, m_facadeElements);
    return true;
}

} // namespace Building
} // namespace Moon
