#pragma once

#include "BuildingTypes.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace Moon {
namespace Building {

inline const Mass* FindMassById(const BuildingDefinition& definition, const std::string& massId) {
    auto it = std::find_if(definition.masses.begin(), definition.masses.end(),
                           [&](const Mass& mass) { return mass.massId == massId; });
    return it == definition.masses.end() ? nullptr : &(*it);
}

inline const Mass* FindMassForFloor(const BuildingDefinition& definition, const Floor& floor) {
    return FindMassById(definition, floor.massId);
}

inline std::vector<GridPos2D> BuildRectOutline(const Rect& rect) {
    if (rect.size[0] <= 0.0f || rect.size[1] <= 0.0f) {
        return {};
    }

    return {
        {rect.origin[0], rect.origin[1]},
        {rect.origin[0] + rect.size[0], rect.origin[1]},
        {rect.origin[0] + rect.size[0], rect.origin[1] + rect.size[1]},
        {rect.origin[0], rect.origin[1] + rect.size[1]}
    };
}

inline bool RectContainsPoint(const Rect& rect, const GridPos2D& point, float epsilon = 0.001f) {
    return point[0] > rect.origin[0] + epsilon &&
           point[0] < rect.origin[0] + rect.size[0] - epsilon &&
           point[1] > rect.origin[1] + epsilon &&
           point[1] < rect.origin[1] + rect.size[1] - epsilon;
}

inline float ComputeSignedArea(const std::vector<GridPos2D>& outline) {
    if (outline.size() < 3) {
        return 0.0f;
    }

    float area = 0.0f;
    for (size_t i = 0; i < outline.size(); ++i) {
        const auto& a = outline[i];
        const auto& b = outline[(i + 1) % outline.size()];
        area += a[0] * b[1] - b[0] * a[1];
    }
    return area * 0.5f;
}

inline GridPos2D ComputeOutlineBoundsMin(const std::vector<GridPos2D>& outline) {
    GridPos2D minPoint = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    for (const auto& point : outline) {
        minPoint[0] = std::min(minPoint[0], point[0]);
        minPoint[1] = std::min(minPoint[1], point[1]);
    }
    return minPoint;
}

inline GridPos2D ComputeOutlineBoundsMax(const std::vector<GridPos2D>& outline) {
    GridPos2D maxPoint = {
        -std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max()
    };
    for (const auto& point : outline) {
        maxPoint[0] = std::max(maxPoint[0], point[0]);
        maxPoint[1] = std::max(maxPoint[1], point[1]);
    }
    return maxPoint;
}

inline float ComputeBuildingTotalHeight(const BuildingDefinition& definition) {
    float totalHeight = 0.0f;
    for (const auto& floor : definition.floors) {
        totalHeight = std::max(totalHeight, GetFloorBaseHeight(definition, floor.level) + floor.floorHeight);
    }
    return totalHeight;
}

} // namespace Building
} // namespace Moon
