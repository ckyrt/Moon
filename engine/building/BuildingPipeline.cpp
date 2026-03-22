#include "BuildingPipeline.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
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

std::string ToLowerCopy(const std::string& value) {
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowered;
}

bool ContainsToken(const std::string& value, const char* token) {
    return ToLowerCopy(value).find(token) != std::string::npos;
}

Moon::Building::VerticalCoreType ClassifyCoreType(const Moon::Building::Rect& rect) {
    const std::string id = ToLowerCopy(rect.rectId);
    if (id.find("elevator") != std::string::npos || id.find("lift") != std::string::npos) {
        return Moon::Building::VerticalCoreType::Elevator;
    }
    if (id.find("stair") != std::string::npos) {
        return Moon::Building::VerticalCoreType::Stair;
    }
    return Moon::Building::VerticalCoreType::Service;
}

bool InferMallVoidFromCorridor(const Moon::Building::Space& space,
                               Moon::Building::Rect& outVoid) {
    if (space.properties.usage != Moon::Building::SpaceUsage::Corridor || space.rects.size() < 4) {
        return false;
    }

    float innerMinX = std::numeric_limits<float>::max();
    float innerMinY = std::numeric_limits<float>::max();
    float innerMaxX = -std::numeric_limits<float>::max();
    float innerMaxY = -std::numeric_limits<float>::max();
    bool hasLeft = false;
    bool hasRight = false;
    bool hasTop = false;
    bool hasBottom = false;

    for (const auto& rect : space.rects) {
        const bool verticalBand = rect.size[0] <= rect.size[1];
        if (verticalBand) {
            const float centerX = rect.origin[0] + rect.size[0] * 0.5f;
            if (!hasLeft || centerX < innerMinX) {
                innerMinX = rect.origin[0] + rect.size[0];
                hasLeft = true;
            }
            if (!hasRight || centerX > innerMaxX) {
                innerMaxX = rect.origin[0];
                hasRight = true;
            }
        } else {
            const float centerY = rect.origin[1] + rect.size[1] * 0.5f;
            if (!hasTop || centerY < innerMinY) {
                innerMinY = rect.origin[1] + rect.size[1];
                hasTop = true;
            }
            if (!hasBottom || centerY > innerMaxY) {
                innerMaxY = rect.origin[1];
                hasBottom = true;
            }
        }
    }

    if (!(hasLeft && hasRight && hasTop && hasBottom)) {
        return false;
    }
    if (innerMaxX - innerMinX < 0.5f || innerMaxY - innerMinY < 0.5f) {
        return false;
    }

    outVoid.rectId = space.rects.front().rectId + "_void";
    outVoid.origin = {innerMinX, innerMinY};
    outVoid.size = {innerMaxX - innerMinX, innerMaxY - innerMinY};
    return true;
}

bool RectContainsPoint(const Moon::Building::Rect& rect, const Moon::Building::GridPos2D& point) {
    return point[0] > rect.origin[0] + 0.001f &&
           point[0] < rect.origin[0] + rect.size[0] - 0.001f &&
           point[1] > rect.origin[1] + 0.001f &&
           point[1] < rect.origin[1] + rect.size[1] - 0.001f;
}

