#include "MassFloorPlateGenerator.h"

#include "../core/Assets/AssetPaths.h"
#include "../massing/MassMeshBuilder.h"
#include "../massing/MassRuleParser.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {

constexpr float kEpsilon = 1e-4f;
constexpr float kCommercialFacadeInset = 1.2f;
constexpr float kDefaultFacadeInset = 0.8f;

struct MeshBounds {
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();
};

struct SliceBounds {
    bool valid = false;
    float minX = 0.0f;
    float minZ = 0.0f;
    float maxX = 0.0f;
    float maxZ = 0.0f;
};

struct SliceSample {
    float x = 0.0f;
    float z = 0.0f;
};

struct CachedMassingBuild {
    Moon::Massing::MassBuildResult buildResult;
    MeshBounds bounds;
};

float SnapDown(float value, float grid) {
    if (grid <= 0.0f) {
        return value;
    }
    return std::floor((value + kEpsilon) / grid) * grid;
}

float SnapUp(float value, float grid) {
    if (grid <= 0.0f) {
        return value;
    }
    return std::ceil((value - kEpsilon) / grid) * grid;
}

float Clamp01(float value) {
    return std::max(0.0f, std::min(1.0f, value));
}

std::string ReadTextFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

MeshBounds ComputeBounds(const Moon::Massing::MassBuildResult& buildResult) {
    MeshBounds bounds;
    bool hasVertex = false;

    for (const auto& item : buildResult.items) {
        if (!item.mesh || !item.mesh->IsValid()) {
            continue;
        }
        for (const auto& vertex : item.mesh->GetVertices()) {
            hasVertex = true;
            bounds.minX = std::min(bounds.minX, vertex.position.x);
            bounds.minY = std::min(bounds.minY, vertex.position.y);
            bounds.minZ = std::min(bounds.minZ, vertex.position.z);
            bounds.maxX = std::max(bounds.maxX, vertex.position.x);
            bounds.maxY = std::max(bounds.maxY, vertex.position.y);
            bounds.maxZ = std::max(bounds.maxZ, vertex.position.z);
        }
    }

    if (!hasVertex) {
        bounds.minX = bounds.minY = bounds.minZ = 0.0f;
        bounds.maxX = bounds.maxY = bounds.maxZ = 0.0f;
    }

    return bounds;
}

void ExpandWithPoint(SliceBounds& bounds, float x, float z) {
    if (!bounds.valid) {
        bounds.valid = true;
        bounds.minX = bounds.maxX = x;
        bounds.minZ = bounds.maxZ = z;
        return;
    }

    bounds.minX = std::min(bounds.minX, x);
    bounds.maxX = std::max(bounds.maxX, x);
    bounds.minZ = std::min(bounds.minZ, z);
    bounds.maxZ = std::max(bounds.maxZ, z);
}

std::vector<SliceSample> ComputeConvexHull(std::vector<SliceSample> points) {
    if (points.size() <= 1) {
        return points;
    }

    std::sort(points.begin(), points.end(),
        [](const SliceSample& a, const SliceSample& b) {
            if (std::abs(a.x - b.x) > kEpsilon) {
                return a.x < b.x;
            }
            return a.z < b.z;
        });

    std::vector<SliceSample> uniquePoints;
    uniquePoints.reserve(points.size());
    for (const auto& point : points) {
        if (!uniquePoints.empty() &&
            std::abs(uniquePoints.back().x - point.x) <= kEpsilon &&
            std::abs(uniquePoints.back().z - point.z) <= kEpsilon) {
            continue;
        }
        uniquePoints.push_back(point);
    }

    if (uniquePoints.size() <= 2) {
        return uniquePoints;
    }

    auto cross = [](const SliceSample& o, const SliceSample& a, const SliceSample& b) {
        return (a.x - o.x) * (b.z - o.z) - (a.z - o.z) * (b.x - o.x);
    };

    std::vector<SliceSample> lower;
    for (const auto& point : uniquePoints) {
        while (lower.size() >= 2 &&
               cross(lower[lower.size() - 2], lower[lower.size() - 1], point) <= kEpsilon) {
            lower.pop_back();
        }
        lower.push_back(point);
    }

    std::vector<SliceSample> upper;
    for (auto it = uniquePoints.rbegin(); it != uniquePoints.rend(); ++it) {
        while (upper.size() >= 2 &&
               cross(upper[upper.size() - 2], upper[upper.size() - 1], *it) <= kEpsilon) {
            upper.pop_back();
        }
        upper.push_back(*it);
    }

    lower.pop_back();
    upper.pop_back();
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
}

