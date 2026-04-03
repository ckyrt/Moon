#include "MassFloorPlateGenerator.h"

#include "../core/Assets/AssetPaths.h"
#include "../massing/MassMeshBuilder.h"
#include "../massing/MassRuleParser.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {

constexpr float kEpsilon = 1e-4f;
constexpr float kCommercialFacadeInset = 0.6f;
constexpr float kDefaultFacadeInset = 0.4f;

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

struct SliceSegment {
    SliceSample a;
    SliceSample b;
};

struct OffsetLine {
    SliceSample point;
    SliceSample direction;
};

struct CachedMassingBuild {
    Moon::Massing::MassBuildResult buildResult;
    MeshBounds bounds;
};

struct SliceCandidate {
    SliceBounds bounds;
    std::vector<SliceSegment> segments;
    std::vector<SliceSample> outerLoop;
    float outerLoopArea = 0.0f;
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

float SnapNearest(float value, float grid) {
    if (grid <= 0.0f) {
        return value;
    }
    return std::round(value / grid) * grid;
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

bool NearlyEqual(float a, float b, float epsilon = kEpsilon) {
    return std::abs(a - b) <= epsilon;
}

bool NearlyEqual(const SliceSample& a, const SliceSample& b, float epsilon = kEpsilon) {
    return NearlyEqual(a.x, b.x, epsilon) && NearlyEqual(a.z, b.z, epsilon);
}

SliceSample SnapSample(const SliceSample& point, float snap) {
    if (snap <= 0.0f) {
        return point;
    }
    return {
        std::round(point.x / snap) * snap,
        std::round(point.z / snap) * snap
    };
}

std::vector<SliceSample> DeduplicatePoints(std::vector<SliceSample> points, float epsilon = kEpsilon) {
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
            NearlyEqual(uniquePoints.back(), point, epsilon)) {
            continue;
        }
        uniquePoints.push_back(point);
    }
    return uniquePoints;
}

std::vector<SliceSample> RemoveSequentialDuplicateLoopPoints(std::vector<SliceSample> points,
                                                             float epsilon = kEpsilon) {
    std::vector<SliceSample> cleaned;
    cleaned.reserve(points.size());
    for (const auto& point : points) {
        if (!cleaned.empty() && NearlyEqual(cleaned.back(), point, epsilon)) {
            continue;
        }
        cleaned.push_back(point);
    }
    if (cleaned.size() >= 2 && NearlyEqual(cleaned.front(), cleaned.back(), epsilon)) {
        cleaned.pop_back();
    }
    return cleaned;
}

bool IntersectEdgeWithPlane(const Moon::Vector3& a,
                            const Moon::Vector3& b,
                            float planeY,
                            SliceSample& outPoint) {
    const bool aOnPlane = NearlyEqual(a.y, planeY);
    const bool bOnPlane = NearlyEqual(b.y, planeY);
    if (aOnPlane && bOnPlane) {
        return false;
    }

    if (aOnPlane) {
        outPoint = {a.x, a.z};
        return true;
    }
    if (bOnPlane) {
        outPoint = {b.x, b.z};
        return true;
    }

    const float minY = std::min(a.y, b.y);
    const float maxY = std::max(a.y, b.y);
    if (planeY < minY - kEpsilon || planeY > maxY + kEpsilon || NearlyEqual(a.y, b.y)) {
        return false;
    }

    const float t = (planeY - a.y) / (b.y - a.y);
    if (t < -kEpsilon || t > 1.0f + kEpsilon) {
        return false;
    }

    outPoint = {
        a.x + (b.x - a.x) * t,
        a.z + (b.z - a.z) * t
    };
    return true;
}

