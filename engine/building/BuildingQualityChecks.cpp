#include "BuildingQualityChecks.h"

#include "BuildingElementRules.h"
#include "BuildingGeometryUtils.h"
#include "BuildingTypology.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace Moon {
namespace Building {

namespace {

float ComputePolygonArea(const std::vector<GridPos2D>& outline) {
    if (outline.size() < 3) {
        return 0.0f;
    }

    float area = 0.0f;
    for (size_t i = 0; i < outline.size(); ++i) {
        const auto& a = outline[i];
        const auto& b = outline[(i + 1) % outline.size()];
        area += a[0] * b[1] - b[0] * a[1];
    }
    return std::abs(area) * 0.5f;
}

float ComputeRectArea(const Rect& rect) {
    return std::max(0.0f, rect.size[0]) * std::max(0.0f, rect.size[1]);
}

float ComputeFloorPlateArea(const FloorPlate& plate) {
    float area = 0.0f;
    if (!plate.outline.empty()) {
        area = ComputePolygonArea(plate.outline);
    } else if (!plate.envelopeOutline.empty()) {
        area = ComputePolygonArea(plate.envelopeOutline);
    } else {
        area = std::max(0.0f, plate.size[0]) * std::max(0.0f, plate.size[1]);
    }

    for (const auto& voidRect : plate.voids) {
        area -= ComputeRectArea(voidRect);
    }
    return std::max(0.0f, area);
}

const FloorPlate* FindFloorPlate(const GeneratedBuilding& building, int floorLevel) {
    for (const auto& plate : building.floorPlates) {
        if (plate.floorLevel == floorLevel) {
            return &plate;
        }
    }
    return nullptr;
}

const Floor* FindFloor(const BuildingDefinition& definition, int floorLevel) {
    for (const auto& floor : definition.floors) {
        if (floor.level == floorLevel) {
            return &floor;
        }
    }
    return nullptr;
}

bool ContainsText(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

void AddError(BuildingQualityReport& report,
              const std::string& code,
              const std::string& message,
              int floorLevel = -1) {
    report.passed = false;
    report.errors.push_back({code, message, floorLevel});
}

bool IsSkippableConnectivityUsage(SpaceUsage usage) {
    return usage == SpaceUsage::Unknown ||
           usage == SpaceUsage::Storage ||
           usage == SpaceUsage::Garage ||
           usage == SpaceUsage::Balcony ||
           usage == SpaceUsage::Terrace;
}

void CheckFloorPlates(const GeneratedBuilding& building, BuildingQualityReport& report) {
    for (const auto& floor : building.definition.floors) {
        const FloorPlate* plate = FindFloorPlate(building, floor.level);
        if (!plate) {
            AddError(report,
                     "missing_floor_plate",
                     "Missing floor plate for generated floor.",
                     floor.level);
            continue;
        }

        const float plateArea = ComputeFloorPlateArea(*plate);
        if (plateArea <= 0.0f) {
            AddError(report,
                     "invalid_floor_plate_area",
                     "Floor plate area must be positive.",
                     floor.level);
            continue;
        }

        float programArea = 0.0f;
        for (const auto& space : floor.spaces) {
            for (const auto& rect : space.rects) {
                programArea += ComputeRectArea(rect);
            }
        }

        if (programArea > plateArea + 1.0f) {
            AddError(report,
                     "program_exceeds_floor_plate",
                     "Resolved program area exceeds usable floor plate area.",
                     floor.level);
        }
    }
}

void CheckVerticalCirculation(const GeneratedBuilding& building, BuildingQualityReport& report) {
    int maxLevel = -1;
    for (const auto& floor : building.definition.floors) {
        maxLevel = std::max(maxLevel, floor.level);
    }
    if (maxLevel <= 0) {
        return;
    }

    for (int level = 0; level < maxLevel; ++level) {
        bool connected = false;
        for (const auto& stair : building.stairs) {
            if (stair.fromLevel <= level && stair.toLevel >= level + 1) {
                connected = true;
                break;
            }
        }
        if (!connected) {
            for (const auto& transport : building.verticalTransports) {
                if (transport.floorFrom <= level && transport.floorTo >= level + 1) {
                    connected = true;
                    break;
                }
            }
        }

        if (!connected) {
            AddError(report,
                     "missing_vertical_circulation",
                     "No stair or vertical transport connects consecutive floors.",
                     level);
        }
    }
}

void CheckConnectivity(const GeneratedBuilding& building, BuildingQualityReport& report) {
    std::unordered_map<int, int> degreeBySpace;
    for (const auto& connection : building.connections) {
        ++degreeBySpace[connection.spaceA];
        ++degreeBySpace[connection.spaceB];
    }

    for (const auto& floor : building.definition.floors) {
        int relevantSpaces = 0;
        int connectedSpaces = 0;
        int circulationSpaces = 0;
        std::unordered_set<int> relevantSpaceIds;

        for (const auto& space : floor.spaces) {
            if (IsSkippableConnectivityUsage(space.properties.usage) || space.properties.isOutdoor) {
                continue;
            }
            ++relevantSpaces;
            if (space.properties.usage == SpaceUsage::Corridor ||
                space.properties.usage == SpaceUsage::Entrance) {
                ++circulationSpaces;
            }
            relevantSpaceIds.insert(space.spaceId);

            if (degreeBySpace[space.spaceId] > 0) {
                ++connectedSpaces;
            }
        }

        bool hasPhysicalPartitions = false;
        for (const auto& wall : building.walls) {
            if (wall.floorLevel != floor.level || wall.neighborSpaceId < 0) {
                continue;
            }
            if (relevantSpaceIds.count(wall.spaceId) && relevantSpaceIds.count(wall.neighborSpaceId)) {
                hasPhysicalPartitions = true;
                break;
            }
        }
        if (!hasPhysicalPartitions) {
            for (const auto& door : building.doors) {
                if (door.floorLevel != floor.level) {
                    continue;
                }
                if (relevantSpaceIds.count(door.spaceA) && relevantSpaceIds.count(door.spaceB)) {
                    hasPhysicalPartitions = true;
                    break;
                }
            }
        }

        const bool isSingleCirculationPair = relevantSpaces == 2 && circulationSpaces == 1;
        if (relevantSpaces > 1 && connectedSpaces == 0 && hasPhysicalPartitions && !isSingleCirculationPair) {
            AddError(report,
                     "disconnected_floor_graph",
                     "Floor has multiple spaces but no meaningful generated connectivity.",
                     floor.level);
        }
    }
}

void CheckWallDoorWindowReferences(const GeneratedBuilding& building, BuildingQualityReport& report) {
    std::unordered_set<int> wallIds;
    for (const auto& wall : building.walls) {
        wallIds.insert(wall.wallId);
    }

    std::unordered_set<int> spaceIds;
    for (const auto& floor : building.definition.floors) {
        for (const auto& space : floor.spaces) {
            spaceIds.insert(space.spaceId);
        }
    }

    for (const auto& door : building.doors) {
        if (!wallIds.count(door.wallId)) {
            AddError(report,
                     "door_missing_wall",
                     "Door references a missing wall.",
                     door.floorLevel);
        }
        if (!spaceIds.count(door.spaceA) || (door.spaceB >= 0 && !spaceIds.count(door.spaceB))) {
            AddError(report,
                     "door_missing_space",
                     "Door references a missing space.",
                     door.floorLevel);
        }
    }

    for (const auto& window : building.windows) {
        if (window.wallId < 0) {
            AddError(report,
                     "window_missing_wall",
                     "Window is missing a host wall assignment.",
                     window.floorLevel);
        } else if (!wallIds.count(window.wallId)) {
            AddError(report,
                     "window_missing_wall",
                     "Window references a missing wall.",
                     window.floorLevel);
        }
        if (!spaceIds.count(window.spaceId)) {
            AddError(report,
                     "window_missing_space",
                     "Window references a missing space.",
                     window.floorLevel);
        }
    }
}

void CheckColumnsAgainstVerticalShafts(const GeneratedBuilding& building, BuildingQualityReport& report) {
    for (const auto& column : building.supportColumns) {
        for (const auto& transport : building.verticalTransports) {
            if (column.floorTo < transport.floorFrom || column.floorFrom > transport.floorTo) {
                continue;
            }

            if (RectContainsPoint(transport.shaftRect, column.center)) {
                AddError(report,
                         "column_inside_shaft",
                         "Support column is placed inside a vertical transport shaft.",
                         transport.floorFrom);
            }
        }
    }
}

BuildingTypology InferTypologyFromResolvedDefinition(const GeneratedBuilding& building) {
    const BuildingDefinition& definition = building.definition;
    int residentialSignals = 0;
    int officeSignals = 0;
    int retailSignals = 0;

    for (const auto& floor : definition.floors) {
        for (const auto& space : floor.spaces) {
            switch (space.properties.usage) {
                case SpaceUsage::Bedroom:
                case SpaceUsage::Living:
                case SpaceUsage::Kitchen:
                case SpaceUsage::Bathroom:
                    ++residentialSignals;
                    break;
                case SpaceUsage::Office:
                    ++officeSignals;
                    break;
                case SpaceUsage::Corridor:
                    break;
                default:
                    break;
            }
        }
    }

    if (ContainsText(definition.style.category, "retail") ||
        ContainsText(definition.style.facade, "retail")) {
        retailSignals += 4;
    }
    if (ContainsText(definition.style.category, "commercial") ||
        ContainsText(definition.style.category, "office")) {
        officeSignals += 1;
    }
    if (ContainsText(definition.style.category, "urban") ||
        ContainsText(definition.style.category, "residential")) {
        residentialSignals += 1;
    }

    if (retailSignals >= 4) {
        return BuildingTypology::Retail;
    }
    if (officeSignals > residentialSignals) {
        return BuildingTypology::Office;
    }
    if (residentialSignals > 0) {
        return BuildingTypology::Residential;
    }
    return BuildingTypology::Unknown;
}

void CheckTypologySpecificSignals(const GeneratedBuilding& building, BuildingQualityReport& report) {
    const BuildingTypology typology = InferTypologyFromResolvedDefinition(building);

    if (IsOfficeTypology(typology)) {
        for (const auto& floor : building.definition.floors) {
            const bool hasCore = std::any_of(floor.spaces.begin(), floor.spaces.end(), [](const Space& space) {
                return space.properties.usage == SpaceUsage::Stairwell;
            }) || std::any_of(building.verticalTransports.begin(), building.verticalTransports.end(), [&](const VerticalTransport& transport) {
                return transport.floorFrom <= floor.level && transport.floorTo >= floor.level;
            }) || std::any_of(building.verticalCores.begin(), building.verticalCores.end(), [&](const VerticalCore& core) {
                return core.floorFrom <= floor.level && core.floorTo >= floor.level;
            });
            if (!hasCore) {
                report.warnings.push_back({"office_floor_missing_core",
                                           "Office floor is missing a clear core/stairwell signal.",
                                           floor.level});
            }
        }
    }

    if (IsRetailTypology(typology)) {
        for (const auto& floor : building.definition.floors) {
            bool hasCorridor = false;
            bool hasVoidLikePlate = false;
            for (const auto& space : floor.spaces) {
                if (space.properties.usage == SpaceUsage::Corridor) {
                    hasCorridor = true;
                }
            }
            const FloorPlate* plate = FindFloorPlate(building, floor.level);
            hasVoidLikePlate = plate != nullptr && !plate->voids.empty();

            if (!hasCorridor) {
                report.warnings.push_back({"retail_floor_missing_circulation",
                                           "Retail floor is missing circulation space.",
                                           floor.level});
            }
            if (!hasVoidLikePlate) {
                report.warnings.push_back({"retail_floor_missing_void",
                                           "Retail floor is missing a floor plate void/atrium signal.",
                                           floor.level});
            }
        }
    }
}

} // namespace

bool BuildingQualityReport::HasErrorCode(const std::string& code) const {
    return std::any_of(errors.begin(), errors.end(), [&](const BuildingQualityIssue& issue) {
        return issue.code == code;
    });
}

BuildingQualityReport EvaluateBuildingQuality(const GeneratedBuilding& building) {
    BuildingQualityReport report;
    CheckFloorPlates(building, report);
    CheckVerticalCirculation(building, report);
    CheckConnectivity(building, report);
    CheckWallDoorWindowReferences(building, report);
    AppendElementRuleViolations(building, report);
    CheckColumnsAgainstVerticalShafts(building, report);
    CheckTypologySpecificSignals(building, report);
    return report;
}

} // namespace Building
} // namespace Moon
