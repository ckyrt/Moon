#include "MassMeshBuilder.h"

#include "../core/CSG/CSGOperations.h"
#include "../core/Geometry/MeshGenerator.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Moon {
namespace Massing {

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kEpsilon = 1e-4f;

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

void ApplyTransformToMesh(Mesh& mesh, const RuleTransform& transform) {
    std::vector<Vertex> vertices = mesh.GetVertices();
    for (Vertex& vertex : vertices) {
        vertex.position = ApplyPointTransform(vertex.position, transform);
    }
    mesh.SetVertices(std::move(vertices));
    RecomputeNormals(mesh);
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

void AddTriangle(std::vector<uint32_t>& indices, uint32_t a, uint32_t b, uint32_t c) {
    indices.push_back(a);
    indices.push_back(b);
    indices.push_back(c);
}

void AddQuad(std::vector<uint32_t>& indices, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    AddTriangle(indices, a, b, c);
    AddTriangle(indices, a, c, d);
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
        sampledProfiles.push_back(ResampleClosedCurve(profile, sampleCount));
    }

    const std::vector<float> levels = BuildLoftLevels(node, node.profiles.size());
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(sampleCount * sampledProfiles.size());

    for (size_t profileIndex = 0; profileIndex < sampledProfiles.size(); ++profileIndex) {
        for (const Vec2& point : sampledProfiles[profileIndex]) {
            vertices.emplace_back(Vector3(point[0], levels[profileIndex], point[1]), Vector3(0,1,0), Vector3(1,1,1), 1.0f, Vector2(point[0], point[1]));
        }
    }

    for (size_t ring = 0; ring + 1 < sampledProfiles.size(); ++ring) {
        const uint32_t currentOffset = static_cast<uint32_t>(ring * sampleCount);
        const uint32_t nextOffset = static_cast<uint32_t>((ring + 1) * sampleCount);
        for (size_t i = 0; i < sampleCount; ++i) {
            const uint32_t next = static_cast<uint32_t>((i + 1) % sampleCount);
            AddQuad(indices, currentOffset + static_cast<uint32_t>(i), currentOffset + next, nextOffset + next, nextOffset + static_cast<uint32_t>(i));
        }
    }

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

bool BuildNode(const RuleNode& node, std::vector<MassBuildItem>& outItems, std::vector<std::string>& warnings, std::string& outError);

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
bool BuildGroup(const RuleNode& node, std::vector<MassBuildItem>& outItems, std::vector<std::string>& warnings, std::string& outError) {
    std::vector<MassBuildItem> localItems;
    for (const RuleNode& child : node.children) {
        if (!BuildNode(child, localItems, warnings, outError)) {
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

bool BuildArray(const RuleNode& node, std::vector<MassBuildItem>& outItems, std::vector<std::string>& warnings, std::string& outError) {
    if (node.children.size() != 1) {
        outError = "Array node must have exactly one child";
        return false;
    }

    std::vector<MassBuildItem> sourceItems;
    if (!BuildNode(node.children.front(), sourceItems, warnings, outError)) {
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

bool BuildCsg(const RuleNode& node, std::vector<MassBuildItem>& outItems, std::vector<std::string>& warnings, std::string& outError) {
    if (node.children.size() != 2) {
        outError = "CSG node must have exactly two children";
        return false;
    }

    std::vector<MassBuildItem> leftItems;
    std::vector<MassBuildItem> rightItems;
    if (!BuildNode(node.children[0], leftItems, warnings, outError)) {
        return false;
    }
    if (!BuildNode(node.children[1], rightItems, warnings, outError)) {
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

bool BuildDeform(const RuleNode& node, std::vector<MassBuildItem>& outItems, std::vector<std::string>& warnings, std::string& outError) {
    if (node.children.size() != 1) {
        outError = "Deform node must have exactly one child";
        return false;
    }

    std::vector<MassBuildItem> localItems;
    if (!BuildNode(node.children[0], localItems, warnings, outError)) {
        return false;
    }

    const std::string mode = node.params.value("mode", std::string("twist"));
    for (MassBuildItem& item : localItems) {
        if (!item.mesh) {
            continue;
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

bool BuildNode(const RuleNode& node, std::vector<MassBuildItem>& outItems, std::vector<std::string>& warnings, std::string& outError) {
    switch (node.type) {
        case RuleNodeType::Primitive: return BuildPrimitive(node, outItems, outError);
        case RuleNodeType::Extrude: return BuildExtrude(node, outItems, outError);
        case RuleNodeType::Revolve: return BuildRevolve(node, outItems, outError);
        case RuleNodeType::Sweep: return BuildSweep(node, outItems, outError);
        case RuleNodeType::Loft: return BuildLoft(node, outItems, outError);
        case RuleNodeType::Csg: return BuildCsg(node, outItems, warnings, outError);
        case RuleNodeType::Deform: return BuildDeform(node, outItems, warnings, outError);
        case RuleNodeType::Array: return BuildArray(node, outItems, warnings, outError);
        case RuleNodeType::Group: return BuildGroup(node, outItems, warnings, outError);
        default:
            outError = "Unsupported massing node type";
            return false;
    }
}

} // namespace

bool MassMeshBuilder::Build(const RuleSet& ruleSet, MassBuildResult& outResult, std::string& outError) {
    outResult.items.clear();
    outResult.warnings.clear();
    return BuildNode(ruleSet.root, outResult.items, outResult.warnings, outError);
}

} // namespace Massing
} // namespace Moon