std::vector<SliceSegment> ComputeSliceSegments(const Moon::Massing::MassBuildResult& buildResult,
                                               float planeY,
                                               float bandHalfThickness,
                                               SliceBounds* outBounds) {
    std::vector<SliceSegment> segments;
    SliceBounds bounds;
    const float snap = std::max(0.001f, bandHalfThickness * 0.1f);

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

            std::vector<SliceSample> hits;
            SliceSample point;
            if (IntersectEdgeWithPlane(a, b, planeY, point)) {
                hits.push_back(point);
            }
            if (IntersectEdgeWithPlane(b, c, planeY, point)) {
                hits.push_back(point);
            }
            if (IntersectEdgeWithPlane(c, a, planeY, point)) {
                hits.push_back(point);
            }

            hits = DeduplicatePoints(std::move(hits), snap);
            if (hits.size() < 2) {
                continue;
            }

            SliceSegment segment;
            segment.a = SnapSample(hits[0], snap);
            segment.b = SnapSample(hits[1], snap);
            if (NearlyEqual(segment.a, segment.b, snap)) {
                continue;
            }

            ExpandWithPoint(bounds, segment.a.x, segment.a.z);
            ExpandWithPoint(bounds, segment.b.x, segment.b.z);
            segments.push_back(segment);
        }
    }

    if (outBounds != nullptr) {
        *outBounds = bounds;
    }

    return segments;
}

std::vector<Moon::Building::GridPos2D> BuildInsetOutlineLegacy(const std::vector<SliceSample>& hull,
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

float ComputeSignedArea(const std::vector<SliceSample>& outline) {
    if (outline.size() < 3) {
        return 0.0f;
    }

    float area = 0.0f;
    for (size_t i = 0; i < outline.size(); ++i) {
        const auto& a = outline[i];
        const auto& b = outline[(i + 1) % outline.size()];
        area += a.x * b.z - b.x * a.z;
    }
    return area * 0.5f;
}

SliceSample Normalize(const SliceSample& vector) {
    const float length = std::sqrt(vector.x * vector.x + vector.z * vector.z);
    if (length <= kEpsilon) {
        return {};
    }
    return {vector.x / length, vector.z / length};
}

SliceSample LeftNormal(const SliceSample& direction) {
    return {-direction.z, direction.x};
}

float Dot(const SliceSample& a, const SliceSample& b) {
    return a.x * b.x + a.z * b.z;
}

float Cross(const SliceSample& a, const SliceSample& b) {
    return a.x * b.z - a.z * b.x;
}

float DistancePointToLine(const SliceSample& point,
                          const SliceSample& lineStart,
                          const SliceSample& lineEnd) {
    const SliceSample line{lineEnd.x - lineStart.x, lineEnd.z - lineStart.z};
    const float lineLength = std::sqrt(line.x * line.x + line.z * line.z);
    if (lineLength <= kEpsilon) {
        const float dx = point.x - lineStart.x;
        const float dz = point.z - lineStart.z;
        return std::sqrt(dx * dx + dz * dz);
    }

    const SliceSample offset{point.x - lineStart.x, point.z - lineStart.z};
    return std::abs(Cross(offset, line)) / lineLength;
}

bool PointOnSegment(const SliceSample& a, const SliceSample& b, const SliceSample& point) {
    const SliceSample ab{b.x - a.x, b.z - a.z};
    const SliceSample ap{point.x - a.x, point.z - a.z};
    if (std::abs(Cross(ab, ap)) > 0.01f) {
        return false;
    }

    const float dot = ap.x * ab.x + ap.z * ab.z;
    if (dot < -0.01f) {
        return false;
    }

    const float squaredLength = ab.x * ab.x + ab.z * ab.z;
    return dot <= squaredLength + 0.01f;
}

bool PointInPolygon(const std::vector<SliceSample>& polygon, const SliceSample& point) {
    if (polygon.size() < 3) {
        return false;
    }

    for (size_t i = 0; i < polygon.size(); ++i) {
        const auto& a = polygon[i];
        const auto& b = polygon[(i + 1) % polygon.size()];
        if (PointOnSegment(a, b, point)) {
            return true;
        }
    }

    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const auto& a = polygon[i];
        const auto& b = polygon[j];
        const bool intersects = ((a.z > point.z) != (b.z > point.z)) &&
            (point.x < (b.x - a.x) * (point.z - a.z) / ((b.z - a.z) + 0.000001f) + a.x);
        if (intersects) {
            inside = !inside;
        }
    }
    return inside;
}

