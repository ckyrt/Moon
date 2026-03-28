#pragma once

#include "../Math/Vector3.h"
#include "../Mesh/Mesh.h"
#include "Path.h"
#include <vector>

namespace Moon {
namespace Geometry {

class PathMeshBuilder {
public:
    struct SweptSegment {
        Vector3 center;
        Quaternion rotation;
        float length = 0.0f;
    };

    static std::vector<SweptSegment> BuildRectangularSweepSegments(const Path3D& path,
                                                                   float segmentLength = 0.2f);

    static Mesh* SweepRectangleAlongPath(const Path3D& path,
                                         float width,
                                         float height,
                                         const Vector3& color = Vector3(1, 1, 1));
};

} // namespace Geometry
} // namespace Moon
