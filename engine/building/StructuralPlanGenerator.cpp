#include "StructuralPlanGenerator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>

namespace {

using namespace Moon::Building;

std::string ToLowerCopy(const std::string& value) {
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowered;
}

bool ContainsToken(const std::string& value, const char* token) {
    return ToLowerCopy(value).find(token) != std::string::npos;
}

VerticalCoreType ClassifyCoreType(const Rect& rect) {
    const std::string id = ToLowerCopy(rect.rectId);
    if (id.find("elevator") != std::string::npos || id.find("lift") != std::string::npos) {
        return VerticalCoreType::Elevator;
    }
    if (id.find("stair") != std::string::npos) {
        return VerticalCoreType::Stair;
    }
    return VerticalCoreType::Service;
}

bool InferMallVoidFromCorridor(const Space& space, Rect& outVoid) {
    if (space.properties.usage != SpaceUsage::Corridor || space.rects.size() < 4) {
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

GridPos2D ComputePlateCenter(const FloorPlate& plate) {
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

Rect MakeCenteredRect(const GridPos2D& center, float width, float depth) {
    Rect rect;
    rect.origin = {
        center[0] - width * 0.5f,
        center[1] - depth * 0.5f
    };
    rect.size = {width, depth};
    return rect;
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

bool IsCommercialOrOfficeTower(const BuildingDefinition& definition) {
    return definition.style.category == "commercial" || definition.style.category == "retail";
}

void AddMassDrivenCoreIfNeeded(const BuildingDefinition& definition,
                               int maxFloorLevel,
                               GeneratedBuilding& outBuilding) {
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
    const GridPos2D center = {centerX, centerY};
    const float fitMargin = std::max(0.35f, definition.grid * 0.5f);

    bool fitted = false;
    for (int attempts = 0; attempts < 12 && !fitted; ++attempts) {
        const Rect candidate = MakeCenteredRect(center, targetWidth, targetDepth);
        fitted = std::all_of(outBuilding.floorPlates.begin(), outBuilding.floorPlates.end(),
            [&](const FloorPlate& plate) {
                return RectFitsPlate(plate, candidate, fitMargin);
            });
        if (!fitted) {
            targetWidth = std::max(definition.grid * 4.0f, targetWidth - definition.grid);
            targetDepth = std::max(definition.grid * 4.0f, targetDepth - definition.grid);
        }
    }

    Rect overallCoreRect = MakeCenteredRect(center, targetWidth, targetDepth);

    const float circulationGap = definition.grid;
    const float stairWidth = std::clamp(targetWidth * 0.26f, 1.8f, 2.8f);
    const float serviceWidth = std::clamp(targetWidth * 0.24f, 1.5f, 2.4f);
    const float elevatorWidth = std::max(2.4f, targetWidth - stairWidth - serviceWidth - circulationGap * 2.0f);

    Rect stairRect;
    stairRect.rectId = "mass_core_stair";
    stairRect.origin = {overallCoreRect.origin[0], overallCoreRect.origin[1]};
    stairRect.size = {stairWidth, overallCoreRect.size[1]};

    Rect elevatorRect;
    elevatorRect.rectId = "mass_core_elevator";
    elevatorRect.origin = {
        stairRect.origin[0] + stairRect.size[0] + circulationGap,
        overallCoreRect.origin[1] + std::max(0.0f, (overallCoreRect.size[1] - std::max(2.8f, overallCoreRect.size[1] * 0.4f)) * 0.5f)
    };
    elevatorRect.size = {
        std::max(2.4f, elevatorWidth),
        std::max(2.8f, overallCoreRect.size[1] * 0.4f)
    };

    Rect serviceRect;
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
        [&](const FloorPlate& plate) { return RectFitsPlate(plate, stairRect, 0.2f); })) {
        stairRect = overallCoreRect;
        stairRect.size[0] = std::min(overallCoreRect.size[0], std::max(2.0f, overallCoreRect.size[0] * 0.28f));
    }

    if (!std::all_of(outBuilding.floorPlates.begin(), outBuilding.floorPlates.end(),
        [&](const FloorPlate& plate) { return RectFitsPlate(plate, elevatorRect, 0.2f); })) {
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
        [&](const FloorPlate& plate) { return RectFitsPlate(plate, serviceRect, 0.2f); })) {
        serviceRect = overallCoreRect;
        serviceRect.size[0] = std::max(1.5f, overallCoreRect.size[0] * 0.24f);
        serviceRect.origin[0] = overallCoreRect.origin[0] + overallCoreRect.size[0] - serviceRect.size[0];
    }

    VerticalCore stairCore;
    stairCore.coreId = "mass_core_stair";
    stairCore.type = VerticalCoreType::Stair;
    stairCore.rect = stairRect;
    stairCore.floorFrom = 0;
    stairCore.floorTo = maxFloorLevel;
    outBuilding.verticalCores.push_back(stairCore);

    VerticalCore elevatorCore;
    elevatorCore.coreId = "mass_core_elevator";
    elevatorCore.type = VerticalCoreType::Elevator;
    elevatorCore.rect = elevatorRect;
    elevatorCore.floorFrom = 0;
    elevatorCore.floorTo = maxFloorLevel;
    outBuilding.verticalCores.push_back(elevatorCore);

    VerticalCore serviceCore;
    serviceCore.coreId = "mass_core_service";
    serviceCore.type = VerticalCoreType::Service;
    serviceCore.rect = serviceRect;
    serviceCore.floorFrom = 0;
    serviceCore.floorTo = maxFloorLevel;
    outBuilding.verticalCores.push_back(serviceCore);
}

} // namespace

namespace Moon {
namespace Building {

bool StructuralPlanGenerator::Generate(const BuildingDefinition& definition,
                                       const std::vector<FloorPlate>& slicedFloorPlates,
                                       GeneratedBuilding& outBuilding,
                                       std::string& outError) const {
    outBuilding.floorPlates.clear();
    outBuilding.verticalCores.clear();
    outBuilding.supportColumns.clear();
    outBuilding.programBlocks.clear();

    const bool hasMassingRule = std::any_of(definition.masses.begin(), definition.masses.end(),
        [](const Mass& mass) { return !mass.massingRuleAsset.empty(); });

    int maxFloorLevel = 0;
    for (const auto& floor : definition.floors) {
        maxFloorLevel = std::max(maxFloorLevel, floor.level);
    }

    struct PendingColumnPlan {
        FloorPlate plate;
        std::vector<Rect> exclusionRects;
    };

    std::vector<PendingColumnPlan> pendingColumnPlans;

    for (const auto& floor : definition.floors) {
        const Mass* mass = nullptr;
        for (const auto& candidate : definition.masses) {
            if (candidate.massId == floor.massId) {
                mass = &candidate;
                break;
            }
        }
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
        pendingColumnPlans.push_back({plate, exclusionRects});
    }

    if (hasMassingRule) {
        AddMassDrivenCoreIfNeeded(definition, maxFloorLevel, outBuilding);
    }

    const float spacing = (definition.style.category == "commercial" || definition.style.category == "retail")
        ? 8.0f : 6.0f;
    const float columnSize = (definition.style.category == "commercial" || definition.style.category == "retail")
        ? 0.55f : 0.4f;

    for (const auto& plan : pendingColumnPlans) {
        std::vector<Rect> exclusionRects = plan.exclusionRects;
        for (const auto& core : outBuilding.verticalCores) {
            if (core.floorFrom <= plan.plate.floorLevel && core.floorTo >= plan.plate.floorLevel) {
                exclusionRects.push_back(core.rect);
            }
        }

        for (float x = plan.plate.origin[0] + spacing * 0.5f;
             x < plan.plate.origin[0] + plan.plate.size[0] - spacing * 0.25f;
             x += spacing) {
            for (float y = plan.plate.origin[1] + spacing * 0.5f;
                 y < plan.plate.origin[1] + plan.plate.size[1] - spacing * 0.25f;
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
                    existing->floorTo = std::max(existing->floorTo, plan.plate.floorLevel);
                    continue;
                }

                SupportColumn column;
                column.columnId = columnId;
                column.center = center;
                column.width = columnSize;
                column.depth = columnSize;
                column.floorFrom = 0;
                column.floorTo = plan.plate.floorLevel;
                outBuilding.supportColumns.push_back(column);
            }
        }
    }

    if (outBuilding.floorPlates.empty()) {
        outError = "No floor plates were generated for the building definition";
        return false;
    }

    return true;
}

} // namespace Building
} // namespace Moon