bool SegmentIntersection(const SliceSample& a0,
                         const SliceSample& a1,
                         const SliceSample& b0,
                         const SliceSample& b1) {
    const SliceSample r{a1.x - a0.x, a1.z - a0.z};
    const SliceSample s{b1.x - b0.x, b1.z - b0.z};
    const float denom = Cross(r, s);
    const SliceSample qp{b0.x - a0.x, b0.z - a0.z};

    if (std::abs(denom) <= kEpsilon) {
        return false;
    }

    const float t = Cross(qp, s) / denom;
    const float u = Cross(qp, r) / denom;
    return t > kEpsilon && t < 1.0f - kEpsilon &&
           u > kEpsilon && u < 1.0f - kEpsilon;
}

bool IsSimplePolygon(const std::vector<SliceSample>& polygon) {
    if (polygon.size() < 3) {
        return false;
    }

    for (size_t i = 0; i < polygon.size(); ++i) {
        const SliceSample& a0 = polygon[i];
        const SliceSample& a1 = polygon[(i + 1) % polygon.size()];
        for (size_t j = i + 1; j < polygon.size(); ++j) {
            const size_t nextI = (i + 1) % polygon.size();
            const size_t nextJ = (j + 1) % polygon.size();
            if (i == j || i == nextJ || nextI == j) {
                continue;
            }
            if (i == 0 && nextJ == 0) {
                continue;
            }

            const SliceSample& b0 = polygon[j];
            const SliceSample& b1 = polygon[nextJ];
            if (SegmentIntersection(a0, a1, b0, b1)) {
                return false;
            }
        }
    }

    return true;
}

bool IntersectLines(const OffsetLine& a, const OffsetLine& b, SliceSample& outPoint) {
    const float denominator = Cross(a.direction, b.direction);
    if (std::abs(denominator) <= kEpsilon) {
        return false;
    }

    const SliceSample delta{b.point.x - a.point.x, b.point.z - a.point.z};
    const float t = Cross(delta, b.direction) / denominator;
    outPoint = {
        a.point.x + a.direction.x * t,
        a.point.z + a.direction.z * t
    };
    return true;
}

std::vector<SliceSample> BuildInsetLoop(const std::vector<SliceSample>& polygon,
                                        float insetDistance) {
    std::vector<SliceSample> insetLoop;
    if (polygon.size() < 3 || insetDistance <= kEpsilon) {
        return polygon;
    }

    const float signedArea = ComputeSignedArea(polygon);
    if (std::abs(signedArea) <= kEpsilon) {
        return {};
    }

    const float orientation = signedArea >= 0.0f ? 1.0f : -1.0f;
    insetLoop.reserve(polygon.size());

    for (size_t i = 0; i < polygon.size(); ++i) {
        const SliceSample& prev = polygon[(i + polygon.size() - 1) % polygon.size()];
        const SliceSample& curr = polygon[i];
        const SliceSample& next = polygon[(i + 1) % polygon.size()];

        const SliceSample edgeIn = Normalize({curr.x - prev.x, curr.z - prev.z});
        const SliceSample edgeOut = Normalize({next.x - curr.x, next.z - curr.z});
        if ((std::abs(edgeIn.x) <= kEpsilon && std::abs(edgeIn.z) <= kEpsilon) ||
            (std::abs(edgeOut.x) <= kEpsilon && std::abs(edgeOut.z) <= kEpsilon)) {
            return {};
        }

        const SliceSample inwardIn = {
            LeftNormal(edgeIn).x * orientation,
            LeftNormal(edgeIn).z * orientation
        };
        const SliceSample inwardOut = {
            LeftNormal(edgeOut).x * orientation,
            LeftNormal(edgeOut).z * orientation
        };

        const OffsetLine lineIn{
            {curr.x + inwardIn.x * insetDistance, curr.z + inwardIn.z * insetDistance},
            edgeIn
        };
        const OffsetLine lineOut{
            {curr.x + inwardOut.x * insetDistance, curr.z + inwardOut.z * insetDistance},
            edgeOut
        };

        SliceSample insetPoint;
        if (!IntersectLines(lineIn, lineOut, insetPoint)) {
            const SliceSample bisector = Normalize({
                inwardIn.x + inwardOut.x,
                inwardIn.z + inwardOut.z
            });
            if (std::abs(bisector.x) <= kEpsilon && std::abs(bisector.z) <= kEpsilon) {
                return {};
            }

            insetPoint = {
                curr.x + bisector.x * insetDistance,
                curr.z + bisector.z * insetDistance
            };
        }

        insetLoop.push_back(insetPoint);
    }

    return RemoveSequentialDuplicateLoopPoints(std::move(insetLoop), 0.01f);
}

