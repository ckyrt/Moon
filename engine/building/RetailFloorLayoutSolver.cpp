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
    bool hasAny = false;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    for (const auto& core : cores) {
        if (core.type == VerticalCoreType::Service || core.type == VerticalCoreType::Elevator || core.type == VerticalCoreType::Stair) {
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
    }

    if (!hasAny) {
        return {};
    }

    merged.origin = {minX, minY};
    merged.size = {maxX - minX, maxY - minY};
    return merged;
}

Space MakeSpace(int spaceId,
                const std::string& rectId,
                SpaceUsage usage,
                const std::vector<Rect>& rects,
                float ceilingHeight,
                int floorLevel) {
    Space space;
    space.spaceId = spaceId;
    space.properties.usage = usage;
    space.properties.isOutdoor = false;
    space.properties.hasStairs = (usage == SpaceUsage::Stairwell);
    space.properties.ceilingHeight = ceilingHeight > 0.0f ? ceilingHeight : 4.2f;
    space.rects = rects;
    if (usage == SpaceUsage::Stairwell && !rects.empty()) {
        space.stairsConfig.connectToLevel = floorLevel + 1;
        space.stairsConfig.width = rects.front().size[0];
        space.stairsConfig.position = rects.front().origin;
    }
    return space;
}

void AddDebugBlocks(const Space& space,
                    int floorLevel,
                    std::vector<ProgramBlock>* outDebugBlocks) {
    if (!outDebugBlocks) {
        return;
    }
    for (const auto& rect : space.rects) {
        ProgramBlock block;
        block.blockId = rect.rectId;
        block.floorLevel = floorLevel;
        block.usage = space.properties.usage;
        block.rect = rect;
        outDebugBlocks->push_back(block);
    }
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

} // namespace

namespace Moon {
namespace Building {

bool RetailFloorLayoutSolver::GenerateFloor(const BuildingDefinition& definition,
                                            const FloorLayoutInput& layoutInput,
                                            const FloorPlate& floorPlate,
                                            const std::vector<VerticalCore>& floorCores,
                                            int& ioNextSpaceId,
                                            std::vector<Space>& outSpaces,
                                            std::vector<ProgramBlock>* outDebugBlocks,
                                            std::string& outError) const {
    outSpaces.clear();

    Rect atriumVoid;
    bool hasVoid = false;
    if (!floorPlate.voids.empty()) {
        atriumVoid = floorPlate.voids.front();
        hasVoid = true;
    }

    const float ringDepth = std::max(3.0f, definition.grid * 6.0f);
    const float shopBandDepth = std::max(5.5f, definition.grid * 10.0f);
    const float margin = std::max(0.2f, definition.grid * 0.5f);
    const Rect mergedCore = MergeCoreEnvelope(floorCores);

    std::vector<Rect> circulationRects;
    std::vector<Rect> shopCandidates;

    if (hasVoid) {
        Rect top;
        top.rectId = "retail_ring_top";
        top.origin = {atriumVoid.origin[0] - ringDepth, atriumVoid.origin[1] + atriumVoid.size[1]};
        top.size = {atriumVoid.size[0] + ringDepth * 2.0f, ringDepth};
        Rect bottom;
        bottom.rectId = "retail_ring_bottom";
        bottom.origin = {atriumVoid.origin[0] - ringDepth, atriumVoid.origin[1] - ringDepth};
        bottom.size = {atriumVoid.size[0] + ringDepth * 2.0f, ringDepth};
        Rect left;
        left.rectId = "retail_ring_left";
        left.origin = {atriumVoid.origin[0] - ringDepth, atriumVoid.origin[1]};
        left.size = {ringDepth, atriumVoid.size[1]};
        Rect right;
        right.rectId = "retail_ring_right";
        right.origin = {atriumVoid.origin[0] + atriumVoid.size[0], atriumVoid.origin[1]};
        right.size = {ringDepth, atriumVoid.size[1]};

        for (Rect rect : {top, bottom, left, right}) {
            if (RectFitsPlate(floorPlate, rect, margin)) {
                circulationRects.push_back(rect);
            }
        }

        Rect topShop;
        topShop.rectId = "retail_shop_top_band";
        topShop.origin = {top.origin[0], top.origin[1] + top.size[1]};
        topShop.size = {top.size[0], shopBandDepth};
        Rect bottomShop;
        bottomShop.rectId = "retail_shop_bottom_band";
        bottomShop.origin = {bottom.origin[0], bottom.origin[1] - shopBandDepth};
        bottomShop.size = {bottom.size[0], shopBandDepth};
        Rect leftShop;
        leftShop.rectId = "retail_shop_left_band";
        leftShop.origin = {left.origin[0] - shopBandDepth, left.origin[1]};
        leftShop.size = {shopBandDepth, left.size[1]};
        Rect rightShop;
        rightShop.rectId = "retail_shop_right_band";
        rightShop.origin = {right.origin[0] + right.size[0], right.origin[1]};
        rightShop.size = {shopBandDepth, right.size[1]};

        for (Rect rect : {topShop, bottomShop, leftShop, rightShop}) {
            if (RectFitsPlate(floorPlate, rect, margin)) {
                shopCandidates.push_back(rect);
            }
        }
    } else {
        Rect fallbackConcourse;
        fallbackConcourse.rectId = "retail_fallback_concourse";
        fallbackConcourse.origin = {
            floorPlate.origin[0] + floorPlate.size[0] * 0.2f,
            floorPlate.origin[1] + floorPlate.size[1] * 0.4f
        };
        fallbackConcourse.size = {
            std::max(4.0f, floorPlate.size[0] * 0.6f),
            std::max(3.0f, floorPlate.size[1] * 0.2f)
        };
        if (RectFitsPlate(floorPlate, fallbackConcourse, margin)) {
            circulationRects.push_back(fallbackConcourse);
        }

        Rect fallbackShopA;
        fallbackShopA.rectId = "retail_fallback_shop_a";
        fallbackShopA.origin = {floorPlate.origin[0] + margin, floorPlate.origin[1] + margin};
        fallbackShopA.size = {
            std::max(5.0f, floorPlate.size[0] * 0.35f),
            std::max(5.0f, floorPlate.size[1] * 0.3f)
        };
        Rect fallbackShopB = fallbackShopA;
        fallbackShopB.rectId = "retail_fallback_shop_b";
        fallbackShopB.origin = {
            floorPlate.origin[0] + floorPlate.size[0] - fallbackShopB.size[0] - margin,
            floorPlate.origin[1] + margin
        };
        if (RectFitsPlate(floorPlate, fallbackShopA, margin)) {
            shopCandidates.push_back(fallbackShopA);
        }
        if (RectFitsPlate(floorPlate, fallbackShopB, margin)) {
            shopCandidates.push_back(fallbackShopB);
        }
    }

    size_t nextCirculation = 0;
    size_t nextShop = 0;
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
                std::vector<Rect> rects = {matchedCore->rect};
                Space space = MakeSpace(ioNextSpaceId++, semanticSpace.spaceId, usage, rects,
                    semanticSpace.constraints.ceilingHeight, layoutInput.level);
                outSpaces.push_back(space);
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
            std::vector<Rect> taggedRects = circulationRects;
            for (size_t i = 0; i < taggedRects.size(); ++i) {
                taggedRects[i].rectId = semanticSpace.spaceId + "_" + std::to_string(i);
            }
            Space space = MakeSpace(ioNextSpaceId++, semanticSpace.spaceId, usage, taggedRects,
                semanticSpace.constraints.ceilingHeight, layoutInput.level);
            AddDebugBlocks(space, layoutInput.level, outDebugBlocks);
            outSpaces.push_back(space);
            synthesizedAny = true;
            continue;
        }

