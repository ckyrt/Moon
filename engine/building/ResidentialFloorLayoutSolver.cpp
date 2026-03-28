#include "ResidentialFloorLayoutSolver.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

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

bool IsResidentialUnitUsage(SpaceUsage usage) {
    return usage == SpaceUsage::Living ||
           usage == SpaceUsage::Bedroom ||
           usage == SpaceUsage::Kitchen ||
           usage == SpaceUsage::Dining ||
           usage == SpaceUsage::Bathroom ||
           usage == SpaceUsage::Office ||
           usage == SpaceUsage::Closet ||
           usage == SpaceUsage::Storage;
}

int RoomPriority(const SemanticSpace& space) {
    const SpaceUsage usage = StringToSpaceUsage(space.type);
    switch (usage) {
        case SpaceUsage::Living: return 0;
        case SpaceUsage::Dining: return 1;
        case SpaceUsage::Kitchen: return 2;
        case SpaceUsage::Bedroom: return 3;
        case SpaceUsage::Office: return 4;
        case SpaceUsage::Bathroom: return 5;
        case SpaceUsage::Closet: return 6;
        case SpaceUsage::Storage: return 7;
        default: return 8;
    }
}

struct UnitGroup {
    std::string unitId;
    std::vector<const SemanticSpace*> spaces;
};

std::vector<UnitGroup> CollectUnitGroups(const FloorLayoutInput& layoutInput) {
    std::vector<UnitGroup> groups;
    std::unordered_map<std::string, size_t> indexByUnitId;
    for (const auto& semanticSpace : layoutInput.spaces) {
        if (semanticSpace.unitId.empty()) {
            continue;
        }

        const SpaceUsage usage = StringToSpaceUsage(semanticSpace.type);
        if (!IsResidentialUnitUsage(usage)) {
            continue;
        }

        auto [it, inserted] = indexByUnitId.emplace(semanticSpace.unitId, groups.size());
        if (inserted) {
            UnitGroup group;
            group.unitId = semanticSpace.unitId;
            groups.push_back(std::move(group));
        }
        groups[it->second].spaces.push_back(&semanticSpace);
    }

    for (auto& group : groups) {
        std::stable_sort(group.spaces.begin(), group.spaces.end(),
            [](const SemanticSpace* a, const SemanticSpace* b) {
                return RoomPriority(*a) < RoomPriority(*b);
            });
    }
    return groups;
}

std::vector<Rect> SplitBandForRooms(const Rect& band,
                                    const std::vector<const SemanticSpace*>& spaces,
                                    float grid) {
    std::vector<Rect> rects;
    if (spaces.empty() || band.size[0] <= 0.0f || band.size[1] <= 0.0f) {
        return rects;
    }

    const float minDepth = std::max(2.0f, grid * 4.0f);
    std::vector<float> depths;
    depths.reserve(spaces.size());

    float totalDepth = 0.0f;
    for (const auto* space : spaces) {
        const float preferredDepth = std::max(
            minDepth,
            std::max(space->constraints.minWidth, space->areaPreferred / std::max(band.size[0], 2.0f)));
        depths.push_back(preferredDepth);
        totalDepth += preferredDepth;
    }

    if (totalDepth > band.size[1]) {
        const float adjustableDepth = std::max(0.001f, totalDepth - minDepth * static_cast<float>(spaces.size()));
        const float targetAdjustableDepth = std::max(0.0f, band.size[1] - minDepth * static_cast<float>(spaces.size()));
        for (size_t i = 0; i < depths.size(); ++i) {
            const float extraDepth = std::max(0.0f, depths[i] - minDepth);
            const float scaledExtra = extraDepth * (targetAdjustableDepth / adjustableDepth);
            depths[i] = minDepth + scaledExtra;
        }
    } else if (totalDepth < band.size[1]) {
        depths.back() += band.size[1] - totalDepth;
    }

    float cursorY = band.origin[1];
    for (size_t i = 0; i < spaces.size(); ++i) {
        Rect rect;
        rect.rectId = spaces[i]->spaceId;
        rect.origin = {band.origin[0], cursorY};
        rect.size = {
            band.size[0],
            i + 1 == spaces.size() ? band.origin[1] + band.size[1] - cursorY : depths[i]
        };
        cursorY += rect.size[1];
        rects.push_back(rect);
    }

    return rects;
}