bool IsValidInsetLoop(const std::vector<SliceSample>& sourceLoop,
                      const std::vector<SliceSample>& insetLoop) {
    if (insetLoop.size() < 3) {
        return false;
    }

    const float sourceArea = ComputeSignedArea(sourceLoop);
    const float insetArea = ComputeSignedArea(insetLoop);
    if (std::abs(insetArea) <= kEpsilon || sourceArea * insetArea <= 0.0f) {
        return false;
    }
    if (std::abs(insetArea) >= std::abs(sourceArea)) {
        return false;
    }
    if (!IsSimplePolygon(insetLoop)) {
        return false;
    }

    for (const auto& point : insetLoop) {
        if (!PointInPolygon(sourceLoop, point)) {
            return false;
        }
    }

    for (size_t i = 0; i < insetLoop.size(); ++i) {
        const auto& a = insetLoop[i];
        const auto& b = insetLoop[(i + 1) % insetLoop.size()];
        const SliceSample midpoint{
            (a.x + b.x) * 0.5f,
            (a.z + b.z) * 0.5f
        };
        if (!PointInPolygon(sourceLoop, midpoint)) {
            return false;
        }
    }

    return true;
}

std::vector<SliceSample> SimplifyLayoutLoop(const std::vector<SliceSample>& polygon,
                                            float tolerance) {
    if (polygon.size() <= 6 || tolerance <= kEpsilon) {
        return polygon;
    }

    std::vector<SliceSample> simplified = polygon;
    bool changed = true;
    int guard = 32;
    while (changed && simplified.size() > 6 && guard-- > 0) {
        changed = false;
        for (size_t i = 0; i < simplified.size(); ++i) {
            const size_t prevIndex = (i + simplified.size() - 1) % simplified.size();
            const size_t nextIndex = (i + 1) % simplified.size();
            const SliceSample& prev = simplified[prevIndex];
            const SliceSample& curr = simplified[i];
            const SliceSample& next = simplified[nextIndex];

            const SliceSample inDir = Normalize({curr.x - prev.x, curr.z - prev.z});
            const SliceSample outDir = Normalize({next.x - curr.x, next.z - curr.z});
            if ((std::abs(inDir.x) <= kEpsilon && std::abs(inDir.z) <= kEpsilon) ||
                (std::abs(outDir.x) <= kEpsilon && std::abs(outDir.z) <= kEpsilon)) {
                continue;
            }

            const float deviation = DistancePointToLine(curr, prev, next);
            const float turnCos = Dot(inDir, outDir);
            if (deviation <= tolerance && turnCos >= -0.2f) {
                simplified.erase(simplified.begin() + static_cast<std::ptrdiff_t>(i));
                changed = true;
                break;
            }
        }
    }

    return simplified.size() >= 3 ? simplified : polygon;
}

