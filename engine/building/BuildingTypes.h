#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>

namespace Moon {
namespace Building {

/**
 * @brief Grid alignment size (0.5 meters)
 * All coordinates must be multiples of this value
 */
constexpr float GRID_SIZE = 0.5f;

/**
 * @brief Building style configuration
 */
struct BuildingStyle {
    std::string category;       // e.g., "modern", "classical", "industrial"
    std::string facade;         // e.g., "glass_white", "brick", "concrete"
    std::string roof;           // e.g., "flat", "pitched", "gable"
    std::string windowStyle;    // e.g., "full_height", "standard", "small"
    std::string material;       // e.g., "concrete_white", "brick_red"
};

/**
 * @brief 2D position on grid (X, Y in horizontal plane)
 */
using GridPos2D = std::array<float, 2>;

/**
 * @brief 2D size (width, depth)
 */
using GridSize2D = std::array<float, 2>;

/**
 * @brief Building mass (volume)
 * Defines the overall envelope of a building or building section
 */
struct Mass {
    std::string massId;         // Unique identifier
    GridPos2D origin;           // Bottom-left corner [X, Y]
    GridSize2D size;            // [width, depth]
    int floors;                 // Number of floors in this mass
};

/**
 * @brief Rectangle defining a space region
 */
struct Rect {
    std::string rectId;         // Unique identifier within space
    GridPos2D origin;           // Bottom-left corner [X, Y]
    GridSize2D size;            // [width, depth]
};

/**
 * @brief Stair configuration
 */
enum class StairType {
    Straight,
    L,
    U,
    Spiral
};

struct StairConfig {
    StairType type;
    int connectToLevel;         // Target floor level
    GridPos2D position;         // Position within space
    float width;                // Stair width (meters)
};

/**
 * @brief Anchor for procedural object placement
 */
enum class AnchorType {
    FurnitureZone,
    SofaZone,
    BedZone,
    DoorHint,
    StairHint,
    WindowHint
};

struct Anchor {
    std::string name;
    GridPos2D position;         // Position in space [X, Y]
    AnchorType type;
    // Optional metadata
    float rotation = 0.0f;      // Rotation in degrees
    std::string metadata;       // Additional JSON metadata
};

/**
 * @brief Space usage type
 * Defines the functional purpose of a space
 * IMPORTANT: AI must use these exact values in JSON "usage_hint" field
 */
enum class SpaceUsage {
    Unknown,        // Unspecified or unknown
    Living,         // Living room / lounge
    Dining,         // Dining room
    Kitchen,        // Kitchen
    Bedroom,        // Bedroom
    Bathroom,       // Bathroom / toilet
    Corridor,       // Hallway / corridor
    Entrance,       // Entrance hall / foyer
    Closet,         // Closet / wardrobe
    Storage,        // Storage room / utility room
    Office,         // Office / study
    Balcony,        // Balcony
    Terrace,        // Terrace
    Garage,         // Garage / parking
    Laundry,        // Laundry room
    Stairwell       // Stairwell space
};

/**
 * @brief Convert string to SpaceUsage enum
 */
inline SpaceUsage StringToSpaceUsage(const std::string& str) {
    if (str == "living") return SpaceUsage::Living;
    if (str == "dining") return SpaceUsage::Dining;
    if (str == "kitchen") return SpaceUsage::Kitchen;
    if (str == "bedroom") return SpaceUsage::Bedroom;
    if (str == "bathroom") return SpaceUsage::Bathroom;
    if (str == "corridor") return SpaceUsage::Corridor;
    if (str == "entrance") return SpaceUsage::Entrance;
    if (str == "closet") return SpaceUsage::Closet;
    if (str == "storage") return SpaceUsage::Storage;
    if (str == "office") return SpaceUsage::Office;
    if (str == "balcony") return SpaceUsage::Balcony;
    if (str == "terrace") return SpaceUsage::Terrace;
    if (str == "garage") return SpaceUsage::Garage;
    if (str == "laundry") return SpaceUsage::Laundry;
    if (str == "stairwell") return SpaceUsage::Stairwell;
    return SpaceUsage::Unknown;
}

/**
 * @brief Convert SpaceUsage enum to string
 */
inline std::string SpaceUsageToString(SpaceUsage usage) {
    switch (usage) {
        case SpaceUsage::Living: return "living";
        case SpaceUsage::Dining: return "dining";
        case SpaceUsage::Kitchen: return "kitchen";
        case SpaceUsage::Bedroom: return "bedroom";
        case SpaceUsage::Bathroom: return "bathroom";
        case SpaceUsage::Corridor: return "corridor";
        case SpaceUsage::Entrance: return "entrance";
        case SpaceUsage::Closet: return "closet";
        case SpaceUsage::Storage: return "storage";
        case SpaceUsage::Office: return "office";
        case SpaceUsage::Balcony: return "balcony";
        case SpaceUsage::Terrace: return "terrace";
        case SpaceUsage::Garage: return "garage";
        case SpaceUsage::Laundry: return "laundry";
        case SpaceUsage::Stairwell: return "stairwell";
        default: return "unknown";
    }
}

/**
 * @brief Space properties
 */
struct SpaceProperties {
    SpaceUsage usage;           // Space functional purpose (use enum, not string)
    bool isOutdoor;             // Is this an outdoor space (balcony, terrace)?
    bool hasStairs;             // Does this space contain stairs?
    float ceilingHeight;        // Height of ceiling (meters)
};

/**
 * @brief Space definition (room or area)
 * A space is composed of one or more rectangles
 */
struct Space {
    int spaceId;                            // Unique identifier
    std::vector<Rect> rects;                // Rectangle components
    SpaceProperties properties;
    std::vector<Anchor> anchors;            // Placement hints
    bool hasStairs;                         // Does this space have stairs?
    StairConfig stairsConfig;               // Stair configuration (only valid if hasStairs==true)
    