Rect MergeCoreEnvelope(const std::vector<VerticalCore>& cores) {
    Rect merged;
    if (cores.empty()) {
        return merged;
    }

    float minX = cores.front().rect.origin[0];
    float minY = cores.front().rect.origin[1];
    float maxX = cores.front().rect.origin[0] + cores.front().rect.size[0];
    float maxY = cores.front().rect.origin[1] + cores.front().rect.size[1];
    for (const auto& core : cores) {
        minX = std::min(minX, core.rect.origin[0]);
        minY = std::min(minY, core.rect.origin[1]);
        maxX = std::max(maxX, core.rect.origin[0] + core.rect.size[0]);
        maxY = std::max(maxY, core.rect.origin[1] + core.rect.size[1]);
    }
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

float DetermineStairRotationDegrees(const Rect& rect) {
    return rect.size[0] >= rect.size[1] ? 90.0f : 0.0f;
}

float EstimateStairRunLength(const BuildingDefinition& definition, int fromLevel, int toLevel) {
    if (toLevel <= fromLevel) {
        return 0.0f;
    }

    float totalHeight = 0.0f;
    for (const auto& floor : definition.floors) {
        if (floor.level >= fromLevel && floor.level < toLevel) {
            totalHeight += floor.floorHeight;
        }
    }

    const int stepCount = static_cast<int>(std::ceil(totalHeight / 0.18f));
    return std::max(0, stepCount) * 0.28f;
}

StairType DetermineStairType(const BuildingDefinition& definition,
                             int floorLevel,
                             int connectToLevel,
                             const Rect& rect) {
    const float runLength = EstimateStairRunLength(definition, floorLevel, connectToLevel);
    const float availableRun = std::max(rect.size[0], rect.size[1]);
    if (runLength <= std::max(0.0f, availableRun - 0.4f)) {
        return StairType::Straight;
    }

    const float halfRun = runLength * 0.5f;
    if (halfRun <= std::max(0.0f, availableRun - 0.4f)) {
        return StairType::U;
    }

    return StairType::L;
}

StairType ParseStairType(const std::string& stairForm) {
    if (stairForm == "u") {
        return StairType::U;
    }
    if (stairForm == "l") {
        return StairType::L;
    }
    if (stairForm == "spiral") {
        return StairType::Spiral;
    }
    return StairType::Straight;
}

Rect MergeTransportEnvelope(const std::vector<SemanticVerticalSystem>& systems,
                            bool includeExternal) {
    Rect merged;
    bool hasAny = false;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    for (const auto& system : systems) {
        if (!includeExternal && system.placement == "external") {
            continue;
        }
        if (system.shaftRect.size[0] <= 0.0f || system.shaftRect.size[1] <= 0.0f) {
            continue;
        }
        if (!hasAny) {
            minX = system.shaftRect.origin[0];
            minY = system.shaftRect.origin[1];
            maxX = system.shaftRect.origin[0] + system.shaftRect.size[0];
            maxY = system.shaftRect.origin[1] + system.shaftRect.size[1];
            hasAny = true;
        } else {
            minX = std::min(minX, system.shaftRect.origin[0]);
            minY = std::min(minY, system.shaftRect.origin[1]);
            maxX = std::max(maxX, system.shaftRect.origin[0] + system.shaftRect.size[0]);
            maxY = std::max(maxY, system.shaftRect.origin[1] + system.shaftRect.size[1]);
        }
    }

    if (!hasAny) {
        return merged;
    }

    merged.origin = {minX, minY};
    merged.size = {maxX - minX, maxY - minY};
    return merged;
}

ResolvedVerticalTransportPlan MakeResolvedVerticalTransport(const SemanticVerticalSystem& system) {
    ResolvedVerticalTransportPlan transport;
    transport.transportId = system.id;
    transport.type = system.type == "elevator" ? VerticalTransportType::Elevator : VerticalTransportType::Stair;
    transport.shaftRect = system.shaftRect;
    transport.floorFrom = system.floorFrom;
    transport.floorTo = system.floorTo;
    transport.sourceFloorLevel = system.floorFrom;
    transport.continuousShaft = transport.type == VerticalTransportType::Elevator || system.type == "stairwell";
    transport.enclosed = system.mode != "open";
    transport.external = system.placement == "external";
    transport.stairType = ParseStairType(system.stairForm);
    transport.width = std::min(system.shaftRect.size[0], system.shaftRect.size[1]);
    transport.position = transport.type == VerticalTransportType::Elevator
        ? GridPos2D{system.shaftRect.origin[0] + system.shaftRect.size[0] * 0.5f,
                    system.shaftRect.origin[1] + system.shaftRect.size[1] * 0.5f}
        : GridPos2D{system.shaftRect.origin[0] + system.shaftRect.size[0] * 0.5f,
                    system.shaftRect.origin[1] + 0.25f};
    transport.rotationDegrees = DetermineStairRotationDegrees(system.shaftRect);
    return transport;
}

ResolvedSpacePlan MakeResolvedSpace(const std::string& spaceId,
                                    SpaceUsage usage,
                                    const Rect& rect,
                                    float ceilingHeight,
                                    int floorLevel,
                                    bool hasStairs = false,
                                    int stairConnectToLevel = -1,
                                    float stairWidth = 0.0f,
                                    const GridPos2D& stairPosition = {0.0f, 0.0f},
                                    StairType stairType = StairType::Straight) {
    ResolvedSpacePlan space;
    space.spaceId = spaceId;
    space.usage = usage;
    space.rect = rect;
    space.rect.rectId = spaceId;
    space.ceilingHeight = ceilingHeight > 0.0f ? ceilingHeight : 3.0f;
    space.hasStairs = hasStairs;
    space.stairConnectToLevel = hasStairs ? stairConnectToLevel : -1;
    space.stairWidth = stairWidth > 0.0f ? stairWidth : rect.size[0];
    space.stairPosition = stairPosition;
    space.stairType = stairType;
    space.stairRotationDegrees = DetermineStairRotationDegrees(rect);
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
    outResolvedFloor.verticalTransports.clear();
    outResolvedFloor.debugBlocks.clear();

    std::unordered_set<std::string> emittedTransports;
    for (const auto& system : layoutInput.verticalSystems) {
        if (!system.id.empty() && emittedTransports.insert(system.id).second) {
            outResolvedFloor.verticalTransports.push_back(MakeResolvedVerticalTransport(system));
        }
    }

    Rect combinedCore = MergeCoreEnvelope(floorCores);
    if (combinedCore.size[0] <= 0.0f || combinedCore.size[1] <= 0.0f) {
        combinedCore = MergeTransportEnvelope(layoutInput.verticalSystems, false);
    }
    if (combinedCore.size[0] <= 0.0f || combinedCore.size[1] <= 0.0f) {
        const GridPos2D plateCenter = {
            floorPlate.origin[0] + floorPlate.size[0] * 0.5f,
            floorPlate.origin[1] + floorPlate.size[1] * 0.5f
        };
        combinedCore.origin = {
            plateCenter[0] - std::max(1.2f, definition.grid * 2.0f),
            plateCenter[1] - std::max(2.0f, definition.grid * 3.0f)
        };
        combinedCore.size = {
            std::max(2.4f, definition.grid * 4.0f),
            std::max(4.0f, definition.grid * 6.0f)
        };
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
    const std::vector<UnitGroup> unitGroups = CollectUnitGroups(layoutInput);
    std::unordered_map<std::string, size_t> unitBandAssignments;
    for (size_t i = 0; i < unitGroups.size() && i < unitBands.size(); ++i) {
        unitBandAssignments.emplace(unitGroups[i].unitId, i);
    }

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
                const bool connectsUp = semanticSpace.constraints.connectsToFloor >= 0 &&
                    semanticSpace.constraints.connectsToFloor != layoutInput.level;
                const bool shouldGenerateStair = semanticSpace.type == "core" || semanticSpace.type == "stairs";
                outResolvedFloor.spaces.push_back(MakeResolvedSpace(
                    semanticSpace.spaceId,
                    usage,
                    matchedCore->rect,
                    semanticSpace.constraints.ceilingHeight,
                    layoutInput.level,
                    shouldGenerateStair && connectsUp,
                    semanticSpace.constraints.connectsToFloor,
                    matchedCore->rect.size[0],
                    matchedCore->rect.origin,
                    DetermineStairType(definition,
                                       layoutInput.level,
                                       semanticSpace.constraints.connectsToFloor,
                                       matchedCore->rect)));
                synthesizedAny = true;
            }
            continue;
        }

        const SpaceUsage usage = StringToSpaceUsage(semanticSpace.type);
        if (usage == SpaceUsage::Unknown) {
            continue;
        }

        if (!semanticSpace.unitId.empty() && IsResidentialUnitUsage(usage)) {
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

    for (const auto& unitGroup : unitGroups) {
        const auto assignmentIt = unitBandAssignments.find(unitGroup.unitId);
        if (assignmentIt == unitBandAssignments.end() || assignmentIt->second >= unitBands.size()) {
            continue;
        }

        const Rect& unitBand = unitBands[assignmentIt->second];
        const std::vector<Rect> roomRects = SplitBandForRooms(unitBand, unitGroup.spaces, definition.grid);
        for (size_t i = 0; i < roomRects.size() && i < unitGroup.spaces.size(); ++i) {
            const SemanticSpace& semanticSpace = *unitGroup.spaces[i];
            Rect candidate = FitRectToArea(
                roomRects[i],
                std::max(6.0f, semanticSpace.areaPreferred),
                std::max(2.0f, semanticSpace.constraints.minWidth));
            candidate.rectId = semanticSpace.spaceId;
            if (!RectFitsPlate(floorPlate, candidate, margin)) {
                continue;
            }

            ResolvedSpacePlan space = MakeResolvedSpace(
                semanticSpace.spaceId,
                StringToSpaceUsage(semanticSpace.type),
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