std::vector<Moon::Building::GridPos2D> BuildInsetOutline(const std::vector<SliceSample>& hull,
                                                         float insetDistance,
                                                         float grid) {
    if (hull.size() < 3) {
        return {};
    }
    const std::vector<SliceSample> layoutHull = SimplifyLayoutLoop(
        hull,
        std::max(grid * 2.0f, insetDistance * 1.25f));
    const std::array<float, 4> insetAttempts = {1.0f, 0.75f, 0.5f, 0.25f};
    for (const float scale : insetAttempts) {
        const float attemptInset = insetDistance * scale;
        std::vector<SliceSample> insetLoop = BuildInsetLoop(layoutHull, attemptInset);
        if (!IsValidInsetLoop(hull, insetLoop)) {
            continue;
        }

        std::vector<Moon::Building::GridPos2D> outline;
        outline.reserve(insetLoop.size());
        for (const auto& point : insetLoop) {
            outline.push_back({
                SnapNearest(point.x, grid),
                SnapNearest(point.z, grid)
            });
        }

        std::vector<SliceSample> snappedLoop;
        snappedLoop.reserve(outline.size());
        for (const auto& point : outline) {
            snappedLoop.push_back({point[0], point[1]});
        }
        snappedLoop = RemoveSequentialDuplicateLoopPoints(std::move(snappedLoop), 0.01f);
        if (IsValidInsetLoop(hull, snappedLoop)) {
            return outline;
        }
    }

    return BuildInsetOutlineLegacy(hull, insetDistance, grid);
}

std::vector<std::vector<SliceSample>> BuildSliceLoops(const std::vector<SliceSegment>& segments) {
    std::vector<std::vector<SliceSample>> loops;
    if (segments.empty()) {
        return loops;
    }

    std::vector<bool> used(segments.size(), false);
    for (size_t startIndex = 0; startIndex < segments.size(); ++startIndex) {
        if (used[startIndex]) {
            continue;
        }

        std::vector<SliceSample> loop;
        loop.push_back(segments[startIndex].a);
        SliceSample current = segments[startIndex].b;
        const SliceSample startPoint = segments[startIndex].a;
        used[startIndex] = true;

        int guard = static_cast<int>(segments.size()) * 4;
        while (guard-- > 0) {
            loop.push_back(current);
            if (NearlyEqual(current, startPoint, 0.01f)) {
                loop.pop_back();
                break;
            }

            bool foundNext = false;
            for (size_t candidateIndex = 0; candidateIndex < segments.size(); ++candidateIndex) {
                if (used[candidateIndex]) {
                    continue;
                }

                const SliceSegment& candidate = segments[candidateIndex];
                if (NearlyEqual(candidate.a, current, 0.01f)) {
                    current = candidate.b;
                    used[candidateIndex] = true;
                    foundNext = true;
                    break;
                }
                if (NearlyEqual(candidate.b, current, 0.01f)) {
                    current = candidate.a;
                    used[candidateIndex] = true;
                    foundNext = true;
                    break;
                }
            }

            if (!foundNext) {
                break;
            }
        }

        loop = RemoveSequentialDuplicateLoopPoints(std::move(loop), 0.01f);
        if (loop.size() >= 3) {
            loops.push_back(std::move(loop));
        }
    }

    return loops;
}

std::vector<Moon::Building::GridPos2D> BuildEnvelopeOutline(const std::vector<SliceSample>& hull,
                                                            float grid) {
    std::vector<Moon::Building::GridPos2D> outline;
    outline.reserve(hull.size());
    for (const auto& point : hull) {
        outline.push_back({
            SnapNearest(point.x, grid),
            SnapNearest(point.z, grid)
        });
    }
    return outline;
}

