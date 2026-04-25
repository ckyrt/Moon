#include "BuildingElementRules.h"

#include "BuildingGeometryUtils.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Moon {
namespace Building {

namespace {

constexpr float kWallLengthEpsilon = 0.01f;
constexpr float kAlignmentEpsilon = 0.35f;
constexpr float kMinimumSideClearance = 0.05f;

void AddError(BuildingQualityReport& report,
              const std::string& code,
              const std::string& message,
              int floorLevel = -1) {
    report.passed = false;
    report.errors.push_back({code, message, floorLevel});
}

float DistanceSquared(const GridPos2D& a, const GridPos2D& b) {
    const float dx = a[0] - b[0];
    const float dy = a[1] - b[1];
    return dx * dx + dy * dy;
}

float SegmentLength(const WallSegment& wall) {
    return std::sqrt(DistanceSquared(wall.start, wall.end));
}

float DistancePointToSegment(const GridPos2D& point, const WallSegment& wall) {
    const float vx = wall.end[0] - wall.start[0];
    const float vy = wall.end[1] - wall.start[1];
    const float segmentLengthSquared = vx * vx + vy * vy;
    if (segmentLengthSquared <= 1e-6f) {
        return std::sqrt(DistanceSquared(point, wall.start));
    }

    const float wx = point[0] - wall.start[0];
    const float wy = point[1] - wall.start[1];
    const float t = std::max(0.0f, std::min(1.0f, (wx * vx + wy * vy) / segmentLengthSquared));
    const GridPos2D projection = {
        wall.start[0] + vx * t,
        wall.start[1] + vy * t
    };
    return std::sqrt(DistanceSquared(point, projection));
}

float DistanceAlongSegment(const GridPos2D& point, const WallSegment& wall) {
    const float vx = wall.end[0] - wall.start[0];
    const float vy = wall.end[1] - wall.start[1];
    const float segmentLengthSquared = vx * vx + vy * vy;
    if (segmentLengthSquared <= 1e-6f) {
        return 0.0f;
    }

    const float wx = point[0] - wall.start[0];
    const float wy = point[1] - wall.start[1];
    const float t = std::max(0.0f, std::min(1.0f, (wx * vx + wy * vy) / segmentLengthSquared));
    return std::sqrt(segmentLengthSquared) * t;
}

float GetTopOfFloor(const BuildingDefinition& definition, int floorLevel) {
    const float base = GetFloorBaseHeight(definition, floorLevel);
    for (const auto& floor : definition.floors) {
        if (floor.level == floorLevel) {
            return base + floor.floorHeight;
        }
    }
    return base;
}

float GetRenderedSlabThickness(const GeneratedBuilding& building) {
    return building.floorPlates.empty() ? 0.05f : 0.18f;
}

float GetWallBaseHeight(const GeneratedBuilding& building, int floorLevel) {
    return GetFloorBaseHeight(building.definition, floorLevel) + GetRenderedSlabThickness(building);
}

const WallSegment* FindWallById(const GeneratedBuilding& building, int wallId) {
    for (const auto& wall : building.walls) {
        if (wall.wallId == wallId) {
            return &wall;
        }
    }
    return nullptr;
}

void CheckWallGeometry(const GeneratedBuilding& building, BuildingQualityReport& report) {
    for (const auto& wall : building.walls) {
        const float wallLength = SegmentLength(wall);
        if (wallLength <= kWallLengthEpsilon) {
            AddError(report,
                     "wall_zero_length",
                     "Wall segment has zero or near-zero length.",
                     wall.floorLevel);
        }

        if (wall.floorLevel < 0) {
            AddError(report,
                     "wall_invalid_floor",
                     "Wall is assigned to an invalid floor.",
                     wall.floorLevel);
        }

        if (wall.height <= 0.0f) {
            AddError(report,
                     "wall_invalid_height",
                     "Wall height must be positive.",
                     wall.floorLevel);
        }
    }
}

void CheckDoorRules(const GeneratedBuilding& building, BuildingQualityReport& report) {
    for (const auto& door : building.doors) {
        const WallSegment* wall = FindWallById(building, door.wallId);
        if (!wall) {
            continue;
        }

        if (wall->floorLevel != door.floorLevel) {
            AddError(report,
                     "door_floor_mismatch",
                     "Door floor level does not match its host wall.",
                     door.floorLevel);
        }

        const float wallLength = SegmentLength(*wall);
        if (door.width <= 0.0f || door.height <= 0.0f) {
            AddError(report,
                     "door_invalid_size",
                     "Door dimensions must be positive.",
                     door.floorLevel);
        }

        if (wallLength > 0.0f && door.width > std::max(0.0f, wallLength - kMinimumSideClearance * 2.0f)) {
            AddError(report,
                     "door_exceeds_wall_span",
                     "Door width is too large for its host wall.",
                     door.floorLevel);
        }

        const float clearStoryHeight = std::max(0.0f, GetTopOfFloor(building.definition, door.floorLevel) - GetWallBaseHeight(building, door.floorLevel));
        if (clearStoryHeight > 0.0f && door.height > clearStoryHeight + kAlignmentEpsilon) {
            AddError(report,
                     "door_exceeds_story_height",
                     "Door height exceeds the clear story height.",
                     door.floorLevel);
        }
    }
}

void CheckWindowRules(const GeneratedBuilding& building, BuildingQualityReport& report) {
    for (const auto& window : building.windows) {
        const WallSegment* wall = FindWallById(building, window.wallId);
        if (!wall) {
            continue;
        }

        if (wall->floorLevel != window.floorLevel) {
            AddError(report,
                     "window_floor_mismatch",
                     "Window floor level does not match its host wall.",
                     window.floorLevel);
        }

        const float wallLength = SegmentLength(*wall);
        const float distanceToWall = DistancePointToSegment(window.position, *wall);
        if (distanceToWall > kAlignmentEpsilon) {
            AddError(report,
                     "window_off_wall",
                     "Window position is not aligned with its host wall.",
                     window.floorLevel);
        }

        if (window.width <= 0.0f || window.height <= 0.0f) {
            AddError(report,
                     "window_invalid_size",
                     "Window dimensions must be positive.",
                     window.floorLevel);
        }

        if (wallLength > 0.0f && window.width > std::max(0.0f, wallLength - kMinimumSideClearance * 2.0f)) {
            AddError(report,
                     "window_exceeds_wall_span",
                     "Window width is too large for its host wall.",
                     window.floorLevel);
        }

        if (window.sillHeight < 0.0f) {
            AddError(report,
                     "window_invalid_sill_height",
                     "Window sill height must be non-negative.",
                     window.floorLevel);
        }

        const float clearStoryHeight = std::max(0.0f, GetTopOfFloor(building.definition, window.floorLevel) - GetWallBaseHeight(building, window.floorLevel));
        if (clearStoryHeight > 0.0f &&
            window.sillHeight + window.height > clearStoryHeight + kAlignmentEpsilon) {
            AddError(report,
                     "window_exceeds_story_height",
                     "Window top exceeds the clear story height and may pierce the upper slab.",
                     window.floorLevel);
        }
    }
}

} // namespace

void AppendElementRuleViolations(const GeneratedBuilding& building,
                                 BuildingQualityReport& report) {
    CheckWallGeometry(building, report);
    CheckDoorRules(building, report);
    CheckWindowRules(building, report);
}

} // namespace Building
} // namespace Moon
