#include "MassMeshBuilder.h"

#include "MassRuleParser.h"
#include "../core/Assets/AssetPaths.h"
#include "../core/CSG/CSGOperations.h"
#include "../core/Geometry/MeshGenerator.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <iterator>
#include <unordered_set>

namespace Moon {
namespace Massing {

namespace {

struct BuildContext {
    std::vector<std::string> referenceStack;
};

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kEpsilon = 1e-4f;
constexpr float kDefaultCreaseAngleDegrees = 75.0f;
constexpr int kDefaultLoftSegmentsPerSpan = 4;

struct Bounds {
    Vector3 min;
    Vector3 max;
};

float ToRadians(float degrees) {
    return degrees * kPi / 180.0f;
}

Vector3 RotateX(const Vector3& v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return Vector3(v.x, v.y * c - v.z * s, v.y * s + v.z * c);
}

Vector3 RotateY(const Vector3& v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return Vector3(v.x * c + v.z * s, v.y, -v.x * s + v.z * c);
}

Vector3 RotateZ(const Vector3& v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return Vector3(v.x * c - v.y * s, v.x * s + v.y * c, v.z);
}

Vector3 ApplyRotation(const Vector3& value, const Vec3& rotationDegrees) {
    Vector3 result = value;
    result = RotateX(result, ToRadians(rotationDegrees[0]));
    result = RotateY(result, ToRadians(rotationDegrees[1]));
    result = RotateZ(result, ToRadians(rotationDegrees[2]));
    return result;
}

Vector3 ApplyPointTransform(const Vector3& value, const RuleTransform& transform) {
    Vector3 scaled(value.x * transform.scale[0], value.y * transform.scale[1], value.z * transform.scale[2]);
    Vector3 rotated = ApplyRotation(scaled, transform.rotation);
    return Vector3(rotated.x + transform.position[0], rotated.y + transform.position[1], rotated.z + transform.position[2]);
}

Vector3 ApplyNormalTransform(const Vector3& value, const RuleTransform& transform) {
    const float safeScaleX = std::abs(transform.scale[0]) <= kEpsilon ? 1.0f : transform.scale[0];
    const float safeScaleY = std::abs(transform.scale[1]) <= kEpsilon ? 1.0f : transform.scale[1];
    const float safeScaleZ = std::abs(transform.scale[2]) <= kEpsilon ? 1.0f : transform.scale[2];

    Vector3 adjusted(
        value.x / safeScaleX,
        value.y / safeScaleY,
        value.z / safeScaleZ
    );
    Vector3 rotated = ApplyRotation(adjusted, transform.rotation);
    const float length = rotated.Length();
    if (length <= kEpsilon) {
        return value;
    }
    return rotated * (1.0f / length);
}

std::string ReadUtf8TextFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }

    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (contents.size() >= 3 &&
        static_cast<unsigned char>(contents[0]) == 0xEF &&
        static_cast<unsigned char>(contents[1]) == 0xBB &&
        static_cast<unsigned char>(contents[2]) == 0xBF) {
        contents.erase(0, 3);
    }
    return contents;
}

std::string NormalizeReferencePath(const std::string& reference) {
    if (reference.empty()) {
        return {};
    }

    const bool isAbsoluteWindowsPath =
        reference.size() > 2 &&
        ((reference[1] == ':') &&
         (reference[2] == '/' || reference[2] == '\\'));
    if (isAbsoluteWindowsPath) {
        return Moon::Assets::NormalizeSlashes(reference);
    }

    const std::string assetRelative = reference.rfind("massing/", 0) == 0
        ? reference
        : std::string("massing/") + reference;
    return Moon::Assets::NormalizeSlashes(Moon::Assets::BuildAssetPath(assetRelative));
}

Vector3 SafeNormalize(const Vector3& value, const Vector3& fallback = Vector3(0.0f, 1.0f, 0.0f)) {
    const float length = value.Length();
    if (length <= kEpsilon) {
        return fallback;
    }
    return value * (1.0f / length);
}

std::shared_ptr<Mesh> CloneMesh(const std::shared_ptr<Mesh>& mesh) {
    if (!mesh) {
        return nullptr;
    }
    auto clone = std::make_shared<Mesh>();
    clone->SetVertices(mesh->GetVertices());
    clone->SetIndices(mesh->GetIndices());
    return clone;
}

void RecomputeNormals(Mesh& mesh) {
    std::vector<Vertex> vertices = mesh.GetVertices();
    const std::vector<uint32_t>& indices = mesh.GetIndices();

    for (Vertex& vertex : vertices) {
        vertex.normal = Vector3(0.0f, 0.0f, 0.0f);
    }

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        Vertex& a = vertices[indices[i + 0]];
        Vertex& b = vertices[indices[i + 1]];
        Vertex& c = vertices[indices[i + 2]];
        const Vector3 ab = b.position - a.position;
        const Vector3 ac = c.position - a.position;
        const Vector3 faceNormal = SafeNormalize(Vector3::Cross(ab, ac), Vector3(0.0f, 1.0f, 0.0f));
        a.normal = a.normal + faceNormal;
        b.normal = b.normal + faceNormal;
        c.normal = c.normal + faceNormal;
    }

    for (Vertex& vertex : vertices) {
        vertex.normal = SafeNormalize(vertex.normal);
    }

    mesh.SetVertices(std::move(vertices));
}

