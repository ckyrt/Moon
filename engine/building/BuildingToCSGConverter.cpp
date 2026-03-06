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

// Returns the world Y of a floor's base given its level.
static float GetFloorBaseHeight(const BuildingDefinition& def, int floorLevel)
{
    for (const auto& f : def.floors)
        if (f.level == floorLevel)
            return f.level * f.floorHeight;
    return 0.0f;
}

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
    blueprint["name"]           = "generated_building_v8";
    blueprint["description"]    = "Auto-generated from Building System V8";
    blueprint["version"]        = 1;

    json children = json::array();
    const float wallThickness = 0.2f; // 20 cm walls

    // -----------------------------------------------------------------------
    // 1. Floor slabs
    // -----------------------------------------------------------------------
    for (const auto& floor : building.definition.floors) {
        const float floorBaseY = floor.level * floor.floorHeight;
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

                const std::string& usage = space.properties.usageHint;
                if      (usage == "living")   node["material"] = "wood_floor";
                else if (usage == "kitchen")  node["material"] = "tile_ceramic";
                else if (usage == "bedroom")  node["material"] = "carpet";
                else                          node["material"] = "concrete_floor";

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
            }

    int wallIdx = 0;
    for (size_t wali = 0; wali < building.walls.size(); ++wali) {
        const auto& wall = building.walls[wali];
        float dx = wall.end[0] - wall.start[0];
        float dz = wall.end[1] - wall.start[1];
        float length = std::sqrt(dx * dx + dz * dz);
        if (length < 0.01f) { wallIdx++; continue; }

        const bool alongX = std::abs(dz) < 0.01f;

        // Wall cube: center X/Z at segment midpoint,
        // center Y at floorBaseHeight + height/2
        json wallCube;
        wallCube["type"]      = "primitive";
        wallCube["primitive"] = "cube";
        wallCube["params"]    = {
            {"size_x", m2cm(alongX ? length        : wall.thickness)},
            {"size_y", m2cm(wall.height)},
            {"size_z", m2cm(alongX ? wall.thickness : length)}
        };
        wallCube["transform"] = {{"position", pos3(
            m2cm((wall.start[0] + wall.end[0]) * 0.5f),
            m2cm(GetFloorBaseHeight(building.definition, wall.floorLevel) + wall.height * 0.5f),
            m2cm((wall.start[1] + wall.end[1]) * 0.5f)
        )}};
        wallCube["material"] = (wall.type == WallType::Exterior) ? "brick" : "plaster";

        // Hole cubes
        std::vector<json> holes;

        // Window holes
        for (int wi : wallWindowIdx[wali]) {
            const auto& win = building.windows[wi];
            // World Y of window opening centre
            float holeY = GetFloorBaseHeight(building.definition, win.floorLevel) + win.sillHeight + win.height * 0.5f;

            json hole;
            hole["type"]      = "primitive";
            hole["primitive"] = "cube";
            hole["params"]    = {
                {"size_x", m2cm(win.width)},
                {"size_y", m2cm(win.height) + 2.0f},        // +2 cm clearance
                {"size_z", m2cm(wall.thickness) + 2.0f}     // punch through
            };
            hole["transform"] = {
                {"position", pos3(m2cm(win.position[0]), m2cm(holeY), m2cm(win.position[1]))},
                {"rotation", json::array({0.0f, win.rotation, 0.0f})}
            };
            holes.push_back(hole);
        }

        // Door holes
        for (int di : wallDoorIdx[wali]) {
            const auto& door = building.doors[di];
            // Door opening sits at floor base; hole centre at floorBase + height/2
            float holeY = GetFloorBaseHeight(building.definition, door.floorLevel) + door.height * 0.5f;

            json hole;
            hole["type"]      = "primitive";
            hole["primitive"] = "cube";
            hole["params"]    = {
                {"size_x", m2cm(door.width)},
                {"size_y", m2cm(door.height) + 2.0f},       // +2 cm clearance
                {"size_z", m2cm(wall.thickness) + 2.0f}     // punch through
            };
            hole["transform"] = {
                {"position", pos3(m2cm(door.position[0]), m2cm(holeY), m2cm(door.position[1]))},
                {"rotation", json::array({0.0f, door.rotation, 0.0f})}
            };
            holes.push_back(hole);
        }

        json wallNode = SubtractHoles(wallCube, holes);
        wallNode["name"] = "wall_" + std::to_string(wallIdx++);
        children.push_back(wallNode);
    }

    // -----------------------------------------------------------------------
    // 3. Door references
    // -----------------------------------------------------------------------
    int doorIdx = 0;
    for (const auto& door : building.doors) {
        json node;
        node["name"] = "door_" + std::to_string(doorIdx++);
        node["type"] = "reference";
        node["ref"]  = "complete_door_v1";
        node["overrides"] = {
            {"door_width",  m2cm(door.width)},
            {"door_height", m2cm(door.height)},
            {"frame_depth", m2cm(wallThickness)}
        };
        node["transform"] = {
            {"position", pos3(m2cm(door.position[0]), m2cm(GetFloorBaseHeight(building.definition, door.floorLevel)), m2cm(door.position[1]))},
            {"rotation", json::array({0.0f, door.rotation, 0.0f})}
        };
        children.push_back(node);
    }

    // -----------------------------------------------------------------------
    // 4. Window frame references (holes already cut from walls above)
    // -----------------------------------------------------------------------
    int windowIdx = 0;
    for (const auto& window : building.windows) {
        float winCenterY = GetFloorBaseHeight(building.definition, window.floorLevel) + window.sillHeight + window.height * 0.5f;

        json node;
        node["name"] = "window_" + std::to_string(windowIdx++);
        node["type"] = "reference";
        node["ref"]  = "window_v1";
        node["overrides"] = {
            {"w", m2cm(window.width)},
            {"h", m2cm(window.height)},
            {"t", m2cm(wallThickness)}
        };
        node["transform"] = {
            {"position", pos3(m2cm(window.position[0]), m2cm(winCenterY), m2cm(window.position[1]))},
            {"rotation", json::array({0.0f, window.rotation, 0.0f})}
        };
        children.push_back(node);
    }

    // -----------------------------------------------------------------------
    // 5. Root group
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
