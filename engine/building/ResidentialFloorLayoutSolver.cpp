#include "ResidentialFloorLayoutSolver.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace {

using namespace Moon::Building;

std::string ToLowerCopy(const std::string& value) {
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowered;
}

bool IsCoreSemanticType(const std::string& type) {
    const std::string lowered = ToLowerCopy(type);
    return lowered == "core" || lowered == "stairs" || lowered == "elevator" || lowered == "mechanical";
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

Rect MergeCoreEnvelope(const std::vector<VerticalCore>& cores) {
    Rect merged;
    const auto stairIt = std::find_if(cores.begin(), cores.end(),
        [](const VerticalCore& core) { return core.type == VerticalCoreType::Stair; });
    const auto elevatorIt = std::find_if(cores.begin(), cores.end(),
        [](const VerticalCore& core) { return core.type == VerticalCoreType::Elevator; });
    if (stairIt == cores.end() || elevatorIt == cores.end()) {
        return merged;
    }

    const float minX = std::min(stairIt->rect.origin[0], elevatorIt->rect.origin[0]);
    const float minY = std::min(stairIt->rect.origin[1], elevatorIt->rect.origin[1]);
    const float maxX = std::max(stairIt->rect.origin[0] + stairIt->rect.size[0],
                                elevatorIt->rect.origin[0] + elevatorIt->rect.size[0]);
    const float maxY = std::max(stairIt->rect.origin[1] + stairIt->rect.size[1],
                                elevatorIt->rect.origin[1] + elevatorIt->rect.size[1]);
    merged.origin = {minX, minY};
    merged.size = {maxX - minX, maxY - minY};
    return merged;
}

Rect FitRectToArea(const Rect& source, float targetArea, float minWidth) {
    Rect rect = source;
    const float sourceArea = std::max(0.001f, source.size[0] * source.size[1]);
    if (targetArea >= sourceArea - 0.25f) {
        return rect;
    }

    if (source.size[0] >= source.size[1]) {
        rect.size[0] = std::max(minWidth, std::min(source.size[0], targetArea / std::max(source.size[1], 0.5f)));
    } else {
        rect.size[1] = std::max(minWidth, std::min(source.size[1], targetArea / std::max(source.size[0], 0.5f)));
    }
    return rect;
}

ResolvedSpacePlan MakeResolvedSpace(const std::string& spaceId,
                                    SpaceUsage usage,
                                    const Rect& rect,
                                    float ceilingHeight,
                                    int floorLevel) {
    ResolvedSpacePlan space;
    space.spaceId = spaceId;
    space.usage = usage;
    space.rect = rect;
    space.rect.rectId = spaceId;
    space.ceilingHeight = ceilingHeight > 0.0f ? ceilingHeight : 3.0f;
    space.hasStairs = (usage == SpaceUsage::Stairwell);
    space.stairConnectToLevel = floorLevel + 1;
    return space;
}

void AddDebugBlock(const ResolvedSpacePlan& space,
                   int floorLevel,
                   std::vector<ProgramBlock>& outDebugBlocks) {
    ProgramBlock block;
    block.blockId = space.spaceId;
    block.floorLevel = floorLevel;
    block.usage = space.usage;
    block.rect = space.rect;
    outDebugBlocks.push_back(block);
}

} // namespace