        if (usage == SpaceUsage::Office &&
            nextShop < shopCandidates.size()) {
            Rect candidate = shopCandidates[nextShop++];
            candidate = FitRectToArea(candidate,
                std::max(18.0f, semanticSpace.areaPreferred),
                std::max(3.0f, semanticSpace.constraints.minWidth));
            candidate.rectId = semanticSpace.spaceId;
            if (!RectFitsPlate(floorPlate, candidate, margin)) {
                continue;
            }

            Space space = MakeSpace(ioNextSpaceId++, semanticSpace.spaceId, usage, {candidate},
                semanticSpace.constraints.ceilingHeight, layoutInput.level);
            AddDebugBlocks(space, layoutInput.level, outDebugBlocks);
            outSpaces.push_back(space);
            synthesizedAny = true;
            continue;
        }

        if (usage == SpaceUsage::Storage && mergedCore.size[0] > 0.0f && mergedCore.size[1] > 0.0f) {
            Rect storageRect;
            storageRect.rectId = semanticSpace.spaceId;
            storageRect.origin = {
                mergedCore.origin[0],
                mergedCore.origin[1] + mergedCore.size[1] + definition.grid
            };
            storageRect.size = {
                std::max(2.0f, mergedCore.size[0]),
                std::max(2.5f, semanticSpace.areaPreferred / std::max(mergedCore.size[0], 2.0f))
            };
            if (RectFitsPlate(floorPlate, storageRect, margin)) {
                Space space = MakeSpace(ioNextSpaceId++, semanticSpace.spaceId, usage, {storageRect},
                    semanticSpace.constraints.ceilingHeight, layoutInput.level);
                AddDebugBlocks(space, layoutInput.level, outDebugBlocks);
                outSpaces.push_back(space);
                synthesizedAny = true;
            }
        }
    }

    if (!synthesizedAny) {
        outError = "Failed to synthesize retail floor spaces inside sliced floor plates";
        return false;
    }

    return true;
}

} // namespace Building
} // namespace Moon
