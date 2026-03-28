#include "PathMeshBuilder.h"

#include "../Math/Vector2.h"

namespace Moon {
namespace Geometry {

namespace {

void AddQuad(std::vector<Vertex>& vertices,
             std::vector<uint32_t>& indices,
             const Vector3& a,
             const Vector3& b,
             const Vector3& c,
             const Vector3& d,
             const Vector3& normal,
             const Vector3& color) {
    const uint32_t base = static_cast<uint32_t>(vertices.size());
    vertices.emplace_back(a, normal, color, 1.0f, Vector2(0.0f, 1.0f));
    vertices.emplace_back(b, normal, color, 1.0f, Vector2(1.0f, 1.0f));
    vertices.emplace_back(c, normal, color, 1.0f, Vector2(1.0f, 0.0f));
    vertices.emplace_back(d, normal, color, 1.0f, Vector2(0.0f, 0.0f));

    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 0);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
}

struct Ring {
    Vector3 topLeft;
    Vector3 topRight;
    Vector3 bottomRight;
    Vector3 bottomLeft;
};

Ring BuildRing(const PathFrame& frame, float halfWidth, float halfHeight) {
    Ring ring;
    const Vector3 right = frame.right * halfWidth;
    const Vector3 up = frame.up * halfHeight;
    ring.topLeft = frame.position - right + up;
    ring.topRight = frame.position + right + up;
    ring.bottomRight = frame.position + right - up;
    ring.bottomLeft = frame.position - right - up;
    return ring;
}

} // namespace

std::vector<PathMeshBuilder::SweptSegment> PathMeshBuilder::BuildRectangularSweepSegments(
    const Path3D& path,
    float segmentLength) {
    std::vector<SweptSegment> segments;
    if (!path.IsValid()) {
        return segments;
    }

    if (path.GetPointCount() == 2) {
        const PathFrame start = path.SampleAtDistance(0.0f);
        const PathFrame end = path.SampleAtDistance(path.GetLength());
        const Vector3 delta = end.position - start.position;
        const float length = delta.Length();
        if (length > 1e-5f) {
            SweptSegment segment;
            segment.center = start.position + delta * 0.5f;
            segment.rotation = RotationFromFrame(start);
            segment.length = length;
            segments.push_back(segment);
        }
        return segments;
    }

    PathPlacementOptions options;
    options.spacing = segmentLength;
    options.includeEnds = true;
    std::vector<PathFrame> frames = GenerateUniformPlacements(path, options);
    if (frames.size() < 2) {
        frames = {
            path.SampleAtDistance(0.0f),
            path.SampleAtDistance(path.GetLength())
        };
    }

    segments.reserve(frames.size() > 0 ? frames.size() - 1 : 0);
    for (size_t index = 0; index + 1 < frames.size(); ++index) {
        const Vector3 delta = frames[index + 1].position - frames[index].position;
        const float length = delta.Length();
        if (length <= 1e-5f) {
            continue;
        }

        SweptSegment segment;
        segment.center = frames[index].position + delta * 0.5f;
        segment.rotation = RotationFromFrame(frames[index]);
        segment.length = length;
        segments.push_back(segment);
    }

    return segments;
}

Mesh* PathMeshBuilder::SweepRectangleAlongPath(const Path3D& path,
                                               float width,
                                               float height,
                                               const Vector3& color) {
    if (!path.IsValid() || width <= 1e-5f || height <= 1e-5f) {
        return new Mesh();
    }

    PathPlacementOptions options;
    options.spacing = 0.2f;
    options.includeEnds = true;
    std::vector<PathFrame> frames = GenerateUniformPlacements(path, options);
    if (frames.size() < 2) {
        frames = {
            path.SampleAtDistance(0.0f),
            path.SampleAtDistance(path.GetLength())
        };
    }

    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Ring> rings;
    rings.reserve(frames.size());
    for (const auto& frame : frames) {
        rings.push_back(BuildRing(frame, halfWidth, halfHeight));
    }

    for (size_t i = 0; i + 1 < rings.size(); ++i) {
        const Ring& a = rings[i];
        const Ring& b = rings[i + 1];

        AddQuad(vertices, indices, a.topLeft, a.topRight, b.topRight, b.topLeft, frames[i].up, color);
        AddQuad(vertices, indices, a.bottomRight, a.bottomLeft, b.bottomLeft, b.bottomRight, -frames[i].up, color);
        AddQuad(vertices, indices, a.topRight, a.bottomRight, b.bottomRight, b.topRight, frames[i].right, color);
        AddQuad(vertices, indices, a.bottomLeft, a.topLeft, b.topLeft, b.bottomLeft, -frames[i].right, color);
    }

    const Ring& start = rings.front();
    const Ring& end = rings.back();
    AddQuad(vertices, indices, start.topRight, start.topLeft, start.bottomLeft, start.bottomRight, -frames.front().tangent, color);
    AddQuad(vertices, indices, end.topLeft, end.topRight, end.bottomRight, end.bottomLeft, frames.back().tangent, color);

    Mesh* mesh = new Mesh();
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    return mesh;
}

} // namespace Geometry
} // namespace Moon