void SplitVerticesByCrease(Mesh& mesh, float creaseAngleDegrees = kDefaultCreaseAngleDegrees) {
    const std::vector<Vertex>& sourceVertices = mesh.GetVertices();
    const std::vector<uint32_t>& sourceIndices = mesh.GetIndices();
    if (sourceVertices.empty() || sourceIndices.size() < 3) {
        return;
    }

    struct CornerUse {
        size_t faceIndex = 0;
        int corner = 0;
    };

    const size_t faceCount = sourceIndices.size() / 3;
    std::vector<Vector3> faceNormals(faceCount, Vector3(0.0f, 1.0f, 0.0f));
    std::vector<std::vector<CornerUse>> cornersByVertex(sourceVertices.size());

    for (size_t faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
        const uint32_t i0 = sourceIndices[faceIndex * 3 + 0];
        const uint32_t i1 = sourceIndices[faceIndex * 3 + 1];
        const uint32_t i2 = sourceIndices[faceIndex * 3 + 2];
        if (i0 >= sourceVertices.size() || i1 >= sourceVertices.size() || i2 >= sourceVertices.size()) {
            continue;
        }

        const Vector3 ab = sourceVertices[i1].position - sourceVertices[i0].position;
        const Vector3 ac = sourceVertices[i2].position - sourceVertices[i0].position;
        faceNormals[faceIndex] = SafeNormalize(Vector3::Cross(ab, ac), Vector3(0.0f, 1.0f, 0.0f));

        cornersByVertex[i0].push_back({faceIndex, 0});
        cornersByVertex[i1].push_back({faceIndex, 1});
        cornersByVertex[i2].push_back({faceIndex, 2});
    }

    const float creaseDot = std::cos(ToRadians(creaseAngleDegrees));
    std::vector<Vertex> splitVertices;
    std::vector<uint32_t> splitIndices(sourceIndices.size(), 0);
    splitVertices.reserve(sourceIndices.size());

    for (size_t vertexIndex = 0; vertexIndex < cornersByVertex.size(); ++vertexIndex) {
        const std::vector<CornerUse>& incidentCorners = cornersByVertex[vertexIndex];
        if (incidentCorners.empty()) {
            continue;
        }

        std::vector<bool> used(incidentCorners.size(), false);
        for (size_t seed = 0; seed < incidentCorners.size(); ++seed) {
            if (used[seed]) {
                continue;
            }

            std::vector<size_t> cluster;
            cluster.push_back(seed);
            used[seed] = true;
            Vector3 clusterNormal = faceNormals[incidentCorners[seed].faceIndex];

            bool changed = true;
            while (changed) {
                changed = false;
                const Vector3 referenceNormal = SafeNormalize(clusterNormal, faceNormals[incidentCorners[seed].faceIndex]);
                for (size_t candidate = 0; candidate < incidentCorners.size(); ++candidate) {
                    if (used[candidate]) {
                        continue;
                    }

                    const Vector3& candidateNormal = faceNormals[incidentCorners[candidate].faceIndex];
                    if (Vector3::Dot(referenceNormal, candidateNormal) >= creaseDot) {
                        cluster.push_back(candidate);
                        used[candidate] = true;
                        clusterNormal = clusterNormal + candidateNormal;
                        changed = true;
                    }
                }
            }

            Vertex splitVertex = sourceVertices[vertexIndex];
            splitVertex.normal = SafeNormalize(clusterNormal, sourceVertices[vertexIndex].normal);
            const uint32_t splitIndex = static_cast<uint32_t>(splitVertices.size());
            splitVertices.push_back(splitVertex);

            for (size_t clusterIndex : cluster) {
                const CornerUse& use = incidentCorners[clusterIndex];
                splitIndices[use.faceIndex * 3 + static_cast<size_t>(use.corner)] = splitIndex;
            }
        }
    }

    mesh.SetVertices(std::move(splitVertices));
    mesh.SetIndices(std::move(splitIndices));
}

void ApplyTransformToMesh(Mesh& mesh, const RuleTransform& transform) {
    std::vector<Vertex> vertices = mesh.GetVertices();
    for (Vertex& vertex : vertices) {
        vertex.position = ApplyPointTransform(vertex.position, transform);
        vertex.normal = ApplyNormalTransform(vertex.normal, transform);
    }
    mesh.SetVertices(std::move(vertices));
}

