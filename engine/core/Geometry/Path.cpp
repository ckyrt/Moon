#include "Path.h"

#include <algorithm>
#include <cmath>

namespace Moon {
namespace Geometry {

namespace {

float ClampFloat(float value, float minValue, float maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

Vector3 NormalizeOrFallback(const Vector3& value, const Vector3& fallback) {
    const float length = value.Length();
    if (length <= 1e-5f) {
        return fallback;
    }
    return value * (1.0f / length);
}

Vector3 SafeRightFromTangent(const Vector3& tangent, const Vector3& upHint) {
    Vector3 right = Vector3::Cross(upHint, tangent);
    if (right.Length() <= 1e-5f) {
        right = Vector3::Cross(Vector3(1, 0, 0), tangent);
    }
    if (right.Length() <= 1e-5f) {
        right = Vector3::Cross(Vector3(0, 0, 1), tangent);
    }
    return NormalizeOrFallback(right, Vector3(1, 0, 0));
}

} // namespace

Path3D Path3D::FromPolyline(const std::vector<Vector3>& points, const Vector3& upHint) {
    Path3D path;
    path.m_upHint = NormalizeOrFallback(upHint, Vector3(0, 1, 0));
    if (points.size() < 2) {
        return path;
    }

    path.m_points.reserve(points.size());
    path.m_distances.reserve(points.size());
    path.m_points.push_back(points.front());
    path.m_distances.push_back(0.0f);

    float accumulated = 0.0f;
    for (size_t i = 1; i < points.size(); ++i) {
        const Vector3 segment = points[i] - points[i - 1];
        const float length = segment.Length();
        if (length <= 1e-5f) {
            continue;
        }

        accumulated += length;
        path.m_points.push_back(points[i]);
        path.m_distances.push_back(accumulated);
    }

    if (path.m_points.size() < 2) {
        path.m_points.clear();
        path.m_distances.clear();
    }

    return path;
}

bool Path3D::IsValid() const {
    return m_points.size() >= 2 && m_points.size() == m_distances.size();
}

float Path3D::GetLength() const {
    return m_distances.empty() ? 0.0f : m_distances.back();
}

size_t Path3D::GetPointCount() const {
    return m_points.size();
}

PathFrame Path3D::SampleAtDistance(float distance) const {
    PathFrame frame;
    if (!IsValid()) {
        return frame;
    }

    const float clamped = ClampFloat(distance, 0.0f, GetLength());
    frame.distance = clamped;

    auto upper = std::lower_bound(m_distances.begin(), m_distances.end(), clamped);
    if (upper == m_distances.begin()) {
        upper = m_distances.begin() + 1;
    } else if (upper == m_distances.end()) {
        upper = m_distances.end() - 1;
    }

    const size_t endIndex = static_cast<size_t>(upper - m_distances.begin());
    const size_t startIndex = endIndex - 1;
    const float startDistance = m_distances[startIndex];
    const float endDistance = m_distances[endIndex];
    const float segmentLength = std::max(endDistance - startDistance, 1e-5f);
    const float t = (clamped - startDistance) / segmentLength;

    const Vector3 start = m_points[startIndex];
    const Vector3 end = m_points[endIndex];
    frame.position = start + (end - start) * t;
    frame.tangent = NormalizeOrFallback(end - start, Vector3(0, 0, 1));
    frame.right = SafeRightFromTangent(frame.tangent, m_upHint);
    frame.up = NormalizeOrFallback(Vector3::Cross(frame.tangent, frame.right), m_upHint);
    return frame;
}

std::vector<PathFrame> GenerateUniformPlacements(const Path3D& path, const PathPlacementOptions& options) {
    std::vector<PathFrame> frames;
    if (!path.IsValid()) {
        return frames;
    }

    const float length = path.GetLength();
    const float start = ClampFloat(options.startOffset, 0.0f, length);
    const float end = ClampFloat(length - options.endOffset, 0.0f, length);
    if (end < start) {
        return frames;
    }

    const float spacing = std::max(options.spacing, 1e-4f);
    if (options.includeEnds) {
        frames.push_back(path.SampleAtDistance(start));
    }

    for (float distance = start + spacing; distance < end - 1e-4f; distance += spacing) {
        frames.push_back(path.SampleAtDistance(distance));
    }

    if (options.includeEnds) {
        if (frames.empty() || std::fabs(frames.back().distance - end) > 1e-4f) {
            frames.push_back(path.SampleAtDistance(end));
        }
    }

    return frames;
}

Quaternion RotationFromFrame(const PathFrame& frame) {
    return Quaternion::LookRotation(frame.tangent, frame.up);
}

} // namespace Geometry
} // namespace Moon
