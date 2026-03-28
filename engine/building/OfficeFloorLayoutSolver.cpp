#include "OfficeFloorLayoutSolver.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <unordered_set>

namespace {

using namespace Moon::Building;

std::string ToLowerCopy(const std::string& value) {
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowered;
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

bool RectContainsPoint(const Rect& rect, const GridPos2D& point) {
    return point[0] > rect.origin[0] + 0.001f &&
           point[0] < rect.origin[0] + rect.size[0] - 0.001f &&
           point[1] > rect.origin[1] + 0.001f &&
           point[1] < rect.origin[1] + rect.size[1] - 0.001f;
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

Rect ComputeSafeInteriorRect(const FloorPlate& plate, float margin) {
    Rect rect;
    const auto center = ComputePlateCenter(plate);
    const float safeWidth = std::max(2.0f, plate.size[0] * 0.62f);
    const float safeDepth = std::max(2.0f, plate.size[1] * 0.62f);
    rect.origin = {
        std::max(plate.origin[0] + margin, center[0] - safeWidth * 0.5f),
        std::max(plate.origin[1] + margin, center[1] - safeDepth * 0.5f)
    };
    rect.size = {
        std::min(safeWidth, plate.origin[0] + plate.size[0] - margin - rect.origin[0]),
        std::min(safeDepth, plate.origin[1] + plate.size[1] - margin - rect.origin[1])
    };
    rect.size[0] = std::max(0.5f, rect.size[0]);
    rect.size[1] = std::max(0.5f, rect.size[1]);
    return rect;
}

bool IsCoreSemanticType(const std::string& type) {
    const std::string lowered = ToLowerCopy(type);
    return lowered == "core" || lowered == "stairs" || lowered == "elevator" || lowered == "mechanical";
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

float DetermineStairRotationDegrees(const Rect& rect) {
    return rect.size[0] >= rect.size[1] ? 90.0f : 0.0f;
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
    space.ceilingHeight = ceilingHeight > 0.0f ? ceilingHeight : 3.6f;
    space.hasStairs = hasStairs;
    space.stairConnectToLevel = hasStairs ? stairConnectToLevel : -1;
    space.stairWidth = stairWidth > 0.0f ? stairWidth : rect.size[0];
    space.stairPosition = stairPosition;
    space.stairType = stairType;
    space.stairRotationDegrees = DetermineStairRotationDegrees(rect);
    return space;
}

ResolvedVerticalTransportPlan MakeResolvedVerticalTransport(const std::string& transportId,
                                                            VerticalTransportType type,
                                                            const Rect& shaftRect,
                                                            int floorFrom,
                                                            int floorTo,
                                                            float width,
                                                            const GridPos2D& position,
                                                            float rotationDegrees,
                                                            StairType stairType = StairType::Straight) {
    ResolvedVerticalTransportPlan transport;
    transport.transportId = transportId;
    transport.type = type;
    transport.shaftRect = shaftRect;
    transport.floorFrom = floorFrom;
    transport.floorTo = floorTo;
    transport.sourceFloorLevel = floorFrom;
    transport.continuousShaft = type == VerticalTransportType::Elevator;
    transport.stairType = stairType;
    transport.width = width;
    transport.position = position;
    transport.rotationDegrees = rotationDegrees;
    return transport;
}

ResolvedVerticalTransportPlan MakeResolvedVerticalTransportFromSemantic(const SemanticVerticalSystem& system) {
    const VerticalTransportType type = system.type == "elevator"
        ? VerticalTransportType::Elevator
        : VerticalTransportType::Stair;
    const float width = std::min(system.shaftRect.size[0], system.shaftRect.size[1]);
    const float rotation = DetermineStairRotationDegrees(system.shaftRect);
    const GridPos2D position = type == VerticalTransportType::Elevator
        ? GridPos2D{system.shaftRect.origin[0] + system.shaftRect.size[0] * 0.5f,
                    system.shaftRect.origin[1] + system.shaftRect.size[1] * 0.5f}
        : GridPos2D{system.shaftRect.origin[0] + system.shaftRect.size[0] * 0.5f,
                    system.shaftRect.origin[1] + 0.25f};
    auto transport = MakeResolvedVerticalTransport(
        system.id,
        type,
        system.shaftRect,
        system.floorFrom,
        system.floorTo,
        width,
        position,
        rotation,
        ParseStairType(system.stairForm));
    transport.enclosed = system.mode != "open";
    transport.external = system.placement == "external";
    transport.continuousShaft = type == VerticalTransportType::Elevator || system.type == "stairwell";
    return transport;
}

void AddDebugBlock(const ResolvedSpacePlan& space,
                   int floorLevel,
                   std::vector<ProgramBlock>& outDebugBlocks) {
    if (space.usage == SpaceUsage::Stairwell || space.usage == SpaceUsage::Storage) {
        return;
    }
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

bool OfficeFloorLayoutSolver::GenerateFloor(const BuildingDefinition& definition,
                                            const FloorLayoutInput& layoutInput,
                                            const FloorPlate& floorPlate,
                                            const std::vector<VerticalCore>& floorCores,
                                            ResolvedFloorLayout& outResolvedFloor,
                                            std::string& outError) const {
    outResolvedFloor.level = layoutInput.level;
    outResolvedFloor.spaces.clear();
    outResolvedFloor.verticalTransports.clear();
    outResolvedFloor.debugBlocks.clear();

    const Rect combinedCore = MergeCoreEnvelope(floorCores);
    if (combinedCore.size[0] <= 0.0f || combinedCore.size[1] <= 0.0f) {
        outError = "Office floor layout requires a valid stair/elevator core envelope";
        if (layoutInput.verticalSystems.empty()) {
            return false;
        }
    }

    const float corridorWidth = definition.style.category == "commercial" ? 2.5f : 2.0f;
    const float margin = std::max(0.2f, definition.grid * 0.5f);
    const float sideInset = std::max(0.8f, corridorWidth + definition.grid);
    const Rect safeInterior = ComputeSafeInteriorRect(floorPlate, margin);
    const float plateMinX = safeInterior.origin[0];
    const float plateMinY = safeInterior.origin[1];
    const float plateMaxX = safeInterior.origin[0] + safeInterior.size[0];
    const float plateMaxY = safeInterior.origin[1] + safeInterior.size[1];

    std::vector<Rect> corridorCandidates;
    corridorCandidates.push_back({{}, {combinedCore.origin[0] - corridorWidth, combinedCore.origin[1] + combinedCore.size[1]},
                                  {combinedCore.size[0] + corridorWidth * 2.0f, corridorWidth}});
    corridorCandidates.push_back({{}, {combinedCore.origin[0] - corridorWidth, combinedCore.origin[1] - corridorWidth},
                                  {combinedCore.size[0] + corridorWidth * 2.0f, corridorWidth}});
    corridorCandidates.push_back({{}, {combinedCore.origin[0] - corridorWidth, combinedCore.origin[1]},
                                  {corridorWidth, combinedCore.size[1]}});
    corridorCandidates.push_back({{}, {combinedCore.origin[0] + combinedCore.size[0], combinedCore.origin[1]},
                                  {corridorWidth, combinedCore.size[1]}});

    std::vector<Rect> perimeterCandidates;
    if (layoutInput.level == 0) {
        Rect lobby;
        lobby.origin = {combinedCore.origin[0] - combinedCore.size[0] * 0.35f,
                        combinedCore.origin[1] - corridorWidth - 4.5f};
        lobby.size = {combinedCore.size[0] * 1.7f, 4.0f};
        perimeterCandidates.push_back(lobby);
    }
    perimeterCandidates.push_back({{}, {plateMinX, plateMinY},
                                   {std::max(0.5f, combinedCore.origin[0] - sideInset - plateMinX),
                                    std::max(0.5f, plateMaxY - plateMinY)}});
    perimeterCandidates.push_back({{}, {combinedCore.origin[0] + combinedCore.size[0] + sideInset, plateMinY},
                                   {std::max(0.5f, plateMaxX - (combinedCore.origin[0] + combinedCore.size[0] + sideInset)),
                                    std::max(0.5f, plateMaxY - plateMinY)}});
    perimeterCandidates.push_back({{}, {std::max(plateMinX, combinedCore.origin[0] - combinedCore.size[0] * 0.95f),
                                        combinedCore.origin[1] + combinedCore.size[1] + sideInset},
                                   {std::max(0.5f, combinedCore.origin[0] - definition.grid -
                                                    std::max(plateMinX, combinedCore.origin[0] - combinedCore.size[0] * 0.95f)),
                                    std::max(0.5f, plateMaxY - (combinedCore.origin[1] + combinedCore.size[1] + sideInset))}});
    perimeterCandidates.push_back({{}, {combinedCore.origin[0] + combinedCore.size[0] + definition.grid,
                                        combinedCore.origin[1] + combinedCore.size[1] + sideInset},
                                   {std::max(0.5f, plateMaxX - (combinedCore.origin[0] + combinedCore.size[0] + definition.grid)),
                                    std::max(0.5f, plateMaxY - (combinedCore.origin[1] + combinedCore.size[1] + sideInset))}});
    perimeterCandidates.push_back({{}, {std::max(plateMinX, combinedCore.origin[0] - combinedCore.size[0] * 0.4f), plateMinY},
                                   {std::min(std::max(2.5f, combinedCore.size[0] * 1.8f),
                                             plateMaxX - std::max(plateMinX, combinedCore.origin[0] - combinedCore.size[0] * 0.4f)),
                                    std::max(0.5f, combinedCore.origin[1] - sideInset - plateMinY)}});

    size_t nextCorridorCandidate = 0;
    size_t nextPerimeterCandidate = 0;
    std::unordered_set<std::string> emittedTransports;
    for (const auto& semanticSystem : layoutInput.verticalSystems) {
        if (!semanticSystem.id.empty() && emittedTransports.insert(semanticSystem.id).second) {
            outResolvedFloor.verticalTransports.push_back(
                MakeResolvedVerticalTransportFromSemantic(semanticSystem));
        }
    }

    for (const auto& semanticSpace : layoutInput.spaces) {
        if (IsCoreSemanticType(semanticSpace.type)) {
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
                if (semanticSpace.type == "elevator") {
                    outResolvedFloor.verticalTransports.push_back(MakeResolvedVerticalTransport(
                        semanticSpace.spaceId,
                        VerticalTransportType::Elevator,
                        matchedCore->rect,
                        0,
                        definition.floors.empty() ? 0 : definition.floors.back().level,
                        matchedCore->rect.size[0],
                        {matchedCore->rect.origin[0] + matchedCore->rect.size[0] * 0.5f,
                         matchedCore->rect.origin[1] + matchedCore->rect.size[1] * 0.5f},
                        DetermineStairRotationDegrees(matchedCore->rect)));
                } else if ((semanticSpace.type == "core" || semanticSpace.type == "stairs") && connectsUp) {
                    const float stairWidth = std::min(matchedCore->rect.size[0], matchedCore->rect.size[1]);
                    const float stairRotation = DetermineStairRotationDegrees(matchedCore->rect);
                    const StairType stairType = DetermineStairType(
                        definition,
                        layoutInput.level,
                        semanticSpace.constraints.connectsToFloor,
                        matchedCore->rect);
                    outResolvedFloor.verticalTransports.push_back(MakeResolvedVerticalTransport(
                        semanticSpace.spaceId,
                        VerticalTransportType::Stair,
                        matchedCore->rect,
                        layoutInput.level,
                        semanticSpace.constraints.connectsToFloor,
                        stairWidth,
                        {matchedCore->rect.origin[0] + matchedCore->rect.size[0] * 0.5f,
                         matchedCore->rect.origin[1] + definition.grid * 0.5f},
                        stairRotation,
                        stairType));
                }
            }
            continue;
        }

        const SpaceUsage usage = StringToSpaceUsage(semanticSpace.type);
        if (usage == SpaceUsage::Unknown) {
            continue;
        }

        Rect candidate;
        bool hasCandidate = false;
        if (usage == SpaceUsage::Corridor || usage == SpaceUsage::Entrance) {
            if (nextCorridorCandidate < corridorCandidates.size()) {
                candidate = corridorCandidates[nextCorridorCandidate++];
                hasCandidate = true;
            }
        } else if (nextPerimeterCandidate < perimeterCandidates.size()) {
            candidate = perimeterCandidates[nextPerimeterCandidate++];
            hasCandidate = true;
        }

        if (!hasCandidate) {
            continue;
        }

        candidate = FitRectToArea(candidate,
            std::max(8.0f, semanticSpace.areaPreferred),
            std::max(2.0f, semanticSpace.constraints.minWidth));
        candidate.rectId = semanticSpace.spaceId;

        if (!RectFitsPlate(floorPlate, candidate, 0.1f)) {
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
    }

    if (outResolvedFloor.spaces.empty()) {
        outError = "Failed to synthesize office floor spaces inside sliced floor plates";
        return false;
    }

    return true;
}

} // namespace Building
} // namespace Moon