Bounds ComputeBounds(const Mesh& mesh) {
    Bounds bounds;
    bounds.min = Vector3(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    bounds.max = Vector3(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());

    for (const Vertex& vertex : mesh.GetVertices()) {
        bounds.min.x = std::min(bounds.min.x, vertex.position.x);
        bounds.min.y = std::min(bounds.min.y, vertex.position.y);
        bounds.min.z = std::min(bounds.min.z, vertex.position.z);
        bounds.max.x = std::max(bounds.max.x, vertex.position.x);
        bounds.max.y = std::max(bounds.max.y, vertex.position.y);
        bounds.max.z = std::max(bounds.max.z, vertex.position.z);
    }

    return bounds;
}

float SignedArea(const std::vector<Vec2>& points) {
    float area = 0.0f;
    for (size_t i = 0; i < points.size(); ++i) {
        const Vec2& a = points[i];
        const Vec2& b = points[(i + 1) % points.size()];
        area += a[0] * b[1] - b[0] * a[1];
    }
    return area * 0.5f;
}

std::vector<Vec2> GetClosedProfilePoints(const Curve2D& curve) {
    std::vector<Vec2> points = curve.points;
    if (!points.empty()) {
        const Vec2& first = points.front();
        const Vec2& last = points.back();
        const float dx = first[0] - last[0];
        const float dy = first[1] - last[1];
        if (std::sqrt(dx * dx + dy * dy) <= kEpsilon) {
            points.pop_back();
        }
    }
    return points;
}

std::vector<float> BuildClosedCurveLengths(const std::vector<Vec2>& points) {
    std::vector<float> lengths;
    lengths.reserve(points.size() + 1);
    lengths.push_back(0.0f);
    float accumulated = 0.0f;
    for (size_t i = 0; i < points.size(); ++i) {
        const Vec2& a = points[i];
        const Vec2& b = points[(i + 1) % points.size()];
        const float dx = b[0] - a[0];
        const float dy = b[1] - a[1];
        accumulated += std::sqrt(dx * dx + dy * dy);
        lengths.push_back(accumulated);
    }
    return lengths;
}

std::vector<Vec2> ResampleClosedCurve(const Curve2D& curve, size_t sampleCount) {
    std::vector<Vec2> points = GetClosedProfilePoints(curve);
    if (points.size() < 3 || sampleCount < 3) {
        return points;
    }

    const std::vector<float> lengths = BuildClosedCurveLengths(points);
    const float totalLength = lengths.back();
    if (totalLength <= kEpsilon) {
        return points;
    }

    std::vector<Vec2> result;
    result.reserve(sampleCount);
    for (size_t sample = 0; sample < sampleCount; ++sample) {
        const float target = (totalLength * static_cast<float>(sample)) / static_cast<float>(sampleCount);
        size_t segment = 0;
        while (segment + 1 < lengths.size() && lengths[segment + 1] < target) {
            ++segment;
        }

        const Vec2& a = points[segment % points.size()];
        const Vec2& b = points[(segment + 1) % points.size()];
        const float start = lengths[segment];
        const float end = lengths[segment + 1];
        const float t = (end - start) <= kEpsilon ? 0.0f : (target - start) / (end - start);
        result.push_back({a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t});
    }
    return result;
}

Vec2 ComputeCentroid(const std::vector<Vec2>& points) {
    if (points.empty()) {
        return {0.0f, 0.0f};
    }

    Vec2 centroid{0.0f, 0.0f};
    for (const Vec2& point : points) {
        centroid[0] += point[0];
        centroid[1] += point[1];
    }
    centroid[0] /= static_cast<float>(points.size());
    centroid[1] /= static_cast<float>(points.size());
    return centroid;
}

size_t FindCanonicalStartIndex(const std::vector<Vec2>& points) {
    if (points.empty()) {
        return 0;
    }

    const Vec2 centroid = ComputeCentroid(points);
    size_t bestIndex = 0;
    float bestAngle = std::numeric_limits<float>::max();
    float bestDistance = -std::numeric_limits<float>::max();

    for (size_t i = 0; i < points.size(); ++i) {
        const Vec2 offset{points[i][0] - centroid[0], points[i][1] - centroid[1]};
        const float angle = std::atan2(offset[1], offset[0]);
        const float distance = offset[0] * offset[0] + offset[1] * offset[1];
        if (angle < bestAngle - kEpsilon || (std::abs(angle - bestAngle) <= kEpsilon && distance > bestDistance)) {
            bestAngle = angle;
            bestDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex;
}

std::vector<Vec2> RotateClosedPoints(const std::vector<Vec2>& points, size_t startIndex) {
    if (points.empty()) {
        return {};
    }

    std::vector<Vec2> rotated;
    rotated.reserve(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        rotated.push_back(points[(startIndex + i) % points.size()]);
    }
    return rotated;
}

std::vector<Vec2> NormalizeClosedProfileOrientation(const std::vector<Vec2>& points) {
    if (points.size() < 3) {
        return points;
    }

    std::vector<Vec2> normalized = points;
    if (SignedArea(normalized) < 0.0f) {
        std::reverse(normalized.begin(), normalized.end());
    }
    return RotateClosedPoints(normalized, FindCanonicalStartIndex(normalized));
}

float ComputeProfileAlignmentCost(const std::vector<Vec2>& reference, const std::vector<Vec2>& candidate, size_t shift) {
    float cost = 0.0f;
    for (size_t i = 0; i < reference.size(); ++i) {
        const Vec2& a = reference[i];
        const Vec2& b = candidate[(i + shift) % candidate.size()];
        const float dx = a[0] - b[0];
        const float dy = a[1] - b[1];
        cost += dx * dx + dy * dy;
    }
    return cost;
}

std::vector<Vec2> AlignProfileToReference(const std::vector<Vec2>& reference, const std::vector<Vec2>& candidate) {
    if (reference.size() != candidate.size() || candidate.empty()) {
        return candidate;
    }

    size_t bestShift = 0;
    float bestCost = std::numeric_limits<float>::max();
    for (size_t shift = 0; shift < candidate.size(); ++shift) {
        const float cost = ComputeProfileAlignmentCost(reference, candidate, shift);
        if (cost < bestCost) {
            bestCost = cost;
            bestShift = shift;
        }
    }

    return RotateClosedPoints(candidate, bestShift);
}

void AddTriangle(std::vector<uint32_t>& indices, uint32_t a, uint32_t b, uint32_t c) {
    indices.push_back(a);
    indices.push_back(b);
    indices.push_back(c);
}

void AddQuad(std::vector<uint32_t>& indices, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    AddTriangle(indices, a, b, c);
    AddTriangle(indices, a, c, d);
}

void AddCapFan(
    std::vector<Vertex>& vertices,
    std::vector<uint32_t>& indices,
    const std::vector<Vec2>& points,
    uint32_t ringOffset,
    const Vector3& centerPosition,
    const Vector3& normal,
    bool reverseWinding)
{
    if (points.size() < 3) {
        return;
    }

    const uint32_t centerIndex = static_cast<uint32_t>(vertices.size());
    vertices.emplace_back(centerPosition, normal, Vector3(1, 1, 1), 1.0f, Vector2(0.5f, 0.5f));

    for (size_t i = 0; i < points.size(); ++i) {
        const uint32_t current = ringOffset + static_cast<uint32_t>(i);
        const uint32_t next = ringOffset + static_cast<uint32_t>((i + 1) % points.size());
        if (reverseWinding) {
            AddTriangle(indices, centerIndex, next, current);
        } else {
            AddTriangle(indices, centerIndex, current, next);
        }
    }
}

std::shared_ptr<Mesh> MakeMesh(std::vector<Vertex>&& vertices, std::vector<uint32_t>&& indices) {
    auto mesh = std::make_shared<Mesh>();
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    RecomputeNormals(*mesh);
    return mesh;
}
std::shared_ptr<Mesh> CreateExtrudeMesh(const Curve2D& profile, float height) {
    std::vector<Vec2> points = GetClosedProfilePoints(profile);
    if (points.size() < 3) {
        return nullptr;
    }

    const bool clockwise = SignedArea(points) < 0.0f;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(points.size() * 2);

    for (const Vec2& point : points) {
        vertices.emplace_back(Vector3(point[0], 0.0f, point[1]), Vector3(0, -1, 0), Vector3(1,1,1), 1.0f, Vector2(point[0], point[1]));
    }
    for (const Vec2& point : points) {
        vertices.emplace_back(Vector3(point[0], height, point[1]), Vector3(0, 1, 0), Vector3(1,1,1), 1.0f, Vector2(point[0], point[1]));
    }

    const uint32_t topOffset = static_cast<uint32_t>(points.size());
    for (size_t i = 1; i + 1 < points.size(); ++i) {
        if (clockwise) {
            AddTriangle(indices, 0, static_cast<uint32_t>(i + 1), static_cast<uint32_t>(i));
            AddTriangle(indices, topOffset, topOffset + static_cast<uint32_t>(i), topOffset + static_cast<uint32_t>(i + 1));
        } else {
            AddTriangle(indices, 0, static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1));
            AddTriangle(indices, topOffset, topOffset + static_cast<uint32_t>(i + 1), topOffset + static_cast<uint32_t>(i));
        }
    }

    for (size_t i = 0; i < points.size(); ++i) {
        const uint32_t next = static_cast<uint32_t>((i + 1) % points.size());
        const uint32_t a = static_cast<uint32_t>(i);
        const uint32_t b = next;
        const uint32_t c = topOffset + next;
        const uint32_t d = topOffset + static_cast<uint32_t>(i);
        if (clockwise) {
            AddQuad(indices, a, b, c, d);
        } else {
            AddQuad(indices, a, d, c, b);
        }
    }

    return MakeMesh(std::move(vertices), std::move(indices));
}

std::shared_ptr<Mesh> CreateRevolveMesh(const Curve2D& profile, int segments) {
    if (profile.points.size() < 2 || segments < 3) {
        return nullptr;
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    const size_t profileCount = profile.points.size();
    vertices.reserve(profileCount * static_cast<size_t>(segments) + 2);

    for (int seg = 0; seg < segments; ++seg) {
        const float angle = (kTwoPi * static_cast<float>(seg)) / static_cast<float>(segments);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        for (const Vec2& point : profile.points) {
            const float radius = std::max(0.0f, point[0]);
            vertices.emplace_back(Vector3(radius * c, point[1], radius * s), Vector3(c, 0.0f, s), Vector3(1,1,1));
        }
    }

    for (int seg = 0; seg < segments; ++seg) {
        const int nextSeg = (seg + 1) % segments;
        for (size_t i = 0; i + 1 < profileCount; ++i) {
            const uint32_t a = static_cast<uint32_t>(seg * static_cast<int>(profileCount) + static_cast<int>(i));
            const uint32_t b = static_cast<uint32_t>(nextSeg * static_cast<int>(profileCount) + static_cast<int>(i));
            const uint32_t c = b + 1;
            const uint32_t d = a + 1;
            AddQuad(indices, a, b, c, d);
        }
    }

    const float bottomRadius = std::max(0.0f, profile.points.front()[0]);
    const float topRadius = std::max(0.0f, profile.points.back()[0]);
    if (bottomRadius > kEpsilon) {
        const uint32_t centerIndex = static_cast<uint32_t>(vertices.size());
        vertices.emplace_back(Vector3(0.0f, profile.points.front()[1], 0.0f), Vector3(0.0f, -1.0f, 0.0f), Vector3(1,1,1));
        for (int seg = 0; seg < segments; ++seg) {
            const int nextSeg = (seg + 1) % segments;
            AddTriangle(indices, centerIndex, static_cast<uint32_t>(nextSeg * static_cast<int>(profileCount)), static_cast<uint32_t>(seg * static_cast<int>(profileCount)));
        }
    }
    if (topRadius > kEpsilon) {
        const uint32_t centerIndex = static_cast<uint32_t>(vertices.size());
        vertices.emplace_back(Vector3(0.0f, profile.points.back()[1], 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(1,1,1));
        for (int seg = 0; seg < segments; ++seg) {
            const int nextSeg = (seg + 1) % segments;
            AddTriangle(indices, centerIndex, static_cast<uint32_t>(seg * static_cast<int>(profileCount) + static_cast<int>(profileCount - 1)), static_cast<uint32_t>(nextSeg * static_cast<int>(profileCount) + static_cast<int>(profileCount - 1)));
        }
    }

    return MakeMesh(std::move(vertices), std::move(indices));
}

void BuildSweepFrame(const std::vector<Vec3>& path, size_t index, Vector3& outRight, Vector3& outUp, Vector3& outForward) {
    Vector3 prev = index > 0 ? Vector3(path[index][0] - path[index - 1][0], path[index][1] - path[index - 1][1], path[index][2] - path[index - 1][2]) : Vector3(0,0,0);
    Vector3 next = index + 1 < path.size() ? Vector3(path[index + 1][0] - path[index][0], path[index + 1][1] - path[index][1], path[index + 1][2] - path[index][2]) : Vector3(0,0,0);
    outForward = SafeNormalize(prev + next, index + 1 < path.size() ? SafeNormalize(next) : SafeNormalize(prev, Vector3(0,0,1)));
    Vector3 worldUp(0.0f, 1.0f, 0.0f);
    if (std::abs(Vector3::Dot(outForward, worldUp)) > 0.98f) {
        worldUp = Vector3(1.0f, 0.0f, 0.0f);
    }
    outRight = SafeNormalize(Vector3::Cross(worldUp, outForward), Vector3(1.0f, 0.0f, 0.0f));
    outUp = SafeNormalize(Vector3::Cross(outForward, outRight), Vector3(0.0f, 1.0f, 0.0f));
}
std::vector<Vector3> BuildSweepTangents(const std::vector<Vec3>& path) {
    std::vector<Vector3> tangents(path.size(), Vector3(0.0f, 0.0f, 1.0f));
    if (path.size() < 2) {
        return tangents;
    }
    for (size_t i = 0; i < path.size(); ++i) {
        Vector3 tangent(0.0f, 0.0f, 0.0f);
        if (i > 0) {
            tangent = tangent + Vector3(
                path[i][0] - path[i - 1][0],
                path[i][1] - path[i - 1][1],
                path[i][2] - path[i - 1][2]
            );
        }
        if (i + 1 < path.size()) {
            tangent = tangent + Vector3(
                path[i + 1][0] - path[i][0],
                path[i + 1][1] - path[i][1],
                path[i + 1][2] - path[i][2]
            );
        }
        tangents[i] = SafeNormalize(tangent, Vector3(0.0f, 0.0f, 1.0f));
    }
    return tangents;
}
void BuildParallelTransportFrames(
    const std::vector<Vec3>& path,
    std::vector<Vector3>& outRights,
    std::vector<Vector3>& outUps,
    std::vector<Vector3>& outForwards)
{
    outRights.clear();
    outUps.clear();
    outForwards = BuildSweepTangents(path);
    if (path.empty()) {
        return;
    }
    outRights.resize(path.size());
    outUps.resize(path.size());
    Vector3 seedUp(0.0f, 1.0f, 0.0f);
    if (std::abs(Vector3::Dot(outForwards.front(), seedUp)) > 0.98f) {
        seedUp = Vector3(1.0f, 0.0f, 0.0f);
    }
    outRights.front() = SafeNormalize(Vector3::Cross(seedUp, outForwards.front()), Vector3(1.0f, 0.0f, 0.0f));
    outUps.front() = SafeNormalize(Vector3::Cross(outForwards.front(), outRights.front()), Vector3(0.0f, 1.0f, 0.0f));
    for (size_t i = 1; i < path.size(); ++i) {
        const Vector3 prevForward = outForwards[i - 1];
        const Vector3 currForward = outForwards[i];
        Vector3 right = outRights[i - 1] - currForward * Vector3::Dot(outRights[i - 1], currForward);
        if (right.Length() <= kEpsilon) {
            right = Vector3::Cross(outUps[i - 1], currForward);
        }
        if (right.Length() <= kEpsilon) {
            Vector3 fallbackUp(0.0f, 1.0f, 0.0f);
            if (std::abs(Vector3::Dot(currForward, fallbackUp)) > 0.98f) {
                fallbackUp = Vector3(1.0f, 0.0f, 0.0f);
            }
            right = Vector3::Cross(fallbackUp, currForward);
        }
        outRights[i] = SafeNormalize(right, outRights[i - 1]);
        outUps[i] = SafeNormalize(Vector3::Cross(currForward, outRights[i]), outUps[i - 1]);
        if (Vector3::Dot(prevForward, currForward) < -0.999f) {
            outRights[i] = outRights[i - 1] * -1.0f;
            outUps[i] = outUps[i - 1] * -1.0f;
        }
    }
}

std::shared_ptr<Mesh> CreateSweepMesh(const Curve2D& profile, const Curve3D& path) {
    std::vector<Vec2> points = GetClosedProfilePoints(profile);
    if (points.size() < 3 || path.points.size() < 2) {
        return nullptr;
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    const size_t ringSize = points.size();
    vertices.reserve(ringSize * path.points.size() + 2);

    for (size_t i = 0; i < path.points.size(); ++i) {
        Vector3 right, up, forward;
        BuildSweepFrame(path.points, i, right, up, forward);
        const Vector3 center(path.points[i][0], path.points[i][1], path.points[i][2]);
        for (const Vec2& point : points) {
            const Vector3 position = center + right * point[0] + up * point[1];
            vertices.emplace_back(position, up, Vector3(1,1,1), 1.0f, Vector2(point[0], point[1]));
        }
    }

    for (size_t ring = 0; ring + 1 < path.points.size(); ++ring) {
        const uint32_t currentOffset = static_cast<uint32_t>(ring * ringSize);
        const uint32_t nextOffset = static_cast<uint32_t>((ring + 1) * ringSize);
        for (size_t i = 0; i < ringSize; ++i) {
            const uint32_t next = static_cast<uint32_t>((i + 1) % ringSize);
            AddQuad(indices, currentOffset + static_cast<uint32_t>(i), currentOffset + next, nextOffset + next, nextOffset + static_cast<uint32_t>(i));
        }
    }

    Vector3 startRight, startUp, startForward;
    BuildSweepFrame(path.points, 0, startRight, startUp, startForward);
    Vector3 endRight, endUp, endForward;
    BuildSweepFrame(path.points, path.points.size() - 1, endRight, endUp, endForward);

    const Vector3 startCenter(path.points.front()[0], path.points.front()[1], path.points.front()[2]);
    const Vector3 endCenter(path.points.back()[0], path.points.back()[1], path.points.back()[2]);
    const bool clockwise = SignedArea(points) < 0.0f;

    AddCapFan(vertices, indices, points, 0u, startCenter, startForward * -1.0f, !clockwise);
    AddCapFan(vertices, indices, points, static_cast<uint32_t>((path.points.size() - 1) * ringSize), endCenter, endForward, clockwise);

    return MakeMesh(std::move(vertices), std::move(indices));
}

std::vector<float> BuildLoftLevels(const RuleNode& node, size_t profileCount) {
    std::vector<float> levels;
    if (node.params.contains("levels") && node.params["levels"].is_array() && node.params["levels"].size() == profileCount) {
        for (const auto& value : node.params["levels"]) {
            levels.push_back(value.get<float>());
        }
        return levels;
    }

    const float height = node.params.value("height", 10.0f);
    const float step = profileCount > 1 ? height / static_cast<float>(profileCount - 1) : 0.0f;
    for (size_t i = 0; i < profileCount; ++i) {
        levels.push_back(step * static_cast<float>(i));
    }
    return levels;
}

int GetLoftSegmentsPerSpan(const RuleNode& node) {
    return std::max(1, node.params.value("segments_per_span", kDefaultLoftSegmentsPerSpan));
}

std::shared_ptr<Mesh> CreateLoftMesh(const RuleNode& node) {
    if (node.profiles.size() < 2) {
        return nullptr;
    }

    size_t sampleCount = 0;
    for (const Curve2D& profile : node.profiles) {
        sampleCount = std::max(sampleCount, GetClosedProfilePoints(profile).size());
    }
    sampleCount = std::max<size_t>(sampleCount, 8);

    std::vector<std::vector<Vec2>> sampledProfiles;
    for (const Curve2D& profile : node.profiles) {
        sampledProfiles.push_back(NormalizeClosedProfileOrientation(ResampleClosedCurve(profile, sampleCount)));
    }
    for (size_t i = 1; i < sampledProfiles.size(); ++i) {
        sampledProfiles[i] = AlignProfileToReference(sampledProfiles[i - 1], sampledProfiles[i]);
    }

    const std::vector<float> levels = BuildLoftLevels(node, node.profiles.size());
    const int segmentsPerSpan = GetLoftSegmentsPerSpan(node);
    std::vector<std::vector<Vec2>> loftProfiles;
    std::vector<float> loftLevels;
    loftProfiles.reserve((sampledProfiles.size() - 1) * static_cast<size_t>(segmentsPerSpan) + 1);
    loftLevels.reserve(loftProfiles.capacity());

    for (size_t profileIndex = 0; profileIndex + 1 < sampledProfiles.size(); ++profileIndex) {
        const std::vector<Vec2>& current = sampledProfiles[profileIndex];
        const std::vector<Vec2>& next = sampledProfiles[profileIndex + 1];
        const float currentLevel = levels[profileIndex];
        const float nextLevel = levels[profileIndex + 1];

        for (int segment = 0; segment < segmentsPerSpan; ++segment) {
            if (profileIndex > 0 && segment == 0) {
                continue;
            }

            const float t = static_cast<float>(segment) / static_cast<float>(segmentsPerSpan);
            std::vector<Vec2> interpolated;
            interpolated.reserve(sampleCount);
            for (size_t pointIndex = 0; pointIndex < sampleCount; ++pointIndex) {
                const Vec2& a = current[pointIndex];
                const Vec2& b = next[pointIndex];
                interpolated.push_back({
                    a[0] + (b[0] - a[0]) * t,
                    a[1] + (b[1] - a[1]) * t
                });
            }
            loftProfiles.push_back(std::move(interpolated));
            loftLevels.push_back(currentLevel + (nextLevel - currentLevel) * t);
        }
    }
    loftProfiles.push_back(sampledProfiles.back());
    loftLevels.push_back(levels.back());

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(sampleCount * loftProfiles.size());

    for (size_t profileIndex = 0; profileIndex < loftProfiles.size(); ++profileIndex) {
        for (const Vec2& point : loftProfiles[profileIndex]) {
            vertices.emplace_back(Vector3(point[0], loftLevels[profileIndex], point[1]), Vector3(0,1,0), Vector3(1,1,1), 1.0f, Vector2(point[0], point[1]));
        }
    }

    for (size_t ring = 0; ring + 1 < loftProfiles.size(); ++ring) {
        const uint32_t currentOffset = static_cast<uint32_t>(ring * sampleCount);
        const uint32_t nextOffset = static_cast<uint32_t>((ring + 1) * sampleCount);
        for (size_t i = 0; i < sampleCount; ++i) {
            const uint32_t next = static_cast<uint32_t>((i + 1) % sampleCount);
            AddQuad(indices,
                currentOffset + static_cast<uint32_t>(i),
                nextOffset + static_cast<uint32_t>(i),
                nextOffset + next,
                currentOffset + next);
        }
    }

    Vector3 bottomCenter(0.0f, loftLevels.front(), 0.0f);
    for (const Vec2& point : loftProfiles.front()) {
        bottomCenter.x += point[0];
        bottomCenter.z += point[1];
    }
    bottomCenter.x /= static_cast<float>(sampleCount);
    bottomCenter.z /= static_cast<float>(sampleCount);

    Vector3 topCenter(0.0f, loftLevels.back(), 0.0f);
    for (const Vec2& point : loftProfiles.back()) {
        topCenter.x += point[0];
        topCenter.z += point[1];
    }
    topCenter.x /= static_cast<float>(sampleCount);
    topCenter.z /= static_cast<float>(sampleCount);

    AddCapFan(vertices, indices, loftProfiles.front(), 0u, bottomCenter, Vector3(0.0f, -1.0f, 0.0f), false);
    AddCapFan(vertices, indices, loftProfiles.back(), static_cast<uint32_t>((loftProfiles.size() - 1) * sampleCount), topCenter, Vector3(0.0f, 1.0f, 0.0f), true);

    return MakeMesh(std::move(vertices), std::move(indices));
}
void ApplyTaper(Mesh& mesh, float bottomScale, float topScale) {
    const Bounds bounds = ComputeBounds(mesh);
    const float height = std::max(bounds.max.y - bounds.min.y, kEpsilon);
    std::vector<Vertex> vertices = mesh.GetVertices();
    for (Vertex& vertex : vertices) {
        const float t = (vertex.position.y - bounds.min.y) / height;
        const float scale = bottomScale + (topScale - bottomScale) * t;
        vertex.position.x *= scale;
        vertex.position.z *= scale;
    }
    mesh.SetVertices(std::move(vertices));
    RecomputeNormals(mesh);
}

void SubdivideMesh(Mesh& mesh, int iterations) {
    if (iterations <= 0) {
        return;
    }

    std::vector<Vertex> vertices = mesh.GetVertices();
    std::vector<uint32_t> indices = mesh.GetIndices();

    for (int iteration = 0; iteration < iterations; ++iteration) {
        std::vector<Vertex> nextVertices = vertices;
        std::vector<uint32_t> nextIndices;
        nextIndices.reserve(indices.size() * 4);

        struct EdgeKey {
            uint32_t a;
            uint32_t b;

            bool operator==(const EdgeKey& other) const {
                return a == other.a && b == other.b;
            }
        };

        struct EdgeKeyHasher {
            size_t operator()(const EdgeKey& key) const {
                return (static_cast<size_t>(key.a) << 32) ^ static_cast<size_t>(key.b);
            }
        };

        std::unordered_map<EdgeKey, uint32_t, EdgeKeyHasher> midpointCache;

        auto getMidpointIndex = [&](uint32_t ia, uint32_t ib) -> uint32_t {
            const EdgeKey key{std::min(ia, ib), std::max(ia, ib)};
            const auto found = midpointCache.find(key);
            if (found != midpointCache.end()) {
                return found->second;
            }

            const Vertex& a = nextVertices[ia];
            const Vertex& b = nextVertices[ib];
            Vertex midpoint;
            midpoint.position = (a.position + b.position) * 0.5f;
            midpoint.normal = SafeNormalize(a.normal + b.normal, Vector3(0.0f, 1.0f, 0.0f));
            midpoint.colorR = 0.5f * (a.colorR + b.colorR);
            midpoint.colorG = 0.5f * (a.colorG + b.colorG);
            midpoint.colorB = 0.5f * (a.colorB + b.colorB);
            midpoint.colorA = 0.5f * (a.colorA + b.colorA);
            midpoint.uv = Vector2(
                0.5f * (a.uv.x + b.uv.x),
                0.5f * (a.uv.y + b.uv.y)
            );

            const uint32_t midpointIndex = static_cast<uint32_t>(nextVertices.size());
            nextVertices.push_back(midpoint);
            midpointCache.emplace(key, midpointIndex);
            return midpointIndex;
        };

        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const uint32_t i0 = indices[i + 0];
            const uint32_t i1 = indices[i + 1];
            const uint32_t i2 = indices[i + 2];
            const uint32_t m01 = getMidpointIndex(i0, i1);
            const uint32_t m12 = getMidpointIndex(i1, i2);
            const uint32_t m20 = getMidpointIndex(i2, i0);

            AddTriangle(nextIndices, i0, m01, m20);
            AddTriangle(nextIndices, m01, i1, m12);
            AddTriangle(nextIndices, m20, m12, i2);
            AddTriangle(nextIndices, m01, m12, m20);
        }

        vertices = std::move(nextVertices);
        indices = std::move(nextIndices);
    }

    mesh.SetVertices(std::move(vertices));
    mesh.SetIndices(std::move(indices));
    RecomputeNormals(mesh);
}

void ApplyTwist(Mesh& mesh, float totalDegrees) {
    const Bounds bounds = ComputeBounds(mesh);
    const float height = std::max(bounds.max.y - bounds.min.y, kEpsilon);
    std::vector<Vertex> vertices = mesh.GetVertices();
    for (Vertex& vertex : vertices) {
        const float t = (vertex.position.y - bounds.min.y) / height;
        const float angle = ToRadians(totalDegrees * t);
        vertex.position = RotateY(vertex.position, angle);
    }
    mesh.SetVertices(std::move(vertices));
    RecomputeNormals(mesh);
}

void ApplyShear(Mesh& mesh, float shearX, float shearZ) {
    std::vector<Vertex> vertices = mesh.GetVertices();
    for (Vertex& vertex : vertices) {
        vertex.position.x += vertex.position.y * shearX;
        vertex.position.z += vertex.position.y * shearZ;
    }
    mesh.SetVertices(std::move(vertices));
    RecomputeNormals(mesh);
}

void ApplyBend(Mesh& mesh, float angleDegrees, const std::string& axis) {
    const Bounds bounds = ComputeBounds(mesh);
    const float height = std::max(bounds.max.y - bounds.min.y, kEpsilon);
    const float angleRadians = ToRadians(angleDegrees);
    std::vector<Vertex> vertices = mesh.GetVertices();
    for (Vertex& vertex : vertices) {
        const float t = (vertex.position.y - bounds.min.y) / height;
        const float localAngle = angleRadians * t;
        vertex.position = (axis == "x") ? RotateX(vertex.position, localAngle) : RotateZ(vertex.position, localAngle);
    }
    mesh.SetVertices(std::move(vertices));
    RecomputeNormals(mesh);
}

void ApplyBulge(Mesh& mesh, float amplitude) {
    const Bounds bounds = ComputeBounds(mesh);
    const float height = std::max(bounds.max.y - bounds.min.y, kEpsilon);
    std::vector<Vertex> vertices = mesh.GetVertices();
    for (Vertex& vertex : vertices) {
        const float t = (vertex.position.y - bounds.min.y) / height;
        const float scale = 1.0f + std::sin(t * kPi) * amplitude;
        vertex.position.x *= scale;
        vertex.position.z *= scale;
    }
    mesh.SetVertices(std::move(vertices));
    RecomputeNormals(mesh);
}

std::shared_ptr<Mesh> CollapseToSingleMesh(const std::vector<MassBuildItem>& items, CSG::Operation op) {
    std::shared_ptr<Mesh> current;
    for (const MassBuildItem& item : items) {
        if (!item.mesh || !item.mesh->IsValid()) {
            continue;
        }
        if (!current) {
            current = CloneMesh(item.mesh);
            continue;
        }
        current = CSG::PerformBoolean(current.get(), item.mesh.get(), op);
        if (!current) {
            return nullptr;
        }
    }
    return current;
}

bool BuildNode(const RuleNode& node,
               std::vector<MassBuildItem>& outItems,
               std::vector<std::string>& warnings,
               std::string& outError,
               BuildContext& context);

bool BuildPrimitive(const RuleNode& node, std::vector<MassBuildItem>& outItems, std::string& outError) {
    std::shared_ptr<Mesh> mesh;
    RuleTransform transform = node.transform;

    switch (node.primitive) {
        case PrimitiveType::Cube:
            mesh.reset(MeshGenerator::CreateCube(1.0f, Vector3(1, 1, 1)));
            transform.scale[0] *= node.params.value("size_x", 1.0f);
            transform.scale[1] *= node.params.value("size_y", 1.0f);
            transform.scale[2] *= node.params.value("size_z", 1.0f);
            break;
        case PrimitiveType::Sphere:
            mesh.reset(MeshGenerator::CreateSphere(node.params.value("radius", 0.5f), node.params.value("segments", 24), node.params.value("rings", 16), Vector3(1, 1, 1)));
            break;
        case PrimitiveType::Cylinder:
            mesh.reset(MeshGenerator::CreateCylinder(node.params.value("radius", 0.5f), node.params.value("radius", 0.5f), node.params.value("height", 1.0f), node.params.value("segments", 24), Vector3(1, 1, 1)));
            break;
        case PrimitiveType::Capsule:
            mesh.reset(MeshGenerator::CreateCapsule(node.params.value("radius", 0.5f), node.params.value("height", 2.0f), node.params.value("segments", 16), node.params.value("rings", 8), Vector3(1, 1, 1)));
            break;
        case PrimitiveType::Cone:
            mesh.reset(MeshGenerator::CreateCone(node.params.value("radius", 0.5f), node.params.value("height", 1.0f), node.params.value("segments", 24), Vector3(1, 1, 1)));
            break;
        case PrimitiveType::Torus:
            mesh.reset(MeshGenerator::CreateTorus(node.params.value("radius_outer", node.params.value("major_radius", 0.75f)), node.params.value("radius_inner", node.params.value("minor_radius", 0.25f)), node.params.value("segments_major", 24), node.params.value("segments_minor", 12), Vector3(1, 1, 1)));
            break;
    }

    if (!mesh || !mesh->IsValid()) {
        outError = "Failed to build primitive mesh";
        return false;
    }

    ApplyTransformToMesh(*mesh, transform);
    outItems.push_back({node.name.empty() ? node.id : node.name, node.material, mesh});
    return true;
}

bool BuildExtrude(const RuleNode& node, std::vector<MassBuildItem>& outItems, std::string& outError) {
    if (node.profiles.empty()) {
        outError = "Extrude node requires one closed 2D profile";
        return false;
    }
    std::shared_ptr<Mesh> mesh = CreateExtrudeMesh(node.profiles.front(), node.params.value("height", 10.0f));
    if (!mesh) {
        outError = "Failed to create extrude mesh";
        return false;
    }
    ApplyTransformToMesh(*mesh, node.transform);
    outItems.push_back({node.name.empty() ? node.id : node.name, node.material, mesh});
    return true;
}

bool BuildRevolve(const RuleNode& node, std::vector<MassBuildItem>& outItems, std::string& outError) {
    if (node.profiles.empty()) {
        outError = "Revolve node requires one 2D profile";
        return false;
    }
    std::shared_ptr<Mesh> mesh = CreateRevolveMesh(node.profiles.front(), node.params.value("segments", 32));
    if (!mesh) {
        outError = "Failed to create revolve mesh";
        return false;
    }
    ApplyTransformToMesh(*mesh, node.transform);
    outItems.push_back({node.name.empty() ? node.id : node.name, node.material, mesh});
    return true;
}

bool BuildSweep(const RuleNode& node, std::vector<MassBuildItem>& outItems, std::string& outError) {
    if (node.profiles.empty() || node.paths.empty()) {
        outError = "Sweep node requires one 2D profile and one 3D path";
        return false;
    }
    std::shared_ptr<Mesh> mesh = CreateSweepMesh(node.profiles.front(), node.paths.front());
    if (!mesh) {
        outError = "Failed to create sweep mesh";
        return false;
    }
    ApplyTransformToMesh(*mesh, node.transform);
    outItems.push_back({node.name.empty() ? node.id : node.name, node.material, mesh});
    return true;
}

bool BuildLoft(const RuleNode& node, std::vector<MassBuildItem>& outItems, std::string& outError) {
    std::shared_ptr<Mesh> mesh = CreateLoftMesh(node);
    if (!mesh) {
        outError = "Failed to create loft mesh";
        return false;
    }
    ApplyTransformToMesh(*mesh, node.transform);
    outItems.push_back({node.name.empty() ? node.id : node.name, node.material, mesh});
    return true;
}
bool BuildGroup(const RuleNode& node,
                std::vector<MassBuildItem>& outItems,
                std::vector<std::string>& warnings,
                std::string& outError,
                BuildContext& context) {
    std::vector<MassBuildItem> localItems;
    for (const RuleNode& child : node.children) {
        if (!BuildNode(child, localItems, warnings, outError, context)) {
            return false;
        }
    }
    for (MassBuildItem& item : localItems) {
        if (item.mesh) {
            ApplyTransformToMesh(*item.mesh, node.transform);
        }
        if (item.material.empty() && !node.material.empty()) {
            item.material = node.material;
        }
        outItems.push_back(std::move(item));
    }
    return true;
}

bool BuildArray(const RuleNode& node,
                std::vector<MassBuildItem>& outItems,
                std::vector<std::string>& warnings,
                std::string& outError,
                BuildContext& context) {
    if (node.children.size() != 1) {
        outError = "Array node must have exactly one child";
        return false;
    }

    std::vector<MassBuildItem> sourceItems;
    if (!BuildNode(node.children.front(), sourceItems, warnings, outError, context)) {
        return false;
    }

    const int countX = std::max(1, node.params.value("count_x", 1));
    const int countY = std::max(1, node.params.value("count_y", 1));
    const int countZ = std::max(1, node.params.value("count_z", 1));
    const float spacingX = node.params.value("spacing_x", 0.0f);
    const float spacingY = node.params.value("spacing_y", 0.0f);
    const float spacingZ = node.params.value("spacing_z", 0.0f);
    const float rotateY = node.params.value("rotate_step_y", 0.0f);

    for (int ix = 0; ix < countX; ++ix) {
        for (int iy = 0; iy < countY; ++iy) {
            for (int iz = 0; iz < countZ; ++iz) {
                RuleTransform instanceTransform = node.transform;
                instanceTransform.position[0] += spacingX * static_cast<float>(ix);
                instanceTransform.position[1] += spacingY * static_cast<float>(iy);
                instanceTransform.position[2] += spacingZ * static_cast<float>(iz);
                instanceTransform.rotation[1] += rotateY * static_cast<float>(ix + iy + iz);

                for (const MassBuildItem& sourceItem : sourceItems) {
                    MassBuildItem item;
                    item.name = (node.name.empty() ? node.id : node.name) + "_" + std::to_string(ix) + "_" + std::to_string(iy) + "_" + std::to_string(iz);
                    item.material = sourceItem.material.empty() ? node.material : sourceItem.material;
                    item.mesh = CloneMesh(sourceItem.mesh);
                    if (item.mesh) {
                        ApplyTransformToMesh(*item.mesh, instanceTransform);
                    }
                    outItems.push_back(std::move(item));
                }
            }
        }
    }
    return true;
}

bool BuildCsg(const RuleNode& node,
              std::vector<MassBuildItem>& outItems,
              std::vector<std::string>& warnings,
              std::string& outError,
              BuildContext& context) {
    if (node.children.size() != 2) {
        outError = "CSG node must have exactly two children";
        return false;
    }

    std::vector<MassBuildItem> leftItems;
    std::vector<MassBuildItem> rightItems;
    if (!BuildNode(node.children[0], leftItems, warnings, outError, context)) {
        return false;
    }
    if (!BuildNode(node.children[1], rightItems, warnings, outError, context)) {
        return false;
    }

    std::shared_ptr<Mesh> leftMesh = CollapseToSingleMesh(leftItems, CSG::Operation::Union);
    std::shared_ptr<Mesh> rightMesh = CollapseToSingleMesh(rightItems, CSG::Operation::Union);
    if (!leftMesh || !rightMesh) {
        outError = "Failed to collapse CSG children into meshes";
        return false;
    }

    CSG::Operation op = CSG::Operation::Union;
    switch (node.csgOperation) {
        case CsgOperation::Union: op = CSG::Operation::Union; break;
        case CsgOperation::Subtract: op = CSG::Operation::Subtract; break;
        case CsgOperation::Intersect: op = CSG::Operation::Intersect; break;
    }

    std::shared_ptr<Mesh> result = CSG::PerformBoolean(leftMesh.get(), rightMesh.get(), op);
    if (!result) {
        outError = "Mesh-level CSG boolean failed";
        return false;
    }

    ApplyTransformToMesh(*result, node.transform);
    outItems.push_back({node.name.empty() ? node.id : node.name, node.material.empty() ? leftItems.front().material : node.material, result});
    return true;
}

bool BuildDeform(const RuleNode& node,
                 std::vector<MassBuildItem>& outItems,
                 std::vector<std::string>& warnings,
                 std::string& outError,
                 BuildContext& context) {
    if (node.children.size() != 1) {
        outError = "Deform node must have exactly one child";
        return false;
    }

    std::vector<MassBuildItem> localItems;
    if (!BuildNode(node.children[0], localItems, warnings, outError, context)) {
        return false;
    }

    const std::string mode = node.params.value("mode", std::string("twist"));
    const int subdivideIterations = std::max(0, node.params.value("subdivide", mode == "twist" ? 2 : 1));
    for (MassBuildItem& item : localItems) {
        if (!item.mesh) {
            continue;
        }
        if (subdivideIterations > 0) {
            SubdivideMesh(*item.mesh, subdivideIterations);
        }
        if (mode == "taper") {
            ApplyTaper(*item.mesh, node.params.value("bottom_scale", 1.0f), node.params.value("top_scale", 0.6f));
        } else if (mode == "twist") {
            ApplyTwist(*item.mesh, node.params.value("angle", 20.0f));
        } else if (mode == "bend") {
            ApplyBend(*item.mesh, node.params.value("angle", 15.0f), node.params.value("axis", std::string("z")));
        } else if (mode == "shear") {
            ApplyShear(*item.mesh, node.params.value("shear_x", 0.1f), node.params.value("shear_z", 0.0f));
        } else if (mode == "bulge") {
            ApplyBulge(*item.mesh, node.params.value("amount", 0.2f));
        } else {
            warnings.push_back("Unknown deform mode '" + mode + "', keeping child mesh unchanged");
        }

        ApplyTransformToMesh(*item.mesh, node.transform);
        if (!node.material.empty()) {
            item.material = node.material;
        }
        outItems.push_back(std::move(item));
    }

    return true;
}

bool BuildReference(const RuleNode& node,
                    std::vector<MassBuildItem>& outItems,
                    std::vector<std::string>& warnings,
                    std::string& outError,
                    BuildContext& context) {
    const std::string resolvedPath = NormalizeReferencePath(node.reference);
    if (resolvedPath.empty()) {
        outError = "Reference node must specify a non-empty ref";
        return false;
    }

    if (std::find(context.referenceStack.begin(), context.referenceStack.end(), resolvedPath) != context.referenceStack.end()) {
        outError = "Reference cycle detected: " + resolvedPath;
        return false;
    }

    const std::string jsonString = ReadUtf8TextFile(resolvedPath);
    if (jsonString.empty()) {
        outError = "Failed to read referenced massing asset: " + resolvedPath;
        return false;
    }

    RuleSet referencedRuleSet;
    if (!MassRuleParser::ParseFromString(jsonString, referencedRuleSet, outError)) {
        outError = "Failed to parse referenced massing asset '" + resolvedPath + "': " + outError;
        return false;
    }

    context.referenceStack.push_back(resolvedPath);

    std::vector<MassBuildItem> localItems;
    const bool built = BuildNode(referencedRuleSet.root, localItems, warnings, outError, context);

    context.referenceStack.pop_back();

    if (!built) {
        return false;
    }

    for (MassBuildItem& item : localItems) {
        if (item.mesh) {
            ApplyTransformToMesh(*item.mesh, node.transform);
        }
        if (!node.material.empty()) {
            item.material = node.material;
        }
        if (!node.name.empty()) {
            item.name = node.name + "/" + item.name;
        }
        outItems.push_back(std::move(item));
    }

    warnings.push_back("Expanded massing reference: " + node.reference);
    return true;
}

bool BuildNode(const RuleNode& node,
               std::vector<MassBuildItem>& outItems,
               std::vector<std::string>& warnings,
               std::string& outError,
               BuildContext& context) {
    switch (node.type) {
        case RuleNodeType::Primitive: return BuildPrimitive(node, outItems, outError);
        case RuleNodeType::Extrude: return BuildExtrude(node, outItems, outError);
        case RuleNodeType::Revolve: return BuildRevolve(node, outItems, outError);
        case RuleNodeType::Sweep: return BuildSweep(node, outItems, outError);
        case RuleNodeType::Loft: return BuildLoft(node, outItems, outError);
        case RuleNodeType::Csg: return BuildCsg(node, outItems, warnings, outError, context);
        case RuleNodeType::Deform: return BuildDeform(node, outItems, warnings, outError, context);
        case RuleNodeType::Array: return BuildArray(node, outItems, warnings, outError, context);
        case RuleNodeType::Group: return BuildGroup(node, outItems, warnings, outError, context);
        case RuleNodeType::Reference: return BuildReference(node, outItems, warnings, outError, context);
        default:
            outError = "Unsupported massing node type";
            return false;
    }
}

} // namespace

bool MassMeshBuilder::Build(const RuleSet& ruleSet, MassBuildResult& outResult, std::string& outError) {
    outResult.items.clear();
    outResult.warnings.clear();
    BuildContext context;
    if (!BuildNode(ruleSet.root, outResult.items, outResult.warnings, outError, context)) {
        return false;
    }

    for (MassBuildItem& item : outResult.items) {
        if (!item.mesh || !item.mesh->IsValid()) {
            continue;
        }
        SplitVerticesByCrease(*item.mesh);
    }

    return true;
}

} // namespace Massing
} // namespace Moon





