#include "RetailFloorLayoutSolver.h"

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

bool RectFitsPlateBoundary(const FloorPlate& plate, const Rect& rect, float margin) {
    const std::array<GridPos2D, 5> samplePoints = {{
        {rect.origin[0] + margin, rect.origin[1] + margin},
        {rect.origin[0] + rect.size[0] - margin, rect.origin[1] + margin},
        {rect.origin[0] + rect.size[0] - margin, rect.origin[1] + rect.size[1] - margin},
        {rect.origin[0] + margin, rect.origin[1] + rect.size[1] - margin},
        {rect.origin[0] + rect.size[0] * 0.5f, rect.origin[1] + rect.size[1] * 0.5f}
    }};

    for (const auto& point : samplePoints) {
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

bool RectFitsPlate(const FloorPlate& plate, const Rect& rect, float margin) {
    if (!RectFitsPlateBoundary(plate, rect, margin)) {
        return false;
    }

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
    }

    return true;
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

Rect FitRectInsideBounds(const Rect& rect, const Rect& bounds) {
    Rect fitted = rect;
    const float boundsMaxX = bounds.origin[0] + bounds.size[0];
    const float boundsMaxY = bounds.origin[1] + bounds.size[1];

    fitted.origin[0] = std::max(bounds.origin[0], fitted.origin[0]);
    fitted.origin[1] = std::max(bounds.origin[1], fitted.origin[1]);
    fitted.size[0] = std::min(fitted.size[0], boundsMaxX - fitted.origin[0]);
    fitted.size[1] = std::min(fitted.size[1], boundsMaxY - fitted.origin[1]);
    fitted.size[0] = std::max(0.0f, fitted.size[0]);
    fitted.size[1] = std::max(0.0f, fitted.size[1]);
    return fitted;
}

Rect ExpandRect(const Rect& rect, float distance) {
    Rect expanded = rect;
    expanded.origin[0] -= distance;
    expanded.origin[1] -= distance;
    expanded.size[0] += distance * 2.0f;
    expanded.size[1] += distance * 2.0f;
    return expanded;
}

bool IsUsableRect(const Rect& rect, float minSize = 1.5f) {
    return rect.size[0] >= minSize && rect.size[1] >= minSize;
}

Rect ComputeSafeInteriorRect(const FloorPlate& plate, float margin) {
    const auto center = ComputePlateCenter(plate);
    float width = std::max(8.0f, plate.size[0] * 0.72f);
    float depth = std::max(8.0f, plate.size[1] * 0.72f);

    for (int attempt = 0; attempt < 18; ++attempt) {
        const Rect candidate = MakeCenteredRect(center, width, depth);
        if (RectFitsPlateBoundary(plate, candidate, margin)) {
            return candidate;
        }
        width = std::max(4.0f, width * 0.92f);
        depth = std::max(4.0f, depth * 0.92f);
    }

    return MakeCenteredRect(center, width, depth);
}

const SemanticSpace* FindFirstSemanticSpace(const FloorLayoutInput& layoutInput, const char* type) {
    for (const auto& space : layoutInput.spaces) {
        if (ToLowerCopy(space.type) == type) {
            return &space;
        }
    }
    return nullptr;
}

Rect MergeCoreEnvelope(const std::vector<VerticalCore>& cores) {
    Rect merged;
    bool hasAny = false;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    for (const auto& core : cores) {
        if (core.type != VerticalCoreType::Service &&
            core.type != VerticalCoreType::Elevator &&
            core.type != VerticalCoreType::Stair) {
            continue;
        }

        if (!hasAny) {
            minX = core.rect.origin[0];
            minY = core.rect.origin[1];
            maxX = core.rect.origin[0] + core.rect.size[0];
            maxY = core.rect.origin[1] + core.rect.size[1];
            hasAny = true;
        } else {
            minX = std::min(minX, core.rect.origin[0]);
            minY = std::min(minY, core.rect.origin[1]);
            maxX = std::max(maxX, core.rect.origin[0] + core.rect.size[0]);
            maxY = std::max(maxY, core.rect.origin[1] + core.rect.size[1]);
        }
    }

    if (!hasAny) {
        return {};
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

Rect ComputeAtriumRect(const FloorLayoutInput& layoutInput,
                       const FloorPlate& floorPlate,
                       const Rect& safeInterior,
                       float circulationDepth,
                       float perimeterDepth,
                       float margin) {
    const auto center = ComputePlateCenter(floorPlate);
    const SemanticSpace* semanticVoid = FindFirstSemanticSpace(layoutInput, "void");
    const float minVoidWidth = semanticVoid
        ? std::max(4.0f, semanticVoid->constraints.minWidth)
        : 6.0f;
    const float targetArea = semanticVoid
        ? std::max(36.0f, semanticVoid->areaPreferred)
        : std::max(48.0f, safeInterior.size[0] * safeInterior.size[1] * 0.18f);

    const float maxWidth = std::max(minVoidWidth,
        safeInterior.size[0] - (circulationDepth + perimeterDepth) * 2.0f);
    const float maxDepth = std::max(minVoidWidth,
        safeInterior.size[1] - (circulationDepth + perimeterDepth) * 2.0f);
    float aspect = safeInterior.size[0] / std::max(safeInterior.size[1], 0.5f);
    aspect = std::clamp(aspect, 0.8f, 1.25f);

    float width = std::sqrt(targetArea * aspect);
    float depth = targetArea / std::max(width, 0.5f);
    width = std::clamp(width, minVoidWidth, maxWidth);
    depth = std::clamp(depth, minVoidWidth, maxDepth);

    Rect atrium = FitRectInsideBounds(MakeCenteredRect(center, width, depth), safeInterior);
    for (int attempt = 0; attempt < 12; ++attempt) {
        if (RectFitsPlate(floorPlate, atrium, margin)) {
            return atrium;
        }
        width = std::max(minVoidWidth, width * 0.92f);
        depth = std::max(minVoidWidth, depth * 0.92f);
        atrium = FitRectInsideBounds(MakeCenteredRect(center, width, depth), safeInterior);
    }
    return atrium;
}

std::vector<VerticalCore> BuildFallbackCores(const Rect& safeInterior, const Rect& atriumVoid) {
    Rect envelope;
    envelope.size = {
        std::clamp(safeInterior.size[0] * 0.24f, 4.0f, 7.0f),
        std::clamp(safeInterior.size[1] * 0.22f, 4.5f, 8.0f)
    };
    envelope.origin = {
        safeInterior.origin[0] + std::max(0.5f, safeInterior.size[0] * 0.06f),
        safeInterior.origin[1] + std::max(0.5f, safeInterior.size[1] * 0.06f)
    };

    const auto overlapsAtrium = [&](const Rect& rect) {
        const float rectMaxX = rect.origin[0] + rect.size[0];
        const float rectMaxY = rect.origin[1] + rect.size[1];
        const float atriumMaxX = atriumVoid.origin[0] + atriumVoid.size[0];
        const float atriumMaxY = atriumVoid.origin[1] + atriumVoid.size[1];
        return rect.origin[0] < atriumMaxX &&
               rectMaxX > atriumVoid.origin[0] &&
               rect.origin[1] < atriumMaxY &&
               rectMaxY > atriumVoid.origin[1];
    };

    if (overlapsAtrium(envelope)) {
        envelope.origin = {
            safeInterior.origin[0] + std::max(0.5f, safeInterior.size[0] * 0.06f),
            safeInterior.origin[1] + safeInterior.size[1] - envelope.size[1] - std::max(0.5f, safeInterior.size[1] * 0.06f)
        };
    }
    const float stairWidth = std::clamp(envelope.size[0] * 0.28f, 1.8f, 2.6f);
    const float elevatorWidth = std::clamp(envelope.size[0] * 0.34f, 2.2f, 3.0f);
    const float serviceWidth = std::max(1.4f, envelope.size[0] - stairWidth - elevatorWidth - 0.8f);
    const float gap = std::max(0.3f, (envelope.size[0] - stairWidth - elevatorWidth - serviceWidth) * 0.5f);

    std::vector<VerticalCore> cores;

    VerticalCore stair;
    stair.coreId = "retail_core_stair";
    stair.type = VerticalCoreType::Stair;
    stair.rect.origin = {envelope.origin[0], envelope.origin[1]};
    stair.rect.size = {stairWidth, envelope.size[1]};
    cores.push_back(stair);

    VerticalCore elevator;
    elevator.coreId = "retail_core_elevator";
    elevator.type = VerticalCoreType::Elevator;
    elevator.rect.origin = {stair.rect.origin[0] + stair.rect.size[0] + gap, envelope.origin[1]};
    elevator.rect.size = {elevatorWidth, envelope.size[1]};
    cores.push_back(elevator);

    VerticalCore service;
    service.coreId = "retail_core_service";
    service.type = VerticalCoreType::Service;
    service.rect.origin = {elevator.rect.origin[0] + elevator.rect.size[0] + gap, envelope.origin[1]};
    service.rect.size = {
        std::max(1.4f, envelope.origin[0] + envelope.size[0] - service.rect.origin[0]),
        envelope.size[1]
    };
    cores.push_back(service);

    return cores;
}

std::vector<VerticalCore> ResolveUsableCores(const FloorPlate& floorPlate,
                                             const std::vector<VerticalCore>& floorCores,
                                             const Rect& safeInterior,
                                             const Rect& atriumVoid,
                                             float margin) {
    std::vector<VerticalCore> usable;
    for (const auto& core : floorCores) {
        if (RectFitsPlate(floorPlate, core.rect, margin)) {
            usable.push_back(core);
        }
    }

    const bool hasStair = std::any_of(usable.begin(), usable.end(),
        [](const VerticalCore& core) { return core.type == VerticalCoreType::Stair; });
    const bool hasElevator = std::any_of(usable.begin(), usable.end(),
        [](const VerticalCore& core) { return core.type == VerticalCoreType::Elevator; });
    const bool hasService = std::any_of(usable.begin(), usable.end(),
        [](const VerticalCore& core) { return core.type == VerticalCoreType::Service; });

    if (hasStair && hasElevator && hasService) {
        return usable;
    }

    return BuildFallbackCores(safeInterior, atriumVoid);
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
    space.ceilingHeight = ceilingHeight > 0.0f ? ceilingHeight : 4.2f;
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

bool RetailFloorLayoutSolver::GenerateFloor(const BuildingDefinition& definition,
                                            const FloorLayoutInput& layoutInput,
                                            const FloorPlate& floorPlate,
                                            const std::vector<VerticalCore>& floorCores,
                                            ResolvedFloorLayout& outResolvedFloor,
                                            std::string& outError) const {
    outResolvedFloor.level = layoutInput.level;
    outResolvedFloor.spaces.clear();
    outResolvedFloor.debugBlocks.clear();

    const float margin = std::max(0.35f, definition.grid);
    const float ringDepth = std::max(3.0f, definition.grid * 6.0f);
    const float perimeterDepth = std::max(4.5f, definition.grid * 8.0f);
    const Rect safeInterior = ComputeSafeInteriorRect(floorPlate, margin);
    const Rect atriumVoid = ComputeAtriumRect(
        layoutInput,
        floorPlate,
        safeInterior,
        ringDepth,
        perimeterDepth,
        margin);
    const Rect circulationBounds = FitRectInsideBounds(ExpandRect(atriumVoid, ringDepth), safeInterior);
    const std::vector<VerticalCore> usableCores = ResolveUsableCores(
        floorPlate,
        floorCores,
        safeInterior,
        atriumVoid,
        margin);
    const Rect mergedCore = MergeCoreEnvelope(usableCores);

    std::vector<Rect> circulationRects;
    std::vector<Rect> perimeterRects;

    Rect topConcourse;
    topConcourse.rectId = "retail_ring_top";
    topConcourse.origin = {circulationBounds.origin[0], atriumVoid.origin[1] + atriumVoid.size[1]};
    topConcourse.size = {
        circulationBounds.size[0],
        circulationBounds.origin[1] + circulationBounds.size[1] - (atriumVoid.origin[1] + atriumVoid.size[1])
    };
    Rect bottomConcourse;
    bottomConcourse.rectId = "retail_ring_bottom";
    bottomConcourse.origin = {circulationBounds.origin[0], circulationBounds.origin[1]};
    bottomConcourse.size = {
        circulationBounds.size[0],
        atriumVoid.origin[1] - circulationBounds.origin[1]
    };
    Rect leftConcourse;
    leftConcourse.rectId = "retail_ring_left";
    leftConcourse.origin = {circulationBounds.origin[0], atriumVoid.origin[1]};
    leftConcourse.size = {
        atriumVoid.origin[0] - circulationBounds.origin[0],
        atriumVoid.size[1]
    };
    Rect rightConcourse;
    rightConcourse.rectId = "retail_ring_right";
    rightConcourse.origin = {atriumVoid.origin[0] + atriumVoid.size[0], atriumVoid.origin[1]};
    rightConcourse.size = {
        circulationBounds.origin[0] + circulationBounds.size[0] - (atriumVoid.origin[0] + atriumVoid.size[0]),
        atriumVoid.size[1]
    };

    for (Rect rect : {topConcourse, bottomConcourse, leftConcourse, rightConcourse}) {
        if (IsUsableRect(rect, 2.0f) && RectFitsPlate(floorPlate, rect, margin)) {
            circulationRects.push_back(rect);
        }
    }

    Rect topPerimeter;
    topPerimeter.rectId = "retail_perimeter_top";
    topPerimeter.origin = {safeInterior.origin[0], circulationBounds.origin[1] + circulationBounds.size[1]};
    topPerimeter.size = {
        safeInterior.size[0],
        safeInterior.origin[1] + safeInterior.size[1] - (circulationBounds.origin[1] + circulationBounds.size[1])
    };
    Rect bottomPerimeter;
    bottomPerimeter.rectId = "retail_perimeter_bottom";
    bottomPerimeter.origin = {safeInterior.origin[0], safeInterior.origin[1]};
    bottomPerimeter.size = {
        safeInterior.size[0],
        circulationBounds.origin[1] - safeInterior.origin[1]
    };
    Rect leftPerimeter;
    leftPerimeter.rectId = "retail_perimeter_left";
    leftPerimeter.origin = {safeInterior.origin[0], circulationBounds.origin[1]};
    leftPerimeter.size = {
        circulationBounds.origin[0] - safeInterior.origin[0],
        circulationBounds.size[1]
    };
    Rect rightPerimeter;
    rightPerimeter.rectId = "retail_perimeter_right";
    rightPerimeter.origin = {circulationBounds.origin[0] + circulationBounds.size[0], circulationBounds.origin[1]};
    rightPerimeter.size = {
        safeInterior.origin[0] + safeInterior.size[0] - (circulationBounds.origin[0] + circulationBounds.size[0]),
        circulationBounds.size[1]
    };

    for (Rect rect : {topPerimeter, bottomPerimeter, leftPerimeter, rightPerimeter}) {
        if (IsUsableRect(rect, 2.5f) && RectFitsPlate(floorPlate, rect, margin)) {
            perimeterRects.push_back(rect);
        }
    }

    if (circulationRects.empty()) {
        Rect fallbackConcourse;
        fallbackConcourse.rectId = "retail_fallback_concourse";
        fallbackConcourse.origin = {
            safeInterior.origin[0],
            safeInterior.origin[1] + safeInterior.size[1] * 0.4f
        };
        fallbackConcourse.size = {
            safeInterior.size[0],
            std::max(2.5f, safeInterior.size[1] * 0.16f)
        };
        if (RectFitsPlate(floorPlate, fallbackConcourse, margin)) {
            circulationRects.push_back(fallbackConcourse);
        }
    }

    if (perimeterRects.empty()) {
        Rect fallbackShopA;
        fallbackShopA.rectId = "retail_fallback_shop_a";
        fallbackShopA.origin = {
            safeInterior.origin[0],
            safeInterior.origin[1]
        };
        fallbackShopA.size = {
            std::max(4.0f, safeInterior.size[0] * 0.44f),
            std::max(4.0f, safeInterior.size[1] * 0.3f)
        };

        Rect fallbackShopB = fallbackShopA;
        fallbackShopB.rectId = "retail_fallback_shop_b";
        fallbackShopB.origin = {
            safeInterior.origin[0] + safeInterior.size[0] - fallbackShopB.size[0],
            safeInterior.origin[1] + safeInterior.size[1] - fallbackShopB.size[1]
        };

        if (RectFitsPlate(floorPlate, fallbackShopA, margin)) {
            perimeterRects.push_back(fallbackShopA);
        }
        if (RectFitsPlate(floorPlate, fallbackShopB, margin)) {
            perimeterRects.push_back(fallbackShopB);
        }
    }

    size_t nextShop = 0;
    bool synthesizedAny = false;

    for (const auto& semanticSpace : layoutInput.spaces) {
        if (IsCoreSemanticType(semanticSpace.type)) {
            const auto usage = StringToSpaceUsage(semanticSpace.type);
            const VerticalCore* matchedCore = nullptr;
            if (semanticSpace.type == "elevator") {
                auto it = std::find_if(usableCores.begin(), usableCores.end(),
                    [](const VerticalCore& core) { return core.type == VerticalCoreType::Elevator; });
                if (it != usableCores.end()) {
                    matchedCore = &(*it);
                }
            } else {
                auto it = std::find_if(usableCores.begin(), usableCores.end(),
                    [](const VerticalCore& core) { return core.type == VerticalCoreType::Stair; });
                if (it == usableCores.end()) {
                    it = std::find_if(usableCores.begin(), usableCores.end(),
                        [](const VerticalCore& core) { return core.type == VerticalCoreType::Service; });
                }
                if (it != usableCores.end()) {
                    matchedCore = &(*it);
                }
            }

            if (matchedCore && RectFitsPlate(floorPlate, matchedCore->rect, margin)) {
                const bool connectsUp = semanticSpace.constraints.connectsToFloor >= 0 &&
                    semanticSpace.constraints.connectsToFloor != layoutInput.level;
                const bool shouldGenerateStair = semanticSpace.type == "core" || semanticSpace.type == "stairs";
                ResolvedSpacePlan space = MakeResolvedSpace(
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
                                       matchedCore->rect));
                AddDebugBlock(space, layoutInput.level, outResolvedFloor.debugBlocks);
                outResolvedFloor.spaces.push_back(std::move(space));
                synthesizedAny = true;
            }
            continue;
        }

        const SpaceUsage usage = StringToSpaceUsage(semanticSpace.type);
        if (usage == SpaceUsage::Unknown) {
            continue;
        }

        if ((usage == SpaceUsage::Corridor || usage == SpaceUsage::Entrance) &&
            !circulationRects.empty()) {
            for (size_t i = 0; i < circulationRects.size(); ++i) {
                Rect rect = circulationRects[i];
                rect.rectId = semanticSpace.spaceId + "_" + std::to_string(i);
                if (!RectFitsPlate(floorPlate, rect, margin)) {
                    continue;
                }
                ResolvedSpacePlan space = MakeResolvedSpace(
                    rect.rectId,
                    usage,
                    rect,
                    semanticSpace.constraints.ceilingHeight,
                    layoutInput.level);
                AddDebugBlock(space, layoutInput.level, outResolvedFloor.debugBlocks);
                outResolvedFloor.spaces.push_back(std::move(space));
                synthesizedAny = true;
            }
            continue;
        }

        if ((usage == SpaceUsage::Office || usage == SpaceUsage::Entrance) &&
            nextShop < perimeterRects.size()) {
            Rect candidate = perimeterRects[nextShop++];
            candidate = FitRectToArea(candidate,
                std::max(18.0f, semanticSpace.areaPreferred),
                std::max(3.0f, semanticSpace.constraints.minWidth));
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
            continue;
        }

        if ((usage == SpaceUsage::Storage || usage == SpaceUsage::Bathroom) &&
            mergedCore.size[0] > 0.0f && mergedCore.size[1] > 0.0f) {
            Rect serviceRect;
            serviceRect.rectId = semanticSpace.spaceId;
            serviceRect.origin = {
                std::max(safeInterior.origin[0], mergedCore.origin[0] - definition.grid * 0.5f),
                mergedCore.origin[1] + mergedCore.size[1] + definition.grid
            };
            serviceRect.size = {
                std::max(2.0f, mergedCore.size[0]),
                std::max(2.5f, semanticSpace.areaPreferred / std::max(mergedCore.size[0], 2.0f))
            };

            if (RectFitsPlate(floorPlate, serviceRect, margin)) {
                ResolvedSpacePlan space = MakeResolvedSpace(
                    semanticSpace.spaceId,
                    usage,
                    serviceRect,
                    semanticSpace.constraints.ceilingHeight,
                    layoutInput.level);
                AddDebugBlock(space, layoutInput.level, outResolvedFloor.debugBlocks);
                outResolvedFloor.spaces.push_back(std::move(space));
                synthesizedAny = true;
            }
        }
    }

    if (!synthesizedAny) {
        outError = "Failed to synthesize retail floor spaces inside sliced floor plates"
            " (level=" + std::to_string(layoutInput.level) +
            ", safe=" + std::to_string(safeInterior.size[0]) + "x" + std::to_string(safeInterior.size[1]) +
            ", circulation=" + std::to_string(circulationRects.size()) +
            ", perimeter=" + std::to_string(perimeterRects.size()) +
            ", cores=" + std::to_string(usableCores.size()) + ")";
        return false;
    }

    return true;
}

} // namespace Building
} // namespace Moon
