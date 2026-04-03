#include "BuildingToObjectBlueprintConverter.h"
#include "../../external/nlohmann/json.hpp"
#include <cmath>
#include <limits>
#include <sstream>
#include <unordered_set>

using json = nlohmann::json;

namespace Moon {
namespace Building {

// ---------------------------------------------------------------------------
// Unit helpers
// ---------------------------------------------------------------------------

// Meters to centimetres (CSG blueprint uses cm)
static inline float m2cm(float meters) { return meters * 100.0f; }

static inline json pos3(float x, float y, float z)
{
    return json::array({x, y, z});
}

static inline json rot3(float x, float y, float z)
{
    return json::array({x, y, z});
}

static inline float GetTopOfFloor(const BuildingDefinition& definition, int floorLevel)
{
    const float base = GetFloorBaseHeight(definition, floorLevel);
    for (const auto& floor : definition.floors) {
        if (floor.level == floorLevel) {
            return base + floor.floorHeight;
        }
    }
    return base;
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

static inline bool IsCardinalRotation(float rotationDegrees)
{
    const float normalized = NormalizeRotation(rotationDegrees);
    return std::abs(normalized - 0.0f) < 1.0f ||
           std::abs(normalized - 90.0f) < 1.0f ||
           std::abs(normalized - 180.0f) < 1.0f ||
           std::abs(normalized - 270.0f) < 1.0f;
}

static inline GridPos2D RotateOffset2D(float offsetX, float offsetZ, float rotationDegrees)
{
    const float radians = rotationDegrees * 3.14159265f / 180.0f;
    const float cosTheta = std::cos(radians);
    const float sinTheta = std::sin(radians);
    return {
        offsetX * cosTheta + offsetZ * sinTheta,
        -offsetX * sinTheta + offsetZ * cosTheta
    };
}

static inline bool DoesFloorNeedVerticalOpening(const VerticalTransport& transport, int floorLevel)
{
    if (transport.external) {
        return false;
    }

    return floorLevel > transport.floorFrom && floorLevel <= transport.floorTo;
}

static json CreateCubeNode(const std::string& name,
                           float centerX,
                           float centerY,
                           float centerZ,
                           float sizeX,
                           float sizeY,
                           float sizeZ,
                           const char* material,
                           float rotationY = 0.0f)
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
        {"position", pos3(m2cm(centerX), m2cm(centerY), m2cm(centerZ))},
        {"rotation", rot3(0.0f, rotationY, 0.0f)}
    };
    node["material"] = material;
    return node;
}

static json CreateWallPanelReference(const std::string& name,
                                     float centerX,
                                     float baseY,
                                     float centerZ,
                                     float length,
                                     float height,
                                     float thickness,
                                     float rotationDegrees,
                                     const char* material)
{
    json wallNode;
    wallNode["name"] = name;
    wallNode["type"] = "reference";
    wallNode["ref"] = "wall_panel_v1";
    wallNode["size"] = json::array({length, height, thickness});
    wallNode["overrides"] = {
        {"w", m2cm(length)},
        {"h", m2cm(height)},
        {"t", m2cm(thickness)},
        {"rotation_y", rotationDegrees}
    };
    wallNode["transform"] = {
        {"position", pos3(m2cm(centerX), m2cm(baseY), m2cm(centerZ))}
    };
    wallNode["material"] = material;
    return wallNode;
}

static json SubtractHoles(json base, const std::vector<json>& holes);

static const char* GetExteriorWallMaterial(const BuildingDefinition& definition)
{
    const std::string& facade = definition.style.facade;
    const std::string& material = definition.style.material;

    if (facade.find("glass") != std::string::npos || material.find("glass") != std::string::npos) {
        return "glass_tinted";
    }
    if (facade.find("stone") != std::string::npos) {
        return "stone";
    }
    if (facade.find("concrete") != std::string::npos || material.find("concrete") != std::string::npos) {
        return "concrete";
    }
    return "brick";
}

static float ComputeSignedArea(const std::vector<GridPos2D>& outline)
{
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

static json BuildConvexOutlineClipNode(const std::string& name,
                                       const std::vector<GridPos2D>& outline,
                                       float baseY,
                                       float slabThickness,
                                       const char* material)
{
    float minX = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();
    for (const auto& point : outline) {
        minX = std::min(minX, point[0]);
        minZ = std::min(minZ, point[1]);
        maxX = std::max(maxX, point[0]);
        maxZ = std::max(maxZ, point[1]);
    }

    const float width = std::max(0.1f, maxX - minX);
    const float depth = std::max(0.1f, maxZ - minZ);
    const float centerX = (minX + maxX) * 0.5f;
    const float centerZ = (minZ + maxZ) * 0.5f;
    json shape = CreateCubeNode(
        name + "_bbox",
        centerX,
        baseY + slabThickness * 0.5f,
        centerZ,
        width,
        slabThickness,
        depth,
        material);

    const float signedArea = ComputeSignedArea(outline);
    const bool ccw = signedArea >= 0.0f;
    const float span = std::max(width, depth);
    const float cutDepth = std::max(span * 3.0f, 6.0f);
    const float tangentPadding = std::max(span * 2.0f, 6.0f);
    const float cutHeight = slabThickness + 0.04f;
    std::vector<json> clipNodes;

    for (size_t i = 0; i < outline.size(); ++i) {
        const auto& a = outline[i];
        const auto& b = outline[(i + 1) % outline.size()];
        const float dx = b[0] - a[0];
        const float dz = b[1] - a[1];
        const float length = std::sqrt(dx * dx + dz * dz);
        if (length <= 0.001f) {
            continue;
        }

        const float tangentX = dx / length;
        const float tangentZ = dz / length;
        const float outwardX = ccw ? tangentZ : -tangentZ;
        const float outwardZ = ccw ? -tangentX : tangentX;
        const float centerOffset = cutDepth * 0.5f;
        const float clipCenterX = (a[0] + b[0]) * 0.5f + outwardX * centerOffset;
        const float clipCenterZ = (a[1] + b[1]) * 0.5f + outwardZ * centerOffset;
        const float rotationY = std::atan2(tangentZ, tangentX) * 180.0f / 3.14159265f;

        clipNodes.push_back(CreateCubeNode(
            name + "_clip_" + std::to_string(i),
            clipCenterX,
            baseY + slabThickness * 0.5f,
            clipCenterZ,
            length + tangentPadding,
            cutHeight,
            cutDepth,
            material,
            rotationY));
    }

    return SubtractHoles(shape, clipNodes);
}

static json CreateFloorPlateNode(const std::string& name,
                                 const FloorPlate& plate,
                                 float baseY,
                                 float slabThickness,
                                 const char* material)
{
    const std::vector<GridPos2D>& slabOutline =
        plate.envelopeOutline.size() >= 3 ? plate.envelopeOutline : plate.outline;
    json base;
    if (slabOutline.size() >= 3) {
        base = BuildConvexOutlineClipNode(
            name + "_outline",
            slabOutline,
            baseY,
            slabThickness,
            material);
    } else {
        base = CreateCubeNode(
            name + "_base",
            plate.origin[0] + plate.size[0] * 0.5f,
            baseY + slabThickness * 0.5f,
            plate.origin[1] + plate.size[1] * 0.5f,
            plate.size[0],
            slabThickness,
            plate.size[1],
            material);
    }

    std::vector<json> holes;
    for (const auto& voidRect : plate.voids) {
        holes.push_back(CreateCubeNode(
            name + "_void_" + voidRect.rectId,
            voidRect.origin[0] + voidRect.size[0] * 0.5f,
            baseY + slabThickness * 0.5f,
            voidRect.origin[1] + voidRect.size[1] * 0.5f,
            voidRect.size[0] + 0.02f,
            slabThickness + 0.02f,
            voidRect.size[1] + 0.02f,
            "glass"));
    }

    json node = SubtractHoles(base, holes);
    node["name"] = name;
    return node;
}

static json CreateFloorRectNode(const std::string& name,
                                const Rect& rect,
                                int floorLevel,
                                float baseY,
                                float slabThickness,
                                const char* material,
                                const std::vector<VerticalTransport>& verticalTransports)
{
    json base = CreateCubeNode(
        name + "_base",
        rect.origin[0] + rect.size[0] * 0.5f,
        baseY + slabThickness * 0.5f,
        rect.origin[1] + rect.size[1] * 0.5f,
        rect.size[0],
        slabThickness,
        rect.size[1],
        material);

    std::vector<json> holes;
    for (const auto& transport : verticalTransports) {
        if (!DoesFloorNeedVerticalOpening(transport, floorLevel)) {
            continue;
        }

        const float rectMaxX = rect.origin[0] + rect.size[0];
        const float rectMaxY = rect.origin[1] + rect.size[1];
        const Rect& openingRect = GetTransportOpeningRect(transport);
        const float shaftMaxX = openingRect.origin[0] + openingRect.size[0];
        const float shaftMaxY = openingRect.origin[1] + openingRect.size[1];
        const bool overlaps =
            rect.origin[0] < shaftMaxX - 0.001f && rectMaxX > openingRect.origin[0] + 0.001f &&
            rect.origin[1] < shaftMaxY - 0.001f && rectMaxY > openingRect.origin[1] + 0.001f;
        if (!overlaps) {
            continue;
        }

        holes.push_back(CreateCubeNode(
            name + "_transport_void_" + transport.transportId,
            openingRect.origin[0] + openingRect.size[0] * 0.5f,
            baseY + slabThickness * 0.5f,
            openingRect.origin[1] + openingRect.size[1] * 0.5f,
            openingRect.size[0] + 0.02f,
            slabThickness + 0.02f,
            openingRect.size[1] + 0.02f,
            "glass"));
    }

    json node = SubtractHoles(base, holes);
    node["name"] = name;
    return node;
}

// Note: GetFloorBaseHeight is now in BuildingTypes.h as inline function
// using cumulative floor height calculation

// ---------------------------------------------------------------------------
// Wall / window / door association helpers
// ---------------------------------------------------------------------------

// Returns the parameter t in [0,1] of the closest point on the wall to 'pt',
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

// Window belongs to a wall if: same spaceId, same floor, t in [0.02,0.98],0.98],
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
// same floor, t in [0.02,0.98], perpendicular distance < 50 cm.
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
// BuildingToObjectBlueprintConverter::Convert
// ---------------------------------------------------------------------------

std::string BuildingToObjectBlueprintConverter::Convert(const GeneratedBuilding& building)
{
    json blueprint;
    blueprint["schema_version"] = 1;
    blueprint["name"]           = "generated_building";
    blueprint["description"]    = "Auto-generated from the Moon semantic building system";
    blueprint["version"]        = 1;

    json children = json::array();
    const float wallThickness = 0.2f; // 20 cm walls

    // -----------------------------------------------------------------------
    // 0. Structural floor plates
    // -----------------------------------------------------------------------
    if (!building.floorPlates.empty() && building.floorPlateMeshes.empty()) {
        for (const auto& plate : building.floorPlates) {
            const float floorBaseY = GetFloorBaseHeight(building.definition, plate.floorLevel);
            children.push_back(CreateFloorPlateNode(
                "floor_plate_" + std::to_string(plate.floorLevel),
                plate,
                floorBaseY,
                0.18f,
                "concrete_floor"));
        }
    }

    // -----------------------------------------------------------------------
    // 1. Floor slabs (fallback when no explicit floor plates exist)
    // -----------------------------------------------------------------------
    if (building.floorPlates.empty()) {
        for (const auto& floor : building.definition.floors) {
            const float floorBaseY = GetFloorBaseHeight(building.definition, floor.level);
            for (const auto& space : floor.spaces) {
                for (const auto& rect : space.rects) {
                    const float slabThickness = 0.05f; // 5 cm

                    SpaceUsage usage = space.properties.usage;
                    const char* material = "concrete_floor";
                    if      (usage == SpaceUsage::Living)   material = "wood_floor";
                    else if (usage == SpaceUsage::Kitchen)  material = "tile_ceramic";
                    else if (usage == SpaceUsage::Bedroom)  material = "carpet";

                    json node = CreateFloorRectNode(
                        "floor_" + std::to_string(space.spaceId) + "_" + rect.rectId,
                        rect,
                        floor.level,
                        floorBaseY,
                        slabThickness,
                        material,
                        building.verticalTransports);

                    children.push_back(node);
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // 1a. Vertical transport shafts
    // -----------------------------------------------------------------------
    if (!building.verticalTransports.empty()) {
        int transportIdx = 0;
        for (const auto& transport : building.verticalTransports) {
            if (transport.type != VerticalTransportType::Elevator) {
                ++transportIdx;
                continue;
            }

            const float baseHeight = GetFloorBaseHeight(building.definition, transport.floorFrom);
            const float topHeight = GetTopOfFloor(building.definition, transport.floorTo);
            const float height = std::max(0.1f, topHeight - baseHeight);
            children.push_back(CreateCubeNode(
                "elevator_shaft_" + std::to_string(transportIdx),
                transport.shaftRect.origin[0] + transport.shaftRect.size[0] * 0.5f,
                baseHeight + height * 0.5f,
                transport.shaftRect.origin[1] + transport.shaftRect.size[1] * 0.5f,
                transport.shaftRect.size[0],
                height,
                transport.shaftRect.size[1],
                "metal_black"));

            const float servedFloorHeight =
                std::max(2.3f, std::min(2.8f, GetTopOfFloor(building.definition, transport.floorFrom) - baseHeight - 0.3f));
            json cabinNode;
            cabinNode["name"] = "elevator_cabin_" + std::to_string(transportIdx);
            cabinNode["type"] = "reference";
            cabinNode["ref"] = "elevator_cabin_v1";
            cabinNode["overrides"] = {
                {"cabin_width", m2cm(std::max(1.4f, transport.shaftRect.size[0] - 0.28f))},
                {"cabin_depth", m2cm(std::max(1.4f, transport.shaftRect.size[1] - 0.28f))},
                {"cabin_height", m2cm(servedFloorHeight)},
                {"door_width", m2cm(std::max(0.9f, std::min(1.3f, transport.shaftRect.size[0] * 0.45f)))}
            };
            cabinNode["transform"] = {
                {"position", pos3(
                    m2cm(transport.shaftRect.origin[0] + transport.shaftRect.size[0] * 0.5f),
                    m2cm(baseHeight),
                    m2cm(transport.shaftRect.origin[1] + transport.shaftRect.size[1] * 0.5f))}
            };
            children.push_back(std::move(cabinNode));
            ++transportIdx;
        }
    }

    // -----------------------------------------------------------------------
    // 1b. Support columns for elevated floors
    // -----------------------------------------------------------------------
    if (!building.supportColumns.empty()) {
        for (const auto& column : building.supportColumns) {
            const float baseHeight = GetFloorBaseHeight(building.definition, column.floorFrom);
            const float topHeight = GetTopOfFloor(building.definition, column.floorTo);
            const float height = std::max(0.1f, topHeight - baseHeight);
            children.push_back(CreateCubeNode(
                "support_column_" + column.columnId,
                column.center[0],
                baseHeight + height * 0.5f,
                column.center[1],
                column.width,
                height,
                column.depth,
                "concrete_floor"));
        }
    } else if (building.definition.style.category != "commercial" &&
               building.definition.style.category != "retail" &&
               building.definition.floors.size() <= 4u) {
        std::unordered_set<std::string> emittedSupportColumns;
        for (const auto& floor : building.definition.floors) {
            const float floorBaseY = GetFloorBaseHeight(building.definition, floor.level);
            if (floor.level <= 0 || floorBaseY <= 0.01f) {
                continue;
            }

            for (const auto& space : floor.spaces) {
                for (const auto& rect : space.rects) {
                    const float insetX = std::min(0.35f, rect.size[0] * 0.25f);
                    const float insetZ = std::min(0.35f, rect.size[1] * 0.25f);
                    const float columnSize = 0.3f;

                    const std::vector<GridPos2D> columnCenters = {
                        {rect.origin[0] + insetX, rect.origin[1] + insetZ},
                        {rect.origin[0] + rect.size[0] - insetX, rect.origin[1] + insetZ},
                        {rect.origin[0] + insetX, rect.origin[1] + rect.size[1] - insetZ},
                        {rect.origin[0] + rect.size[0] - insetX, rect.origin[1] + rect.size[1] - insetZ}
                    };

                    for (size_t columnIndex = 0; columnIndex < columnCenters.size(); ++columnIndex) {
                        const auto& center = columnCenters[columnIndex];
                        const int keyX = static_cast<int>(std::round(center[0] * 100.0f));
                        const int keyZ = static_cast<int>(std::round(center[1] * 100.0f));
                        const int keyH = static_cast<int>(std::round(floorBaseY * 100.0f));
                        const std::string key = std::to_string(keyX) + "_" + std::to_string(keyZ) + "_" + std::to_string(keyH);
                        if (!emittedSupportColumns.insert(key).second) {
                            continue;
                        }

                        children.push_back(CreateCubeNode(
                            "support_column_" + std::to_string(floor.level) + "_" + std::to_string(emittedSupportColumns.size()),
                            center[0],
                            floorBaseY * 0.5f,
                            center[1],
                            columnSize,
                            floorBaseY,
                            columnSize,
                            "concrete_floor"));
                    }
                }
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

        const float centerX = (wall.start[0] + wall.end[0]) * 0.5f;
        const float centerZ = (wall.start[1] + wall.end[1]) * 0.5f;
        const float floorBaseY = GetFloorBaseHeight(building.definition, wall.floorLevel);
        const char* wallMaterial =
            (wall.type == WallType::Exterior)
                ? GetExteriorWallMaterial(building.definition)
                : "plaster";

        // Wall reference: use wall_panel_v1 component for proper UV mapping
        // Panel is created along X axis, then rotated via rotation_y parameter
        json wallCube = CreateWallPanelReference(
            "wall_panel_" + std::to_string(wallIdx),
            centerX,
            floorBaseY,
            centerZ,
            length,
            wall.height,
            wall.thickness,
            wallRotationY,
            wallMaterial);

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
    // 5. Stair geometry
    // -----------------------------------------------------------------------
    int stairIdx = 0;
    for (const auto& stair : building.stairs) {
        const float stepHeight = std::max(0.05f, stair.stepHeight);
        const float treadDepth = std::max(0.1f, stair.stepDepth);
        const float stairWidth = std::max(0.8f, stair.stairWidth);
        const float baseHeight = GetFloorBaseHeight(building.definition, stair.fromLevel);
        json stairGroup;
        stairGroup["name"] = "stair_" + std::to_string(stairIdx);
        stairGroup["type"] = "group";
        stairGroup["children"] = json::array();

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

        children.push_back(std::move(stairGroup));
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
bool BuildingToObjectBlueprintConverter::IsWindowOnWall(const Window& window, const WallSegment& wall)
{
    return ::Moon::Building::IsWindowOnWall(window, wall);
}

} // namespace Building
} // namespace Moon



