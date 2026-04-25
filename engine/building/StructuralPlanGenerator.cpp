#include "StructuralPlanGenerator.h"
#include "BuildingGeometryUtils.h"

#include "../core/Mesh/Mesh.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>

namespace {

using namespace Moon::Building;
constexpr float kSearchEpsilon = 0.001f;

std::shared_ptr<Moon::Mesh> CreateExtrudedPlateMesh(const std::vector<GridPos2D>& outline,
                                                    float baseY,
                                                    float thickness) {
    if (outline.size() < 3) {
        return nullptr;
    }

    std::vector<GridPos2D> loop = outline;
    if (ComputeSignedArea(loop) < 0.0f) {
        std::reverse(loop.begin(), loop.end());
    }

    const float topY = baseY + thickness;
    std::vector<Moon::Vertex> vertices;
    std::vector<uint32_t> indices;

    float centerX = 0.0f;
    float centerZ = 0.0f;
    for (const auto& point : loop) {
        centerX += point[0];
        centerZ += point[1];
    }
    centerX /= static_cast<float>(loop.size());
    centerZ /= static_cast<float>(loop.size());

    const uint32_t topCenterIndex = static_cast<uint32_t>(vertices.size());
    vertices.emplace_back(Moon::Vector3(centerX, topY, centerZ), Moon::Vector3(0, 1, 0), Moon::Vector3(1, 1, 1));
    const uint32_t bottomCenterIndex = static_cast<uint32_t>(vertices.size());
    vertices.emplace_back(Moon::Vector3(centerX, baseY, centerZ), Moon::Vector3(0, -1, 0), Moon::Vector3(1, 1, 1));

    std::vector<uint32_t> topIndices;
    std::vector<uint32_t> bottomIndices;
    for (const auto& point : loop) {
        topIndices.push_back(static_cast<uint32_t>(vertices.size()));
        vertices.emplace_back(Moon::Vector3(point[0], topY, point[1]), Moon::Vector3(0, 1, 0), Moon::Vector3(1, 1, 1));
        bottomIndices.push_back(static_cast<uint32_t>(vertices.size()));
        vertices.emplace_back(Moon::Vector3(point[0], baseY, point[1]), Moon::Vector3(0, -1, 0), Moon::Vector3(1, 1, 1));
    }

    for (size_t i = 0; i < loop.size(); ++i) {
        const size_t next = (i + 1) % loop.size();
        indices.push_back(topCenterIndex);
        indices.push_back(topIndices[i]);
        indices.push_back(topIndices[next]);

        indices.push_back(bottomCenterIndex);
        indices.push_back(bottomIndices[next]);
        indices.push_back(bottomIndices[i]);
    }

    for (size_t i = 0; i < loop.size(); ++i) {
        const size_t next = (i + 1) % loop.size();
        const GridPos2D a = loop[i];
        const GridPos2D b = loop[next];
        const float edgeX = b[0] - a[0];
        const float edgeZ = b[1] - a[1];
        const float edgeLen = std::sqrt(edgeX * edgeX + edgeZ * edgeZ);
        if (edgeLen <= 0.0001f) {
            continue;
        }

        const Moon::Vector3 normal(edgeZ / edgeLen, 0.0f, -edgeX / edgeLen);
        const uint32_t sideBase = static_cast<uint32_t>(vertices.size());
        vertices.emplace_back(Moon::Vector3(a[0], baseY, a[1]), normal, Moon::Vector3(1, 1, 1));
        vertices.emplace_back(Moon::Vector3(b[0], baseY, b[1]), normal, Moon::Vector3(1, 1, 1));
        vertices.emplace_back(Moon::Vector3(b[0], topY, b[1]), normal, Moon::Vector3(1, 1, 1));
        vertices.emplace_back(Moon::Vector3(a[0], topY, a[1]), normal, Moon::Vector3(1, 1, 1));

        indices.push_back(sideBase + 0);
        indices.push_back(sideBase + 1);
        indices.push_back(sideBase + 2);
        indices.push_back(sideBase + 0);
        indices.push_back(sideBase + 2);
        indices.push_back(sideBase + 3);
    }

    auto mesh = std::make_shared<Moon::Mesh>();
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    return mesh->IsValid() ? mesh : nullptr;
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

bool RectFitsPlate(const FloorPlate& plate, const Rect& rect, float margin);

bool TryFitCoreRectAcrossPlates(const std::vector<FloorPlate>& plates,
                                const std::vector<GridPos2D>& candidateCenters,
                                float initialWidth,
                                float initialDepth,
                                float fitMargin,
                                float grid,
                                Rect& outRect) {
    for (const auto& center : candidateCenters) {
        float testWidth = initialWidth;
        float testDepth = initialDepth;
        for (int attempts = 0; attempts < 16; ++attempts) {
            const Rect candidate = MakeCenteredRect(center, testWidth, testDepth);
            const bool fits = std::all_of(plates.begin(), plates.end(),
                [&](const FloorPlate& plate) {
                    return RectFitsPlate(plate, candidate, fitMargin);
                });
            if (fits) {
                outRect = candidate;
                return true;
            }

            testWidth = std::max(grid * 4.0f, testWidth - grid);
            testDepth = std::max(grid * 4.0f, testDepth - grid);
        }
    }

    return false;
}

bool FindBestCoreRectAcrossPlates(const std::vector<FloorPlate>& plates,
                                  float boundsMinX,
                                  float boundsMinY,
                                  float boundsMaxX,
                                  float boundsMaxY,
                                  float initialWidth,
                                  float initialDepth,
                                  float fitMargin,
                                  float grid,
                                  Rect& outRect) {
    const float step = std::max(grid, 0.5f);
    Rect bestRect;
    float bestArea = 0.0f;

    for (float testWidth = initialWidth; testWidth >= std::max(grid * 4.0f, 2.0f); testWidth -= step) {
        for (float testDepth = initialDepth; testDepth >= std::max(grid * 4.0f, 2.0f); testDepth -= step) {
            for (float centerX = boundsMinX + testWidth * 0.5f;
                 centerX <= boundsMaxX - testWidth * 0.5f + kSearchEpsilon;
                 centerX += step) {
                for (float centerY = boundsMinY + testDepth * 0.5f;
                     centerY <= boundsMaxY - testDepth * 0.5f + kSearchEpsilon;
                     centerY += step) {
                    const Rect candidate = MakeCenteredRect({centerX, centerY}, testWidth, testDepth);
                    const bool fits = std::all_of(plates.begin(), plates.end(),
                        [&](const FloorPlate& plate) {
                            return RectFitsPlate(plate, candidate, fitMargin);
                        });
                    if (!fits) {
                        continue;
                    }

                    const float area = candidate.size[0] * candidate.size[1];
                    if (area > bestArea) {
                        bestArea = area;
                        bestRect = candidate;
                    }
                }
            }

            if (bestArea > 0.0f) {
                outRect = bestRect;
                return true;
            }
        }
    }

    return false;
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

bool CandidateColumnAllowed(const FloorPlate& plate,
                            const std::vector<Rect>& exclusionRects,
                            const GridPos2D& center) {
    for (const auto& exclusion : exclusionRects) {
        if (RectContainsPoint(exclusion, center)) {
            return false;
        }
    }

    if (plate.outline.size() >= 3) {
        return PointInOutline(plate.outline, center);
    }

    return center[0] > plate.origin[0] + 0.001f &&
           center[0] < plate.origin[0] + plate.size[0] - 0.001f &&
           center[1] > plate.origin[1] + 0.001f &&
           center[1] < plate.origin[1] + plate.size[1] - 0.001f;
}

std::vector<GridPos2D> BuildMassingColumnCandidates(const FloorPlate& plate) {
    const float insetX = std::min(std::max(0.9f, plate.size[0] * 0.08f), std::max(1.0f, plate.size[0] * 0.2f));
    const float insetY = std::min(std::max(0.9f, plate.size[1] * 0.08f), std::max(1.0f, plate.size[1] * 0.2f));
    const float minX = plate.origin[0] + insetX;
    const float maxX = plate.origin[0] + plate.size[0] - insetX;
    const float minY = plate.origin[1] + insetY;
    const float maxY = plate.origin[1] + plate.size[1] - insetY;
    const float centerX = plate.origin[0] + plate.size[0] * 0.5f;
    const float centerY = plate.origin[1] + plate.size[1] * 0.5f;

    std::vector<GridPos2D> candidates = {
        {minX, minY},
        {maxX, minY},
        {minX, maxY},
        {maxX, maxY}
    };

    if (plate.size[0] >= 18.0f) {
        candidates.push_back({centerX, minY});
        candidates.push_back({centerX, maxY});
    }
    if (plate.size[1] >= 18.0f) {
        candidates.push_back({minX, centerY});
        candidates.push_back({maxX, centerY});
    }
    if (plate.size[0] >= 24.0f && plate.size[1] >= 24.0f) {
        candidates.push_back({centerX, centerY});
    }

    return candidates;
}

bool IsCommercialOrOfficeTower(const BuildingDefinition& definition) {
    return definition.style.category == "commercial" || definition.style.category == "retail";
}

bool RequiresSharedVerticalZone(const BuildingDefinition& definition) {
    if (!(definition.style.category == "commercial" || definition.style.category == "retail")) {
        return false;
    }
    return definition.floors.size() >= 5;
}

struct PendingColumnPlan {
    FloorPlate plate;
    std::vector<Rect> exclusionRects;
};

FloorPlate BuildFloorPlateForMass(const Floor& floor,
                                  const Mass& mass,
                                  const std::vector<FloorPlate>& slicedFloorPlates) {
    auto slicedPlateIt = std::find_if(slicedFloorPlates.begin(), slicedFloorPlates.end(),
        [&](const FloorPlate& candidate) {
            return candidate.floorLevel == floor.level && candidate.massId == floor.massId;
        });
    if (slicedPlateIt != slicedFloorPlates.end()) {
        return *slicedPlateIt;
    }

    FloorPlate plate;
    plate.floorLevel = floor.level;
    plate.massId = floor.massId;
    plate.origin = mass.origin;
    plate.size = mass.size;
    const Rect massRect{"mass_outline", mass.origin, mass.size};
    plate.envelopeOutline = BuildRectOutline(massRect);
    plate.outline = plate.envelopeOutline;
    return plate;
}

bool IsExplicitCoreRect(const Rect& rect) {
    return ContainsToken(rect.rectId, "core") ||
           ContainsToken(rect.rectId, "elevator") ||
           ContainsToken(rect.rectId, "stair");
}

VerticalCore BuildExplicitCore(const Space& space,
                               const Rect& rect,
                               int floorLevel,
                               int maxFloorLevel) {
    VerticalCore core;
    core.coreId = rect.rectId;
    core.type = ClassifyCoreType(rect);
    core.rect = rect;
    core.floorFrom = floorLevel;
    core.floorTo = floorLevel;
    if (space.properties.hasStairs) {
        core.floorTo = std::max(core.floorTo, space.stairsConfig.connectToLevel);
    } else if (core.type == VerticalCoreType::Elevator && floorLevel < maxFloorLevel) {
        core.floorTo = floorLevel + 1;
    }
    return core;
}

void CollectFloorExclusionsAndCores(const Floor& floor,
                                    bool hasMassingRule,
                                    int maxFloorLevel,
                                    FloorPlate& plate,
                                    std::vector<VerticalCore>& outCores,
                                    std::vector<Rect>& outExclusionRects) {
    for (const auto& space : floor.spaces) {
        for (const auto& rect : space.rects) {
            if (!hasMassingRule && IsExplicitCoreRect(rect)) {
                outCores.push_back(BuildExplicitCore(space, rect, floor.level, maxFloorLevel));
                outExclusionRects.push_back(rect);
            }
        }

        Rect inferredVoid;
        if (InferMallVoidFromCorridor(space, inferredVoid)) {
            plate.voids.push_back(inferredVoid);
            outExclusionRects.push_back(inferredVoid);
        }
    }
}

void AppendFloorPlateMesh(const BuildingDefinition& definition,
                          const FloorPlate& plate,
                          GeneratedBuilding& outBuilding) {
    if (!plate.voids.empty()) {
        return;
    }

    const std::vector<GridPos2D>& slabOutline =
        plate.envelopeOutline.size() >= 3 ? plate.envelopeOutline : plate.outline;
    if (auto mesh = CreateExtrudedPlateMesh(slabOutline, GetFloorBaseHeight(definition, plate.floorLevel), 0.18f)) {
        GeneratedMeshPart meshPart;
        meshPart.partId = "floor_plate_mesh_" + std::to_string(plate.floorLevel);
        meshPart.material = "concrete_floor";
        meshPart.mesh = std::move(mesh);
        outBuilding.floorPlateMeshes.push_back(std::move(meshPart));
    }
}

void AppendActiveCoreExclusions(const GeneratedBuilding& outBuilding,
                                int floorLevel,
                                std::vector<Rect>& exclusionRects) {
    for (const auto& core : outBuilding.verticalCores) {
        if (core.floorFrom <= floorLevel && core.floorTo >= floorLevel) {
            exclusionRects.push_back(core.rect);
        }
    }
}

std::vector<GridPos2D> BuildRegularColumnCandidates(const FloorPlate& plate, float spacing) {
    std::vector<GridPos2D> candidates;
    for (float x = plate.origin[0] + spacing * 0.5f;
         x < plate.origin[0] + plate.size[0] - spacing * 0.25f;
         x += spacing) {
        for (float y = plate.origin[1] + spacing * 0.5f;
             y < plate.origin[1] + plate.size[1] - spacing * 0.25f;
             y += spacing) {
            candidates.push_back({x, y});
        }
    }
    return candidates;
}

std::vector<GridPos2D> BuildColumnCandidates(const FloorPlate& plate,
                                             bool useMassingColumnPattern,
                                             float spacing) {
    return useMassingColumnPattern
        ? BuildMassingColumnCandidates(plate)
        : BuildRegularColumnCandidates(plate, spacing);
}

void UpsertSupportColumn(GeneratedBuilding& outBuilding,
                         const GridPos2D& center,
                         float columnSize,
                         int floorLevel) {
    const int keyX = static_cast<int>(std::round(center[0] * 100.0f));
    const int keyY = static_cast<int>(std::round(center[1] * 100.0f));
    const std::string columnId = "column_" + std::to_string(keyX) + "_" + std::to_string(keyY);
    auto existing = std::find_if(outBuilding.supportColumns.begin(), outBuilding.supportColumns.end(),
        [&](const SupportColumn& column) { return column.columnId == columnId; });
    if (existing != outBuilding.supportColumns.end()) {
        existing->floorFrom = std::min(existing->floorFrom, 0);
        existing->floorTo = std::max(existing->floorTo, floorLevel);
        return;
    }

    SupportColumn column;
    column.columnId = columnId;
    column.center = center;
    column.width = columnSize;
    column.depth = columnSize;
    column.floorFrom = 0;
    column.floorTo = floorLevel;
    outBuilding.supportColumns.push_back(column);
}

void AddMassDrivenCoreIfNeeded(const BuildingDefinition& definition,
                               int maxFloorLevel,
                               GeneratedBuilding& outBuilding) {
    if (!outBuilding.verticalCores.empty() || outBuilding.floorPlates.empty()) {
        return;
    }
    if (!RequiresSharedVerticalZone(definition)) {
        return;
    }

    float minWidth = std::numeric_limits<float>::max();
    float minDepth = std::numeric_limits<float>::max();
    float minPlateArea = std::numeric_limits<float>::max();
    float centerXSum = 0.0f;
    float centerYSum = 0.0f;
    int count = 0;
    float boundsMinX = std::numeric_limits<float>::max();
    float boundsMinY = std::numeric_limits<float>::max();
    float boundsMaxX = -std::numeric_limits<float>::max();
    float boundsMaxY = -std::numeric_limits<float>::max();
    std::vector<GridPos2D> candidateCenters;

    for (const auto& plate : outBuilding.floorPlates) {
        minWidth = std::min(minWidth, plate.size[0]);
        minDepth = std::min(minDepth, plate.size[1]);
        minPlateArea = std::min(minPlateArea, plate.size[0] * plate.size[1]);
        const auto center = ComputePlateCenter(plate);
        centerXSum += center[0];
        centerYSum += center[1];
        ++count;
        candidateCenters.push_back(center);
        candidateCenters.push_back({
            plate.origin[0] + plate.size[0] * 0.5f,
            plate.origin[1] + plate.size[1] * 0.5f
        });
        boundsMinX = std::min(boundsMinX, plate.origin[0]);
        boundsMinY = std::min(boundsMinY, plate.origin[1]);
        boundsMaxX = std::max(boundsMaxX, plate.origin[0] + plate.size[0]);
        boundsMaxY = std::max(boundsMaxY, plate.origin[1] + plate.size[1]);
    }

    if (count == 0 || minWidth <= 0.0f || minDepth <= 0.0f) {
        return;
    }

    const float centerX = centerXSum / static_cast<float>(count);
    const float centerY = centerYSum / static_cast<float>(count);
    const float targetCoreArea = IsCommercialOrOfficeTower(definition)
        ? std::clamp(minPlateArea * 0.16f, 24.0f, 80.0f)
        : std::clamp(minPlateArea * 0.12f, 12.0f, 40.0f);
    float targetWidth = IsCommercialOrOfficeTower(definition)
        ? std::clamp(std::sqrt(targetCoreArea * 0.75f), 4.0f, std::max(4.0f, minWidth * 0.45f))
        : std::clamp(std::sqrt(targetCoreArea * 0.8f), 3.0f, std::max(3.0f, minWidth * 0.4f));
    float targetDepth = std::max(
        targetCoreArea / std::max(targetWidth, 0.5f),
        IsCommercialOrOfficeTower(definition) ? 5.0f : 4.0f);
    targetDepth = std::min(targetDepth, std::max(4.0f, minDepth * 0.5f));
    const float fitMargin = std::max(0.35f, definition.grid * 0.5f);
    const GridPos2D averageCenter = {centerX, centerY};
    candidateCenters.insert(candidateCenters.begin(), averageCenter);
    candidateCenters.push_back({
        (boundsMinX + boundsMaxX) * 0.5f,
        (boundsMinY + boundsMaxY) * 0.5f
    });
    for (float dx = -definition.grid * 4.0f; dx <= definition.grid * 4.0f; dx += definition.grid * 2.0f) {
        for (float dy = -definition.grid * 4.0f; dy <= definition.grid * 4.0f; dy += definition.grid * 2.0f) {
            candidateCenters.push_back({averageCenter[0] + dx, averageCenter[1] + dy});
        }
    }

    Rect overallCoreRect;
    bool fitted = TryFitCoreRectAcrossPlates(
        outBuilding.floorPlates,
        candidateCenters,
        targetWidth,
        targetDepth,
        fitMargin,
        definition.grid,
        overallCoreRect);
    if (!fitted) {
        fitted = FindBestCoreRectAcrossPlates(
            outBuilding.floorPlates,
            boundsMinX,
            boundsMinY,
            boundsMaxX,
            boundsMaxY,
            targetWidth,
            targetDepth,
            fitMargin,
            definition.grid,
            overallCoreRect);
    }
    if (!fitted) {
        return;
    }

    Rect sharedZoneRect = overallCoreRect;
    sharedZoneRect.rectId = "mass_vertical_zone";

    VerticalCore stairCore;
    stairCore.coreId = "mass_core_stair";
    stairCore.type = VerticalCoreType::Stair;
    stairCore.rect = sharedZoneRect;
    stairCore.floorFrom = 0;
    stairCore.floorTo = maxFloorLevel;
    outBuilding.verticalCores.push_back(stairCore);

    VerticalCore elevatorCore;
    elevatorCore.coreId = "mass_core_elevator";
    elevatorCore.type = VerticalCoreType::Elevator;
    elevatorCore.rect = sharedZoneRect;
    elevatorCore.floorFrom = 0;
    elevatorCore.floorTo = maxFloorLevel;
    outBuilding.verticalCores.push_back(elevatorCore);

    VerticalCore serviceCore;
    serviceCore.coreId = "mass_core_service";
    serviceCore.type = VerticalCoreType::Service;
    serviceCore.rect = sharedZoneRect;
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
    outBuilding.floorPlateMeshes.clear();
    outBuilding.verticalCores.clear();
    outBuilding.supportColumns.clear();
    outBuilding.programBlocks.clear();

    const bool hasMassingRule = std::any_of(definition.masses.begin(), definition.masses.end(),
        [](const Mass& mass) { return !mass.massingRuleAsset.empty(); });

    int maxFloorLevel = 0;
    for (const auto& floor : definition.floors) {
        maxFloorLevel = std::max(maxFloorLevel, floor.level);
    }

    std::vector<PendingColumnPlan> pendingColumnPlans;

    for (const auto& floor : definition.floors) {
        const Mass* mass = FindMassForFloor(definition, floor);
        if (!mass) {
            continue;
        }

        FloorPlate plate = BuildFloorPlateForMass(floor, *mass, slicedFloorPlates);

        std::vector<Rect> exclusionRects;
        CollectFloorExclusionsAndCores(
            floor,
            hasMassingRule,
            maxFloorLevel,
            plate,
            outBuilding.verticalCores,
            exclusionRects);

        outBuilding.floorPlates.push_back(plate);
        AppendFloorPlateMesh(definition, plate, outBuilding);
        pendingColumnPlans.push_back({plate, exclusionRects});
    }

    if (hasMassingRule) {
        AddMassDrivenCoreIfNeeded(definition, maxFloorLevel, outBuilding);
    }

    const float spacing = (definition.style.category == "commercial" || definition.style.category == "retail")
        ? 12.0f : 8.0f;
    const float columnSize = (definition.style.category == "commercial" || definition.style.category == "retail")
        ? 0.45f : 0.35f;

    const bool sparseHighriseColumns = RequiresSharedVerticalZone(definition);
    for (const auto& plan : pendingColumnPlans) {
        if (sparseHighriseColumns) {
            continue;
        }

        std::vector<Rect> exclusionRects = plan.exclusionRects;
        AppendActiveCoreExclusions(outBuilding, plan.plate.floorLevel, exclusionRects);

        const bool useMassingColumnPattern = hasMassingRule;
        const std::vector<GridPos2D> candidates = BuildColumnCandidates(plan.plate, useMassingColumnPattern, spacing);

        for (const auto& center : candidates) {
            if (!CandidateColumnAllowed(plan.plate, exclusionRects, center)) {
                continue;
            }

            UpsertSupportColumn(outBuilding, center, columnSize, plan.plate.floorLevel);
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