namespace Moon {
namespace Building {

bool ResidentialFloorLayoutSolver::GenerateFloor(const BuildingDefinition& definition,
                                                 const FloorLayoutInput& layoutInput,
                                                 const FloorPlate& floorPlate,
                                                 const std::vector<VerticalCore>& floorCores,
                                                 ResolvedFloorLayout& outResolvedFloor,
                                                 std::string& outError) const {
    outResolvedFloor.level = layoutInput.level;
    outResolvedFloor.spaces.clear();
    outResolvedFloor.debugBlocks.clear();

    const Rect combinedCore = MergeCoreEnvelope(floorCores);
    if (combinedCore.size[0] <= 0.0f || combinedCore.size[1] <= 0.0f) {
        outError = "Residential floor layout requires a valid stair/elevator core envelope";
        return false;
    }

    const float corridorWidth = std::max(1.8f, definition.grid * 3.0f);
    Rect corridorRect;
    corridorRect.rectId = "residential_corridor";
    corridorRect.origin = {
        combinedCore.origin[0] - corridorWidth,
        combinedCore.origin[1]
    };
    corridorRect.size = {
        combinedCore.size[0] + corridorWidth * 2.0f,
        combinedCore.size[1]
    };

    const float margin = std::max(0.2f, definition.grid * 0.5f);
    if (!RectFitsPlate(floorPlate, corridorRect, margin)) {
        corridorRect.origin = {
            combinedCore.origin[0],
            combinedCore.origin[1] - corridorWidth
        };
        corridorRect.size = {
            combinedCore.size[0],
            combinedCore.size[1] + corridorWidth * 2.0f
        };
    }

    Rect leftUnitBand;
    leftUnitBand.rectId = "residential_unit_left";
    leftUnitBand.origin = {floorPlate.origin[0] + margin, floorPlate.origin[1] + margin};
    leftUnitBand.size = {
        std::max(4.5f, corridorRect.origin[0] - leftUnitBand.origin[0] - definition.grid),
        std::max(5.0f, floorPlate.size[1] - margin * 2.0f)
    };

    Rect rightUnitBand;
    rightUnitBand.rectId = "residential_unit_right";
    rightUnitBand.origin = {
        corridorRect.origin[0] + corridorRect.size[0] + definition.grid,
        floorPlate.origin[1] + margin
    };
    rightUnitBand.size = {
        std::max(4.5f, floorPlate.origin[0] + floorPlate.size[0] - rightUnitBand.origin[0] - margin),
        std::max(5.0f, floorPlate.size[1] - margin * 2.0f)
    };

    std::vector<Rect> unitBands;
    if (RectFitsPlate(floorPlate, leftUnitBand, margin)) {
        unitBands.push_back(leftUnitBand);
    }
    if (RectFitsPlate(floorPlate, rightUnitBand, margin)) {
        unitBands.push_back(rightUnitBand);
    }

    size_t nextUnitBand = 0;
    bool synthesizedAny = false;

    for (const auto& semanticSpace : layoutInput.spaces) {
        if (IsCoreSemanticType(semanticSpace.type)) {
            const auto usage = StringToSpaceUsage(semanticSpace.type);
            const VerticalCore* matchedCore = nullptr;
            if (semanticSpace.type == "elevator") {
                auto it = std::find_if(floorCores.begin(), floorCores.end(),
                    [](const VerticalCore& core) { return core.type == VerticalCoreType::Elevator; });
                if (it != floorCores.end()) {
                    matchedCore = &(*it);
                }
            } else {
                auto it = std::find_if(floorCores.begin(), floorCores.end(),
                    [](const VerticalCore& core) { return core.type == VerticalCoreType::Stair; });
                if (it == floorCores.end()) {
                    it = std::find_if(floorCores.begin(), floorCores.end(),
                        [](const VerticalCore& core) { return core.type == VerticalCoreType::Service; });
                }
                if (it != floorCores.end()) {
                    matchedCore = &(*it);
                }
            }

            if (matchedCore) {
                outResolvedFloor.spaces.push_back(MakeResolvedSpace(
                    semanticSpace.spaceId,
                    usage,
                    matchedCore->rect,
                    semanticSpace.constraints.ceilingHeight,
                    layoutInput.level));
                synthesizedAny = true;
            }
            continue;
        }

        const SpaceUsage usage = StringToSpaceUsage(semanticSpace.type);
        if (usage == SpaceUsage::Unknown) {
            continue;
        }

        if ((usage == SpaceUsage::Corridor || usage == SpaceUsage::Entrance) &&
            RectFitsPlate(floorPlate, corridorRect, margin)) {
            ResolvedSpacePlan space = MakeResolvedSpace(
                semanticSpace.spaceId,
                usage,
                corridorRect,
                semanticSpace.constraints.ceilingHeight,
                layoutInput.level);
            AddDebugBlock(space, layoutInput.level, outResolvedFloor.debugBlocks);
            outResolvedFloor.spaces.push_back(std::move(space));
            synthesizedAny = true;
            continue;
        }

        if ((usage == SpaceUsage::Living || usage == SpaceUsage::Bedroom || usage == SpaceUsage::Kitchen ||
             usage == SpaceUsage::Dining || usage == SpaceUsage::Bathroom) &&
            nextUnitBand < unitBands.size()) {
            Rect candidate = unitBands[nextUnitBand++];
            candidate = FitRectToArea(candidate,
                std::max(10.0f, semanticSpace.areaPreferred),
                std::max(2.4f, semanticSpace.constraints.minWidth));
            candidate.rectId = semanticSpace.spaceId;
            if (!RectFitsPlate(floorPlate, candidate, margin)) {
                continue;
            }

            ResolvedSpacePlan space = MakeResolvedSpace(
                semanticSpace.spaceId,
                usage,
                candidate,
                semanticSpace.constraints.ceilingHeight,
                layoutInput.level);
            AddDebugBlock(space, layoutInput.level, outResolvedFloor.debugBlocks);
            outResolvedFloor.spaces.push_back(std::move(space));
            synthesizedAny = true;
        }
    }

    if (!synthesizedAny) {
        outError = "Failed to synthesize residential floor spaces inside sliced floor plates";
        return false;
    }

    return true;
}

} // namespace Building
} // namespace Moon