bool PointInOutline(const std::vector<Moon::Building::GridPos2D>& outline,
                    const Moon::Building::GridPos2D& point) {
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

bool IsCommercialOrOfficeTower(const Moon::Building::BuildingDefinition& definition);

Moon::Building::GridPos2D ComputePlateCenter(const Moon::Building::FloorPlate& plate) {
    if (plate.outline.size() >= 3) {
        float sumX = 0.0f;
        float sumY = 0.0f;
        for (const auto& point : plate.outline) {
            sumX += point[0];
            sumY += point[1];
        }
        return {sumX / static_cast<float>(plate.outline.size()),
                sumY / static_cast<float>(plate.outline.size())};
    }

    return {
        plate.origin[0] + plate.size[0] * 0.5f,
        plate.origin[1] + plate.size[1] * 0.5f
    };
}

bool HasFloorProgramType(const Moon::Building::Floor& floor, const char* token) {
    for (const auto& space : floor.spaces) {
        for (const auto& rect : space.rects) {
            if (ContainsToken(rect.rectId, token)) {
                return true;
            }
        }
    }
    return false;
}

Moon::Building::Rect MakeCenteredRect(const Moon::Building::GridPos2D& center,
                                      float width,
                                      float depth) {
    Moon::Building::Rect rect;
    rect.origin = {
        center[0] - width * 0.5f,
        center[1] - depth * 0.5f
    };
    rect.size = {width, depth};
    return rect;
}

bool RectFitsPlate(const Moon::Building::FloorPlate& plate,
                   const Moon::Building::Rect& rect,
                   float margin) {
    const std::array<Moon::Building::GridPos2D, 5> samplePoints = {{
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

void AddProgramBlockIfValid(const Moon::Building::FloorPlate& plate,
                            Moon::Building::GeneratedBuilding& outBuilding,
                            int floorLevel,
                            Moon::Building::SpaceUsage usage,
                            const std::string& blockId,
                            const Moon::Building::Rect& rect,
                            float margin) {
    if (rect.size[0] <= 0.5f || rect.size[1] <= 0.5f) {
        return;
    }
    if (!RectFitsPlate(plate, rect, margin)) {
        return;
    }

    Moon::Building::ProgramBlock block;
    block.blockId = blockId;
    block.floorLevel = floorLevel;
    block.usage = usage;
    block.rect = rect;
    outBuilding.programBlocks.push_back(block);
}

void AddOfficeCirculationBlocks(const Moon::Building::BuildingDefinition& definition,
                                const Moon::Building::Floor& floor,
                                const Moon::Building::FloorPlate& plate,
                                const std::vector<Moon::Building::VerticalCore>& cores,
                                Moon::Building::GeneratedBuilding& outBuilding) {
    auto stairIt = std::find_if(cores.begin(), cores.end(),
        [](const Moon::Building::VerticalCore& core) { return core.type == Moon::Building::VerticalCoreType::Stair; });
    auto elevatorIt = std::find_if(cores.begin(), cores.end(),
        [](const Moon::Building::VerticalCore& core) { return core.type == Moon::Building::VerticalCoreType::Elevator; });
    if (stairIt == cores.end() || elevatorIt == cores.end()) {
        return;
    }

    Moon::Building::Rect combinedCore;
    const float coreMinX = std::min(stairIt->rect.origin[0], elevatorIt->rect.origin[0]);
    const float coreMinY = std::min(stairIt->rect.origin[1], elevatorIt->rect.origin[1]);
    const float coreMaxX = std::max(stairIt->rect.origin[0] + stairIt->rect.size[0],
                                    elevatorIt->rect.origin[0] + elevatorIt->rect.size[0]);
    const float coreMaxY = std::max(stairIt->rect.origin[1] + stairIt->rect.size[1],
                                    elevatorIt->rect.origin[1] + elevatorIt->rect.size[1]);
    combinedCore.origin = {coreMinX, coreMinY};
    combinedCore.size = {coreMaxX - coreMinX, coreMaxY - coreMinY};

    const float corridorWidth = definition.style.category == "commercial" ? 2.5f : 2.0f;
    const float margin = std::max(0.2f, definition.grid * 0.5f);

    Moon::Building::Rect north;
    north.origin = {combinedCore.origin[0] - corridorWidth,
                    combinedCore.origin[1] + combinedCore.size[1]};
    north.size = {combinedCore.size[0] + corridorWidth * 2.0f, corridorWidth};
    AddProgramBlockIfValid(plate, outBuilding, floor.level, Moon::Building::SpaceUsage::Corridor,
        "corridor_north_" + std::to_string(floor.level), north, margin);

    Moon::Building::Rect south;
    south.origin = {combinedCore.origin[0] - corridorWidth,
                    combinedCore.origin[1] - corridorWidth};
    south.size = {combinedCore.size[0] + corridorWidth * 2.0f, corridorWidth};
    AddProgramBlockIfValid(plate, outBuilding, floor.level, Moon::Building::SpaceUsage::Corridor,
        "corridor_south_" + std::to_string(floor.level), south, margin);

    Moon::Building::Rect west;
    west.origin = {combinedCore.origin[0] - corridorWidth,
                   combinedCore.origin[1]};
    west.size = {corridorWidth, combinedCore.size[1]};
    AddProgramBlockIfValid(plate, outBuilding, floor.level, Moon::Building::SpaceUsage::Corridor,
        "corridor_west_" + std::to_string(floor.level), west, margin);

    Moon::Building::Rect east;
    east.origin = {combinedCore.origin[0] + combinedCore.size[0],
                   combinedCore.origin[1]};
    east.size = {corridorWidth, combinedCore.size[1]};
    AddProgramBlockIfValid(plate, outBuilding, floor.level, Moon::Building::SpaceUsage::Corridor,
        "corridor_east_" + std::to_string(floor.level), east, margin);

    if (floor.level == 0) {
        Moon::Building::Rect lobby;
        lobby.origin = {
            combinedCore.origin[0] - combinedCore.size[0] * 0.35f,
            combinedCore.origin[1] - corridorWidth - 6.0f
        };
        lobby.size = {
            combinedCore.size[0] * 1.7f,
            5.5f
        };
        AddProgramBlockIfValid(plate, outBuilding, floor.level, Moon::Building::SpaceUsage::Entrance,
            "lobby_" + std::to_string(floor.level), lobby, margin);
    }

    const float sideInset = std::max(0.8f, corridorWidth + definition.grid);
    Moon::Building::Rect westOffice;
    westOffice.origin = {plate.origin[0] + margin, plate.origin[1] + margin};
    westOffice.size = {
        std::max(0.5f, combinedCore.origin[0] - sideInset - westOffice.origin[0]),
        std::max(0.5f, plate.size[1] - margin * 2.0f)
    };

    Moon::Building::Rect eastOffice;
    eastOffice.origin = {
        combinedCore.origin[0] + combinedCore.size[0] + sideInset,
        plate.origin[1] + margin
    };
    eastOffice.size = {
        std::max(0.5f, plate.origin[0] + plate.size[0] - margin - eastOffice.origin[0]),
        std::max(0.5f, plate.size[1] - margin * 2.0f)
    };

    Moon::Building::Rect northOffice;
    northOffice.origin = {
        std::max(plate.origin[0] + margin, combinedCore.origin[0] - combinedCore.size[0] * 0.25f),
        combinedCore.origin[1] + combinedCore.size[1] + sideInset
    };
    northOffice.size = {
        std::min(plate.size[0] - margin * 2.0f, combinedCore.size[0] * 1.5f),
        std::max(0.5f, plate.origin[1] + plate.size[1] - margin - northOffice.origin[1])
    };

    AddProgramBlockIfValid(plate, outBuilding, floor.level,
        floor.level == 0 ? Moon::Building::SpaceUsage::Office : Moon::Building::SpaceUsage::Office,
        "office_west_" + std::to_string(floor.level), westOffice, margin);
    AddProgramBlockIfValid(plate, outBuilding, floor.level,
        Moon::Building::SpaceUsage::Office,
        "office_east_" + std::to_string(floor.level), eastOffice, margin);
    AddProgramBlockIfValid(plate, outBuilding, floor.level,
        floor.level == 1 ? Moon::Building::SpaceUsage::Office : Moon::Building::SpaceUsage::Office,
        "office_north_" + std::to_string(floor.level), northOffice, margin);
}

void GenerateMassDrivenProgramBlocks(const Moon::Building::BuildingDefinition& definition,
                                     Moon::Building::GeneratedBuilding& outBuilding) {
    if (outBuilding.floorPlates.empty() || outBuilding.verticalCores.empty()) {
        return;
    }

    for (const auto& floor : definition.floors) {
        auto plateIt = std::find_if(outBuilding.floorPlates.begin(), outBuilding.floorPlates.end(),
            [&](const Moon::Building::FloorPlate& plate) { return plate.floorLevel == floor.level; });
        if (plateIt == outBuilding.floorPlates.end()) {
            continue;
        }

        std::vector<Moon::Building::VerticalCore> floorCores;
        for (const auto& core : outBuilding.verticalCores) {
            if (floor.level >= core.floorFrom && floor.level <= core.floorTo) {
                floorCores.push_back(core);
            }
        }

        if (IsCommercialOrOfficeTower(definition)) {
            AddOfficeCirculationBlocks(definition, floor, *plateIt, floorCores, outBuilding);
        }
    }
}

float GetFloorTopHeight(const Moon::Building::BuildingDefinition& definition, int floorLevel) {
    const float base = Moon::Building::GetFloorBaseHeight(definition, floorLevel);
    for (const auto& floor : definition.floors) {
        if (floor.level == floorLevel) {
            return base + floor.floorHeight;
        }
    }
    return base;
}

bool IsCommercialOrOfficeTower(const Moon::Building::BuildingDefinition& definition) {
    return definition.style.category == "commercial" || definition.style.category == "retail";
}

void AddMassDrivenCoreIfNeeded(const Moon::Building::BuildingDefinition& definition,
                               int maxFloorLevel,
                               Moon::Building::GeneratedBuilding& outBuilding) {
    if (!outBuilding.verticalCores.empty() || outBuilding.floorPlates.empty()) {
        return;
    }

    float minWidth = std::numeric_limits<float>::max();
    float minDepth = std::numeric_limits<float>::max();
    float centerXSum = 0.0f;
    float centerYSum = 0.0f;
    int count = 0;

    for (const auto& plate : outBuilding.floorPlates) {
        minWidth = std::min(minWidth, plate.size[0]);
        minDepth = std::min(minDepth, plate.size[1]);
        const auto center = ComputePlateCenter(plate);
        centerXSum += center[0];
        centerYSum += center[1];
        ++count;
    }

    if (count == 0 || minWidth <= 0.0f || minDepth <= 0.0f) {
        return;
    }

    const float centerX = centerXSum / static_cast<float>(count);
    const float centerY = centerYSum / static_cast<float>(count);
    float targetWidth = IsCommercialOrOfficeTower(definition)
        ? std::clamp(minWidth * 0.28f, 5.0f, 8.5f)
        : std::clamp(minWidth * 0.24f, 3.5f, 6.0f);
    float targetDepth = IsCommercialOrOfficeTower(definition)
        ? std::clamp(minDepth * 0.32f, 6.0f, 10.0f)
        : std::clamp(minDepth * 0.28f, 4.0f, 7.0f);
    const Moon::Building::GridPos2D center = {centerX, centerY};
    const float fitMargin = std::max(0.35f, definition.grid * 0.5f);

    bool fitted = false;
    for (int attempts = 0; attempts < 12 && !fitted; ++attempts) {
        const Moon::Building::Rect candidate = MakeCenteredRect(center, targetWidth, targetDepth);
        fitted = std::all_of(outBuilding.floorPlates.begin(), outBuilding.floorPlates.end(),
            [&](const Moon::Building::FloorPlate& plate) {
                return RectFitsPlate(plate, candidate, fitMargin);
            });
        if (!fitted) {
            targetWidth = std::max(definition.grid * 4.0f, targetWidth - definition.grid);
            targetDepth = std::max(definition.grid * 4.0f, targetDepth - definition.grid);
        }
    }

    Moon::Building::Rect overallCoreRect = MakeCenteredRect(center, targetWidth, targetDepth);

    const float circulationGap = definition.grid;
    const float stairWidth = std::clamp(targetWidth * 0.26f, 1.8f, 2.8f);
    const float serviceWidth = std::clamp(targetWidth * 0.24f, 1.5f, 2.4f);
    const float elevatorWidth = std::max(2.4f, targetWidth - stairWidth - serviceWidth - circulationGap * 2.0f);

    Moon::Building::Rect stairRect;
    stairRect.rectId = "mass_core_stair";
    stairRect.origin = {overallCoreRect.origin[0], overallCoreRect.origin[1]};
    stairRect.size = {stairWidth, overallCoreRect.size[1]};

    Moon::Building::Rect elevatorRect;
    elevatorRect.rectId = "mass_core_elevator";
    elevatorRect.origin = {
        stairRect.origin[0] + stairRect.size[0] + circulationGap,
        overallCoreRect.origin[1] + std::max(0.0f, (overallCoreRect.size[1] - std::max(2.8f, overallCoreRect.size[1] * 0.4f)) * 0.5f)
    };
    elevatorRect.size = {
        std::max(2.4f, elevatorWidth),
        std::max(2.8f, overallCoreRect.size[1] * 0.4f)
    };

    Moon::Building::Rect serviceRect;
    serviceRect.rectId = "mass_core_service";
    serviceRect.origin = {
        elevatorRect.origin[0] + elevatorRect.size[0] + circulationGap,
        overallCoreRect.origin[1]
    };
    serviceRect.size = {
        std::max(1.2f, overallCoreRect.origin[0] + overallCoreRect.size[0] - serviceRect.origin[0]),
        overallCoreRect.size[1]
    };

    if (!std::all_of(outBuilding.floorPlates.begin(), outBuilding.floorPlates.end(),
        [&](const Moon::Building::FloorPlate& plate) { return RectFitsPlate(plate, stairRect, 0.2f); })) {
        stairRect = overallCoreRect;
        stairRect.size[0] = std::min(overallCoreRect.size[0], std::max(2.0f, overallCoreRect.size[0] * 0.28f));
    }

    if (!std::all_of(outBuilding.floorPlates.begin(), outBuilding.floorPlates.end(),
        [&](const Moon::Building::FloorPlate& plate) { return RectFitsPlate(plate, elevatorRect, 0.2f); })) {
        elevatorRect = overallCoreRect;
        elevatorRect.size = {
            std::min(overallCoreRect.size[0], std::max(2.4f, overallCoreRect.size[0] * 0.36f)),
            std::min(overallCoreRect.size[1], std::max(2.8f, overallCoreRect.size[1] * 0.36f))
        };
        elevatorRect.origin = {
            center[0] - elevatorRect.size[0] * 0.5f,
            center[1] - elevatorRect.size[1] * 0.5f
        };
    }

    if (!std::all_of(outBuilding.floorPlates.begin(), outBuilding.floorPlates.end(),
        [&](const Moon::Building::FloorPlate& plate) { return RectFitsPlate(plate, serviceRect, 0.2f); })) {
        serviceRect = overallCoreRect;
        serviceRect.size[0] = std::max(1.5f, overallCoreRect.size[0] * 0.24f);
        serviceRect.origin[0] = overallCoreRect.origin[0] + overallCoreRect.size[0] - serviceRect.size[0];
    }

    Moon::Building::VerticalCore stairCore;
    stairCore.coreId = "mass_core_stair";
    stairCore.type = Moon::Building::VerticalCoreType::Stair;
    stairCore.rect = stairRect;
    stairCore.floorFrom = 0;
    stairCore.floorTo = maxFloorLevel;
    outBuilding.verticalCores.push_back(stairCore);

    Moon::Building::VerticalCore elevatorCore;
    elevatorCore.coreId = "mass_core_elevator";
    elevatorCore.type = Moon::Building::VerticalCoreType::Elevator;
    elevatorCore.rect = elevatorRect;
    elevatorCore.floorFrom = 0;
    elevatorCore.floorTo = maxFloorLevel;
    outBuilding.verticalCores.push_back(elevatorCore);

    Moon::Building::VerticalCore serviceCore;
    serviceCore.coreId = "mass_core_service";
    serviceCore.type = Moon::Building::VerticalCoreType::Service;
    serviceCore.rect = serviceRect;
    serviceCore.floorFrom = 0;
    serviceCore.floorTo = maxFloorLevel;
    outBuilding.verticalCores.push_back(serviceCore);
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

    if (!GenerateStructuralPlan(workingDefinition, outBuilding)) {
        outError = "Structural planning failed";
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

bool BuildingPipeline::GenerateStructuralPlan(const BuildingDefinition& definition,
                                             GeneratedBuilding& outBuilding) {
    outBuilding.floorPlates.clear();
    outBuilding.verticalCores.clear();
    outBuilding.supportColumns.clear();
    outBuilding.programBlocks.clear();

    std::vector<FloorPlate> slicedFloorPlates;
    std::string sliceError;
    const bool hasMassingRule = std::any_of(definition.masses.begin(), definition.masses.end(),
        [](const Mass& mass) { return !mass.massingRuleAsset.empty(); });
    if (hasMassingRule) {
        m_massFloorPlateGenerator.Generate(definition, slicedFloorPlates, sliceError);
    }

    int maxFloorLevel = 0;
    for (const auto& floor : definition.floors) {
        maxFloorLevel = std::max(maxFloorLevel, floor.level);
    }

    for (const auto& floor : definition.floors) {
        const Mass* mass = FindMassForFloor(definition, floor);
        if (!mass) {
            continue;
        }

        FloorPlate plate;
        auto slicedPlateIt = std::find_if(slicedFloorPlates.begin(), slicedFloorPlates.end(),
            [&](const FloorPlate& candidate) {
                return candidate.floorLevel == floor.level && candidate.massId == floor.massId;
            });
        if (slicedPlateIt != slicedFloorPlates.end()) {
            plate = *slicedPlateIt;
        } else {
            plate.floorLevel = floor.level;
            plate.massId = floor.massId;
            plate.origin = mass->origin;
            plate.size = mass->size;
        }

        std::vector<Rect> exclusionRects;
        for (const auto& space : floor.spaces) {
            for (const auto& rect : space.rects) {
                const VerticalCoreType coreType = ClassifyCoreType(rect);
                const bool explicitCore = ContainsToken(rect.rectId, "core") ||
                                          ContainsToken(rect.rectId, "elevator") ||
                                          ContainsToken(rect.rectId, "stair");
                if (explicitCore && !hasMassingRule) {
                    VerticalCore core;
                    core.coreId = rect.rectId;
                    core.type = coreType;
                    core.rect = rect;
                    core.floorFrom = floor.level;
                    core.floorTo = floor.level;
                    if (space.properties.hasStairs) {
                        core.floorTo = std::max(core.floorTo, space.stairsConfig.connectToLevel);
                    } else if (core.type == VerticalCoreType::Elevator && floor.level < maxFloorLevel) {
                        core.floorTo = floor.level + 1;
                    }
                    outBuilding.verticalCores.push_back(core);
                    exclusionRects.push_back(rect);
                }
            }

            Rect inferredVoid;
            if (InferMallVoidFromCorridor(space, inferredVoid)) {
                plate.voids.push_back(inferredVoid);
                exclusionRects.push_back(inferredVoid);
            }
        }

        outBuilding.floorPlates.push_back(plate);

        const float spacing = (definition.style.category == "commercial" || definition.style.category == "retail")
            ? 8.0f : 6.0f;
        const float columnSize = (definition.style.category == "commercial" || definition.style.category == "retail")
            ? 0.55f : 0.4f;

        for (float x = plate.origin[0] + spacing * 0.5f;
             x < plate.origin[0] + plate.size[0] - spacing * 0.25f;
             x += spacing) {
            for (float y = plate.origin[1] + spacing * 0.5f;
                 y < plate.origin[1] + plate.size[1] - spacing * 0.25f;
                 y += spacing) {
                GridPos2D center = {x, y};
                bool blocked = false;
                for (const auto& exclusion : exclusionRects) {
                    if (RectContainsPoint(exclusion, center)) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked) {
                    continue;
                }

                const int keyX = static_cast<int>(std::round(center[0] * 100.0f));
                const int keyY = static_cast<int>(std::round(center[1] * 100.0f));
                const std::string columnId = "column_" + std::to_string(keyX) + "_" + std::to_string(keyY);
                auto existing = std::find_if(outBuilding.supportColumns.begin(), outBuilding.supportColumns.end(),
                    [&](const SupportColumn& column) { return column.columnId == columnId; });
                if (existing != outBuilding.supportColumns.end()) {
                    existing->floorFrom = std::min(existing->floorFrom, 0);
                    existing->floorTo = std::max(existing->floorTo, floor.level);
                    continue;
                }

                SupportColumn column;
                column.columnId = columnId;
                column.center = center;
                column.width = columnSize;
                column.depth = columnSize;
                column.floorFrom = 0;
                column.floorTo = floor.level;
                outBuilding.supportColumns.push_back(column);
            }
        }
    }

    if (hasMassingRule) {
        AddMassDrivenCoreIfNeeded(definition, maxFloorLevel, outBuilding);
        GenerateMassDrivenProgramBlocks(definition, outBuilding);
    }

    return !outBuilding.floorPlates.empty();
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