bool BuildBestSliceCandidate(const Moon::Massing::MassBuildResult& buildResult,
                             float bandMinY,
                             float bandMaxY,
                             float bandHalfThickness,
                             SliceCandidate& outCandidate) {
    outCandidate = {};

    const float span = std::max(0.0f, bandMaxY - bandMinY);
    const int sampleCount = 11;
    for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        const float t = sampleCount == 1
            ? 0.5f
            : static_cast<float>(sampleIndex) / static_cast<float>(sampleCount - 1);
        const float planeY = bandMinY + span * t;

        SliceBounds candidateBounds;
        std::vector<SliceSegment> candidateSegments = ComputeSliceSegments(
            buildResult,
            planeY,
            bandHalfThickness,
            &candidateBounds);
        if (!candidateBounds.valid || candidateSegments.empty()) {
            continue;
        }

        const std::vector<std::vector<SliceSample>> loops = BuildSliceLoops(candidateSegments);
        const std::vector<SliceSample>* outerLoop = nullptr;
        float outerLoopArea = 0.0f;
        for (const auto& loop : loops) {
            const float area = std::abs(ComputeSignedArea(loop));
            if (!outerLoop || area > outerLoopArea) {
                outerLoop = &loop;
                outerLoopArea = area;
            }
        }
        if (outerLoop == nullptr || outerLoop->size() < 3) {
            continue;
        }

        if (outerLoopArea > outCandidate.outerLoopArea) {
            outCandidate.bounds = candidateBounds;
            outCandidate.segments = std::move(candidateSegments);
            outCandidate.outerLoop = *outerLoop;
            outCandidate.outerLoopArea = outerLoopArea;
        }
    }

    return outCandidate.bounds.valid && outCandidate.outerLoop.size() >= 3;
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
    if (definition.style.facadeOffset > kEpsilon) {
        return definition.style.facadeOffset;
    }
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
        const float bandHalfThickness = meshHeight * bandT;

        const float floorTop = floorBase + floor.floorHeight;
        const float bandMinT = Clamp01(floorBase / buildingTotalHeight);
        const float bandMaxT = Clamp01(floorTop / buildingTotalHeight);
        const float bandMinY = cachedBuild.bounds.minY + meshHeight * bandMinT;
        const float bandMaxY = cachedBuild.bounds.minY + meshHeight * bandMaxT;

        SliceCandidate bestCandidate;
        if (!BuildBestSliceCandidate(
                cachedBuild.buildResult,
                std::min(bandMinY, bandMaxY),
                std::max(bandMinY, bandMaxY),
                bandHalfThickness,
                bestCandidate)) {
            continue;
        }
        const SliceBounds& slice = bestCandidate.bounds;

        const float snappedMinX = SnapDown(slice.minX, definition.grid);
        const float snappedMinZ = SnapDown(slice.minZ, definition.grid);
        const float snappedMaxX = SnapUp(slice.maxX, definition.grid);
        const float snappedMaxZ = SnapUp(slice.maxZ, definition.grid);
        const std::vector<SliceSample>* outerLoop = &bestCandidate.outerLoop;
        if (outerLoop == nullptr || outerLoop->size() < 3) {
            continue;
        }

        float width = std::max(definition.grid, snappedMaxX - snappedMinX);
        float depth = std::max(definition.grid, snappedMaxZ - snappedMinZ);
        float originX = snappedMinX;
        float originZ = snappedMinZ;

        const float usableInset = std::min(facadeInset,
            std::max(0.0f, std::min(width, depth) * 0.18f));
        const std::vector<GridPos2D> envelopeOutline = BuildEnvelopeOutline(*outerLoop, definition.grid);
        const std::vector<GridPos2D> insetOutline = BuildInsetOutline(*outerLoop, usableInset, definition.grid);
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
        plate.envelopeOutline = envelopeOutline;
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
