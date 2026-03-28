#pragma once

#include "../Math/Quaternion.h"
#include "../Math/Vector3.h"
#include <vector>

namespace Moon {
namespace Geometry {

struct PathFrame {
    Vector3 position;
    Vector3 tangent;
    Vector3 right;
    Vector3 up;
    float distance = 0.0f;
};

class Path3D {
public:
    Path3D() = default;

    static Path3D FromPolyline(const std::vector<Vector3>& points,
                               const Vector3& upHint = Vector3(0, 1, 0));

    bool IsValid() const;
    float GetLength() const;
    size_t GetPointCount() const;
    PathFrame SampleAtDistance(float distance) const;

private:
    std::vector<Vector3> m_points;
    std::vector<float> m_distances;
    Vector3 m_upHint = Vector3(0, 1, 0);
};

struct PathPlacementOptions {
    float spacing = 1.0f;
    float startOffset = 0.0f;
    float endOffset = 0.0f;
    bool includeEnds = true;
};

std::vector<PathFrame> GenerateUniformPlacements(const Path3D& path,
                                                 const PathPlacementOptions& options);

Quaternion RotationFromFrame(const PathFrame& frame);

} // namespace Geometry
} // namespace Moon