    Space() : spaceId(0), hasStairs(false) {}
};

/**
 * @brief Floor definition
 * Contains all spaces on a specific floor level
 */
struct Floor {
    int level;                  // Floor level (0 = ground)
    std::string massId;         // Which mass this floor belongs to
    float floorHeight;          // Total floor height (floor to floor)
    std::vector<Space> spaces;  // All spaces on this floor
};

/**
 * @brief Complete building definition (V8 schema)
 * This is the root structure representing the entire building
 */
struct BuildingDefinition {
    std::string schema;         // Should be "moon_building_v8"
    float grid;                 // Grid size (typically 0.5)
    BuildingStyle style;        // Architectural style
    std::vector<Mass> masses;   // Building volumes
    std::vector<Floor> floors;  // Floor definitions
};

/**
 * @brief Wall segment
 * Generated from space boundaries
 */
enum class WallType {
    Exterior,       // Exterior wall (no neighbor)
    Interior,       // Interior wall (between spaces)
    None            // No wall (open edge)
};

struct WallSegment {
    GridPos2D start;
    GridPos2D end;
    WallType type;
    int spaceId;            // Space on one side
    int neighborSpaceId;    // Space on other side (-1 if exterior)
    int floorLevel = 0;     // Floor level (0 = ground); use with Floor::floorHeight to get world Y
    float height;           // Wall height (floor-to-ceiling)
    float thickness;        // Wall thickness
};

/**
 * @brief Door placement
 */
enum class DoorType {
    Interior,
    Sliding,
    Glass,
    Balcony,
    Entrance
};

struct Door {
    GridPos2D position;
    float rotation;         // Rotation in degrees
    DoorType type;
    float width;            // Door width
    float height;           // Door height
    int spaceA;             // Space on one side
    int spaceB;             // Space on other side
    int floorLevel = 0;     // Floor level (0 = ground)
};

/**
 * @brief Window placement
 */
struct Window {
    GridPos2D position;
    float rotation;
    float width;
    float height;
    float sillHeight;       // Height from floor to bottom of window (relative to this floor)
    int floorLevel = 0;     // Floor level (0 = ground)
    int spaceId;
};

/**
 * @brief Space graph edge (connectivity)
 */
struct SpaceConnection {
    int spaceA;
    int spaceB;
    bool hasDoor;           // Is there a door connecting them?
};

/**
 * @brief Generated building geometry
 * Output of the building pipeline
 */
struct GeneratedBuilding {
    BuildingDefinition definition;
    std::vector<WallSegment> walls;
    std::vector<Door> doors;
    std::vector<Window> windows;
    std::vector<SpaceConnection> connections;
    // Mesh data will be generated from this structure
};

/**
 * @brief Validation result
 */
struct ValidationResult {
    bool valid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

} // namespace Building
} // namespace Moon
