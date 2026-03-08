#include "BuildingToCSGConverter.h"
#include "../../external/nlohmann/json.hpp"
#include <cmath>
#include <sstream>

using json = nlohmann::json;

namespace Moon {
namespace Building {

// ---------------------------------------------------------------------------
// Unit helpers
// ---------------------------------------------------------------------------

// Meters → centimetres (CSG blueprint uses cm)
static inline float m2cm(float meters) { return meters * 100.0f; }

static inline json pos3(float x, float y, float z)
{
    return json::array({x, y, z});
}

static inline float NormalizeRotation(float rotationDegrees)
{
    float normalized = std::fmod(rotationDegrees, 360.0f);
    if (normalized < 0.0f) {
        normalized += 360.0f;
    }
    return normalized;
}

static inline bool IsQuarterTurn(float rotationDegrees)
{
    const float normalized = NormalizeRotation(rotationDegrees);
    return std::abs(normalized - 90.0f) < 1.0f || std::abs(normalized - 270.0f) < 1.0f;
}

static json CreateCubeNode(const std::string& name,
                           float centerX,
                           float centerY,
                           float centerZ,
                           float sizeX,
                           float sizeY,
                           float sizeZ,
                           const char* material)
{
    json node;
    node["name"] = name;
    node["type"] = "primitive";
    node["primitive"] = "cube";
    node["params"] = {
        {"size_x", m2cm(sizeX)},
        {"size_y", m2cm(sizeY)},
        {"size_z", m2cm(sizeZ)}
    };
    node["transform"] = {
        {"position", pos3(m2cm(centerX), m2cm(centerY), m2cm(centerZ))}
    };
    node["material"] = material;
    return node;
}

// Note: GetFloorBaseHeight is now in BuildingTypes.h as inline function
// using cumulative floor height calculation

// ---------------------------------------------------------------------------
// Wall / window / door association helpers
// ---------------------------------------------------------------------------

// Returns the parameter t ∈ [0,1] of the closest point on the wall to 'pt',
// and sets perpDist to the perpendicular distance from pt to the wall line.
// Returns false if the wall is degenerate.
static bool ProjectOntoWall(const GridPos2D& pt, const WallSegment& wall,
                             float& outT, float& outPerpDist)
{
    float dx = wall.end[0] - wall.start[0];
    float dz = wall.end[1] - wall.start[1];
    float lenSq = dx * dx + dz * dz;
    if (lenSq < 0.0001f) return false;

    float t = ((pt[0] - wall.start[0]) * dx +
               (pt[1] - wall.start[1]) * dz) / lenSq;
    outT = t;

    float projX = wall.start[0] + t * dx;
    float projZ = wall.start[1] + t * dz;
    float perpSq = (pt[0] - projX) * (pt[0] - projX) +
                   (pt[1] - projZ) * (pt[1] - projZ);
    outPerpDist = std::sqrt(perpSq);
    return true;
}

// Window belongs to a wall if: same spaceId, same floor, t ∈ [0.02,0.98],
// perpendicular distance < 50 cm.
static bool IsWindowOnWall(const Window& window, const WallSegment& wall)
{
    if (window.spaceId != wall.spaceId) return false;
    if (window.floorLevel != wall.floorLevel) return false;

    float t, perp;
    if (!ProjectOntoWall(window.position, wall, t, perp)) return false;
    return (t >= 0.02f && t <= 0.98f && perp < 0.5f);
}

// Door belongs to a wall if: door.spaceA or spaceB matches wall.spaceId,
// same floor, t ∈ [0.02,0.98], perpendicular distance < 50 cm.
static bool IsDoorOnWall(const Door& door, const WallSegment& wall)
{
    if (wall.spaceId != door.spaceA && wall.spaceId != door.spaceB) return false;
    if (door.floorLevel != wall.floorLevel) return false;

    float t, perp;
    if (!ProjectOntoWall(door.position, wall, t, perp)) return false;
    return (t >= 0.02f && t <= 0.98f && perp < 0.5f);
}

// Build a CSG subtract chain: base - holes[0] - holes[1] - ...
static json SubtractHoles(json base, const std::vector<json>& holes)
{
    for (const auto& hole : holes) {
        json node;
        node["type"]      = "csg";
        node["operation"] = "subtract";
        node["left"]      = std::move(base);
        node["right"]     = hole;
        base              = std::move(node);
    }
    return base;
}

// ---------------------------------------------------------------------------
// BuildingToCSGConverter::Convert
// ---------------------------------------------------------------------------

std::string BuildingToCSGConverter::Convert(const GeneratedBuilding& building)
{
    json blueprint;
    blueprint["schema_version"] = 1;
    blueprint["name"]           = "generated_building";
    blueprint["description"]    = "Auto-generated from the Moon semantic building system";
    blueprint["version"]        = 1;

    json children = json::array();
    const float wallThickness = 0.2f; // 20 cm walls

    // -----------------------------------------------------------------------
    // 1. Floor slabs
    // -----------------------------------------------------------------------
    for (const auto& floor : building.definition.floors) {
        const float floorBaseY = GetFloorBaseHeight(building.definition, floor.level);
        for (const auto& space : floor.spaces) {
            for (const auto& rect : space.rects) {
                const float slabThickness = 0.05f; // 5 cm

                json node;
                node["name"]      = "floor_" + std::to_string(space.spaceId) + "_" + rect.rectId;
                node["type"]      = "primitive";
                node["primitive"] = "cube";
                node["params"]    = {
                    {"size_x", m2cm(rect.size[0])},
                    {"size_y", m2cm(slabThickness)},
                    {"size_z", m2cm(rect.size[1])}
                };
                node["transform"] = {{"position", pos3(
                    m2cm(rect.origin[0] + rect.size[0] * 0.5f),
                    m2cm(floorBaseY + slabThickness * 0.5f),
                    m2cm(rect.origin[1] + rect.size[1] * 0.5f)
                )}};

                SpaceUsage usage = space.properties.usage;
                if      (usage == SpaceUsage::Living)   node["material"] = "wood_floor";
                else if (usage == SpaceUsage::Kitchen)  node["material"] = "tile_ceramic";
                else if (usage == SpaceUsage::Bedroom)  node["material"] = "carpet";
                else                                    node["material"] = "concrete_floor";

                children.push_back(node);
            }
        }
    }

    // -----------------------------------------------------------------------
    // 2. Walls with window + door holes (CSG subtract chain)
    // -----------------------------------------------------------------------

    // Pre-build per-wall lists of window/door indices
    std::vector<std::vector<int>> wallWindowIdx(building.walls.size());
    std::vector<std::vector<int>> wallDoorIdx(building.walls.size());

    for (size_t wi = 0; wi < building.windows.size(); ++wi)
        for (size_t wali = 0; wali < building.walls.size(); ++wali)
            if (IsWindowOnWall(building.windows[wi], building.walls[wali])) {
                wallWindowIdx[wali].push_back(static_cast<int>(wi));
                break;
            }

    for (size_t di = 0; di < building.doors.size(); ++di)
        for (size_t wali = 0; wali < building.walls.size(); ++wali)
            if (IsDoorOnWall(building.doors[di], building.walls[wali])) {
                wallDoorIdx[wali].push_back(static_cast<int>(di));
                break;
            }

    int wallIdx = 0;
    for (size_t wali = 0; wali < building.walls.size(); ++wali) {
        const auto& wall = building.walls[wali];
        float dx = wall.end[0] - wall.start[0];
        float dz = wall.end[1] - wall.start[1];
        float length = std::sqrt(dx * dx + dz * dz);
        if (length < 0.01f) { wallIdx++; continue; }

        // Calculate wall rotation: angle from +X axis to (dx, dz) direction in degrees
        float wallRotationY = std::atan2(dz, dx) * 180.0f / 3.14159265f;

        // Wall reference: use wall_panel_v1 component for proper UV mapping
        // Panel is created along X axis, then rotated via rotation_y parameter
        json wallCube;
        wallCube["type"] = "reference";
        wallCube["ref"]  = "wall_panel_v1";
        wallCube["size"] = json::array({length, wall.height, wall.thickness});
        wallCube["overrides"] = {
            {"w", m2cm(length)},         // Width along X (before rotation)
            {"h", m2cm(wall.height)},    // Height along Y
            {"t", m2cm(wall.thickness)}, // Thickness along Z
            {"rotation_y", wallRotationY}
        };
        // POSITIONING: wall_panel_v1 uses bottom-center convention (y=0 at base)
        wallCube["transform"] = {
            {"position", pos3(
                m2cm((wall.start[0] + wall.end[0]) * 0.5f),
                m2cm(GetFloorBaseHeight(building.definition, wall.floorLevel)),  // Bottom of wall
                m2cm((wall.start[1] + wall.end[1]) * 0.5f)
            )}
        };
        wallCube["material"] = (wall.type == WallType::Exterior) ? "brick" : "plaster";

        // Hole cubes
        std::vector<json> holes;

        // Window holes - use opening_v1 component
        for (int wi : wallWindowIdx[wali]) {
            const auto& win = building.windows[wi];
            // POSITIONING: opening_v1 uses bottom-center convention (y=0 at sill)
            float holeY = GetFloorBaseHeight(building.definition, win.floorLevel) + win.sillHeight;

            json hole;
            hole["type"] = "reference";
            hole["ref"]  = "opening_v1";
            hole["overrides"] = {
                {"w", m2cm(win.width) + 2.0f},           // +2 cm clearance
                {"h", m2cm(win.height) + 2.0f},          // +2 cm clearance
                {"t", m2cm(wall.thickness) + 2.0f},      // punch through
                {"rotation_y", wallRotationY}
            };
            hole["transform"] = {
                {"position", pos3(m2cm(win.position[0]), m2cm(holeY), m2cm(win.position[1]))}
            };
            holes.push_back(hole);
        }

        // Door holes - use opening_v1 component
        for (int di : wallDoorIdx[wali]) {
            const auto& door = building.doors[di];
            // POSITIONING: opening_v1 uses bottom-center convention (y=0 at ground)
            float holeY = GetFloorBaseHeight(building.definition, door.floorLevel);

            json hole;
            hole["type"] = "reference";
            hole["ref"]  = "opening_v1";
            hole["overrides"] = {
                {"w", m2cm(door.width) + 2.0f},          // +2 cm clearance
                {"h", m2cm(door.height) + 2.0f},         // +2 cm clearance
                {"t", m2cm(wall.thickness) + 2.0f},      // punch through
                {"rotation_y", wallRotationY}
            };
            hole["transform"] = {
                {"position", pos3(m2cm(door.position[0]), m2cm(holeY), m2cm(door.position[1]))}
            };
            holes.push_back(hole);
        }

        json wallNode = SubtractHoles(wallCube, holes);
        wallNode["name"] = "wall_" + std::to_string(wallIdx++);
        wallNode["size"] = json::array({length, wall.height, wall.thickness});
        children.push_back(wallNode);
    }

    // -----------------------------------------------------------------------
    // 3. Door references
    // -----------------------------------------------------------------------
    int doorIdx = 0;
    for (const auto& door : building.doors) {
        // POSITIONING: door_v1 uses bottom-center convention (y=0 at ground)
        json node;
        node["name"] = "door_" + std::to_string(doorIdx++);
        node["type"] = "reference";
        node["ref"]  = "door_v1";
        node["overrides"] = {
            {"door_width",  m2cm(door.width)},
            {"door_height", m2cm(door.height)},
            {"door_thickness", 4.0f},  // Standard door thickness
            {"rotation_y", door.rotation}
        };
        node["transform"] = {
            {"position", pos3(m2cm(door.position[0]), m2cm(GetFloorBaseHeight(building.definition, door.floorLevel)), m2cm(door.position[1]))}
        };
        children.push_back(node);
    }

    // -----------------------------------------------------------------------
    // 4. Window frame references (holes already cut from walls above)
    // -----------------------------------------------------------------------
    int windowIdx = 0;
    for (const auto& window : building.windows) {
        // POSITIONING: window_v1 uses bottom-center convention (y=0 at sill)
        float winBaseY = GetFloorBaseHeight(building.definition, window.floorLevel) + window.sillHeight;

        json node;
        node["name"] = "window_" + std::to_string(windowIdx++);
        node["type"] = "reference";
        node["ref"]  = "window_v1";
        node["overrides"] = {
            {"w", m2cm(window.width)},
            {"h", m2cm(window.height)},
            {"t", m2cm(wallThickness)},
            {"rotation_y", window.rotation}
        };
        node["transform"] = {
            {"position", pos3(m2cm(window.position[0]), m2cm(winBaseY), m2cm(window.position[1]))}
        };
        children.push_back(node);
    }

    // -----------------------------------------------------------------------
    // 5. Stair geometry (steps + landings)
    // -----------------------------------------------------------------------
    int stairIdx = 0;
    for (const auto& stair : building.stairs) {
        json stairGroup;
        stairGroup["name"] = "stair_" + std::to_string(stairIdx);
        stairGroup["type"] = "group";
        stairGroup["children"] = json::array();

        const float stepHeight = std::max(0.05f, stair.stepHeight);
        const float treadDepth = std::max(0.1f, stair.stepDepth);
        const float stairWidth = std::max(0.8f, stair.stairWidth);
        const float baseHeight = GetFloorBaseHeight(building.definition, stair.fromLevel);

        for (size_t stepIndex = 0; stepIndex < stair.steps.size(); ++stepIndex) {
            const auto& step = stair.steps[stepIndex];
            const bool quarterTurn = IsQuarterTurn(step.rotation);
            const float sizeX = quarterTurn ? treadDepth : stairWidth;
            const float sizeZ = quarterTurn ? stairWidth : treadDepth;

            stairGroup["children"].push_back(CreateCubeNode(
                "stair_" + std::to_string(stairIdx) + "_step_" + std::to_string(stepIndex),
                step.position[0],
                baseHeight + step.height + stepHeight * 0.5f,
                step.position[1],
                sizeX,
                stepHeight,
                sizeZ,
                "concrete_floor"));
        }

        for (size_t landingIndex = 0; landingIndex < stair.landings.size(); ++landingIndex) {
            const auto& landing = stair.landings[landingIndex];
            const bool quarterTurn = IsQuarterTurn(landing.rotation);
            const float sizeX = quarterTurn ? landing.depth : landing.width;
            const float sizeZ = quarterTurn ? landing.width : landing.depth;

            stairGroup["children"].push_back(CreateCubeNode(
                "stair_" + std::to_string(stairIdx) + "_landing_" + std::to_string(landingIndex),
                landing.position[0],
                baseHeight + landing.height + stepHeight * 0.5f,
                landing.position[1],
                sizeX,
                stepHeight,
                sizeZ,
                "concrete_floor"));
        }

        children.push_back(stairGroup);
        ++stairIdx;
    }

    // -----------------------------------------------------------------------
    // 6. Root group
    // -----------------------------------------------------------------------
    blueprint["root"] = {
        {"type",     "group"},
        {"children", children},
        {"output",   {{"mode", "separate"}}}
    };

    return blueprint.dump(2);
}

// Satisfy the declaration in the header (delegates to file-scope helper)
bool BuildingToCSGConverter::IsWindowOnWall(const Window& window, const WallSegment& wall)
{
    return ::Moon::Building::IsWindowOnWall(window, wall);
}

} // namespace Building
} // namespace Moon