std::vector<Moon::Building::GridPos2D> BuildInsetOutline(const std::vector<SliceSample>& hull,
                                                         float insetDistance,
                                                         float grid) {
    std::vector<Moon::Building::GridPos2D> outline;
    if (hull.size() < 3) {
        return outline;
    }

    float centroidX = 0.0f;
    float centroidZ = 0.0f;
    for (const auto& point : hull) {
        centroidX += point.x;
        centroidZ += point.z;
    }
    centroidX /= static_cast<float>(hull.size());
    centroidZ /= static_cast<float>(hull.size());

    outline.reserve(hull.size());
    for (const auto& point : hull) {
        float dx = point.x - centroidX;
        float dz = point.z - centroidZ;
        const float length = std::sqrt(dx * dx + dz * dz);
        if (length <= insetDistance + kEpsilon) {
            dx *= 0.8f;
            dz *= 0.8f;
        } else {
            const float scale = (length - insetDistance) / length;
            dx *= scale;
            dz *= scale;
        }

        outline.push_back({
            SnapDown(centroidX + dx, grid),
            SnapDown(centroidZ + dz, grid)
        });
    }

    return outline;
}

bool EdgeCrossesPlane(float yA, float yB, float planeY) {
    const float minY = std::min(yA, yB);
    const float maxY = std::max(yA, yB);
    return planeY >= minY - kEpsilon && planeY <= maxY + kEpsilon && std::abs(yA - yB) > kEpsilon;
}

void AddInterpolatedEdgePoint(const Moon::Vector3& a,
                              const Moon::Vector3& b,
                              float planeY,
                              SliceBounds& bounds) {
    if (!EdgeCrossesPlane(a.y, b.y, planeY)) {
        return;
    }

    const float t = (planeY - a.y) / (b.y - a.y);
    if (t < -kEpsilon || t > 1.0f + kEpsilon) {
        return;
    }

    const float x = a.x + (b.x - a.x) * t;
    const float z = a.z + (b.z - a.z) * t;
    ExpandWithPoint(bounds, x, z);
}

std::vector<SliceSample> ComputeSliceSamples(const Moon::Massing::MassBuildResult& buildResult,
                                             float planeY,
                                             float bandHalfThickness,
                                             SliceBounds* outBounds) {
    std::vector<SliceSample> samples;
    SliceBounds bounds;

    for (const auto& item : buildResult.items) {
        if (!item.mesh || !item.mesh->IsValid()) {
            continue;
        }

        const auto& vertices = item.mesh->GetVertices();
        const auto& indices = item.mesh->GetIndices();
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const auto& a = vertices[indices[i + 0]].position;
            const auto& b = vertices[indices[i + 1]].position;
            const auto& c = vertices[indices[i + 2]].position;

            auto addEdge = [&](const Moon::Vector3& from, const Moon::Vector3& to) {
                if (!EdgeCrossesPlane(from.y, to.y, planeY)) {
                    return;
                }
                const float t = (planeY - from.y) / (to.y - from.y);
                if (t < -kEpsilon || t > 1.0f + kEpsilon) {
                    return;
                }
                const float x = from.x + (to.x - from.x) * t;
                const float z = from.z + (to.z - from.z) * t;
                ExpandWithPoint(bounds, x, z);
                samples.push_back({x, z});
            };
            addEdge(a, b);
            addEdge(b, c);
            addEdge(c, a);
        }

        if (bounds.valid) {
            continue;
        }

        for (const auto& vertex : vertices) {
            if (std::abs(vertex.position.y - planeY) <= bandHalfThickness) {
                ExpandWithPoint(bounds, vertex.position.x, vertex.position.z);
                samples.push_back({vertex.position.x, vertex.position.z});
            }
        }
    }

    if (outBounds != nullptr) {
        *outBounds = bounds;
    }
    return samples;
}

bool LoadMassingBuild(const std::string& assetRef,
                      CachedMassingBuild& outBuild,
                      std::string& outError) {
    const std::string assetPath = Moon::Assets::BuildAssetPath(
        assetRef.rfind("massing/", 0) == 0 ? assetRef : std::string("massing/") + assetRef);
    const std::string json = ReadTextFile(assetPath);
    if (json.empty()) {
        outError = "Failed to read massing rule asset: " + assetPath;
        return false;
    }

    Moon::Massing::RuleSet ruleSet;
    if (!Moon::Massing::MassRuleParser::ParseFromString(json, ruleSet, outError)) {
        outError = "Failed to parse massing rule '" + assetRef + "': " + outError;
        return false;
    }

    if (!Moon::Massing::MassMeshBuilder::Build(ruleSet, outBuild.buildResult, outError)) {
        outError = "Failed to build massing rule '" + assetRef + "': " + outError;
        return false;
    }

    outBuild.bounds = ComputeBounds(outBuild.buildResult);
    return true;
}

float GetFacadeInset(const Moon::Building::BuildingDefinition& definition) {
    if (definition.style.category == "commercial" || definition.style.category == "retail") {
        return kCommercialFacadeInset;
    }
    return kDefaultFacadeInset;
}

} // namespace

namespace Moon {
namespace Building {

bool MassFloorPlateGenerator::Generate(const BuildingDefinition& definition,
                                       std::vector<FloorPlate>& outPlates,
                                       std::string& outError) const {
    outPlates.clear();

    float buildingTotalHeight = 0.0f;
    for (const auto& floor : definition.floors) {
        buildingTotalHeight = std::max(buildingTotalHeight, GetFloorBaseHeight(definition, floor.level) + floor.floorHeight);
    }
    if (buildingTotalHeight <= kEpsilon) {
        outError = "Building has no measurable floor height for mass slicing";
        return false;
    }

    std::unordered_map<std::string, CachedMassingBuild> buildCache;
    const float facadeInset = GetFacadeInset(definition);

    for (const auto& floor : definition.floors) {
        const Mass* mass = nullptr;
        for (const auto& candidate : definition.masses) {
            if (candidate.massId == floor.massId) {
                mass = &candidate;
                break;
            }
        }
        if (mass == nullptr || mass->massingRuleAsset.empty()) {
            continue;
        }

        auto buildIt = buildCache.find(mass->massingRuleAsset);
        if (buildIt == buildCache.end()) {
            CachedMassingBuild cachedBuild;
            if (!LoadMassingBuild(mass->massingRuleAsset, cachedBuild, outError)) {
                return false;
            }
            buildIt = buildCache.emplace(mass->massingRuleAsset, std::move(cachedBuild)).first;
        }

        const CachedMassingBuild& cachedBuild = buildIt->second;
        const float meshHeight = std::max(cachedBuild.bounds.maxY - cachedBuild.bounds.minY, kEpsilon);
        const float floorBase = GetFloorBaseHeight(definition, floor.level);
        const float sliceT = Clamp01((floorBase + floor.floorHeight * 0.5f) / buildingTotalHeight);
        const float bandT = std::max(0.015f, (floor.floorHeight / buildingTotalHeight) * 0.35f);
        const float planeY = cachedBuild.bounds.minY + meshHeight * sliceT;
        const float bandHalfThickness = meshHeight * bandT;

        SliceBounds slice;
        std::vector<SliceSample> samples = ComputeSliceSamples(cachedBuild.buildResult, planeY, bandHalfThickness, &slice);
        if (!slice.valid) {
            continue;
        }

        const float snappedMinX = SnapDown(slice.minX, definition.grid);
        const float snappedMinZ = SnapDown(slice.minZ, definition.grid);
        const float snappedMaxX = SnapUp(slice.maxX, definition.grid);
        const float snappedMaxZ = SnapUp(slice.maxZ, definition.grid);
        const std::vector<SliceSample> hull = ComputeConvexHull(std::move(samples));
        float width = std::max(definition.grid, snappedMaxX - snappedMinX);
        float depth = std::max(definition.grid, snappedMaxZ - snappedMinZ);
        float originX = snappedMinX;
        float originZ = snappedMinZ;

        const float usableInset = std::min(facadeInset,
            std::max(0.0f, std::min(width, depth) * 0.18f));
        const std::vector<GridPos2D> insetOutline = BuildInsetOutline(hull, usableInset, definition.grid);
        if (!insetOutline.empty()) {
            float outlineMinX = std::numeric_limits<float>::max();
            float outlineMinZ = std::numeric_limits<float>::max();
            float outlineMaxX = -std::numeric_limits<float>::max();
            float outlineMaxZ = -std::numeric_limits<float>::max();
            for (const auto& point : insetOutline) {
                outlineMinX = std::min(outlineMinX, point[0]);
                outlineMinZ = std::min(outlineMinZ, point[1]);
                outlineMaxX = std::max(outlineMaxX, point[0]);
                outlineMaxZ = std::max(outlineMaxZ, point[1]);
            }
            originX = outlineMinX;
            originZ = outlineMinZ;
            width = std::max(definition.grid, outlineMaxX - outlineMinX);
            depth = std::max(definition.grid, outlineMaxZ - outlineMinZ);
        }

        FloorPlate plate;
        plate.floorLevel = floor.level;
        plate.massId = floor.massId;
        plate.origin = {originX, originZ};
        plate.size = {width, depth};
        plate.outline = insetOutline;
        outPlates.push_back(plate);
    }

    if (outPlates.empty()) {
        outError = "No floor plates could be sliced from provided massing rules";
        return false;
    }

    std::sort(outPlates.begin(), outPlates.end(),
              [](const FloorPlate& a, const FloorPlate& b) { return a.floorLevel < b.floorLevel; });
    return true;
}

} // namespace Building
} // namespace Moon
