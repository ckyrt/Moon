#pragma once

#include "BuildingTypes.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Moon {
namespace Building {

// ============================================================================
// Semantic Building Definition (Schema V11)
// ============================================================================

enum class RelationshipType {
    Connected,   // Direct connection (door)
    Nearby,      // Adjacent (share wall)
    Separated,   // Not adjacent
    Visual       // Visual connection (window/opening)
};

enum class ImportanceLevel {
    Required,    // Must be satisfied
    Preferred,   // Should be satisfied if possible
    Optional     // Nice to have
};

struct Adjacency {
    std::string to;
    std::string relationship;  // "connected", "nearby", "separated", "visual"
    std::string importance;    // "required", "preferred", "optional"
};

struct SpaceConstraints {
    float aspectRatioMax = 3.0f;
    std::string naturalLight = "none";  // "required", "preferred", "none"
    bool exteriorAccess = false;
    float ceilingHeight = 3.0f;
    float minWidth = 2.0f;
    int connectsToFloor = -1;
    int connectsFromFloor = -1;
};

struct SemanticSpace {
    std::string spaceId;
    std::string type;
    float areaMin = 0.0f;
    float areaMax = 0.0f;
    float areaPreferred = 0.0f;
    std::string priority = "medium";  // "low", "medium", "high"
    std::vector<Adjacency> adjacency;
    SpaceConstraints constraints;
};

struct SemanticFloor {
    int level = 0;
    std::string name;
    std::vector<SemanticSpace> spaces;
};

struct MassConstraints {
    float footprintArea = 0.0f;
    int floors = 1;
    float totalHeight = 0.0f;
};

struct SemanticBuilding {
    std::string schema;
    std::string buildingType;
    BuildingStyle style;  // 使用 BuildingTypes.h 中定义的
    MassConstraints mass;
    SemanticFloor program;  // Will contain all floors
    std::vector<SemanticFloor> floors;  // Parsed floors
};

// ============================================================================
// Layout Resolver
// ============================================================================

class LayoutResolver {
public:
    LayoutResolver();
    ~LayoutResolver();
    
    // Main entry point: convert V11 (semantic) to V10 (geometric)
    bool Resolve(
        const SemanticBuilding& input,
        BuildingDefinition& output,
        std::string& error);
    
    void SetGridSize(float gridSize) { m_gridSize = gridSize; }
    void SetVerbose(bool verbose) { m_verbose = verbose; }
    
private:
    struct AllocatedSpace {
        std::string spaceId;
        std::string type;
        float area;
        GridPos2D position;
        GridSize2D size;
        int floorLevel;
    };
    
    // Pipeline steps
    bool CalculateFootprint(const SemanticBuilding& input);
    bool AllocateSpaces(const SemanticBuilding& input);
    bool GenerateRectangles(const SemanticBuilding& input);
    void BuildOutput(BuildingDefinition& output, const SemanticBuilding& input);
    
    // Helper functions
    float CalculateTotalArea(const std::vector<SemanticSpace>& spaces) const;
    float GetDefaultAreaForSpaceType(const std::string& spaceType) const;
    int GetPriorityWeight(const std::string& priority) const;
    float GetTargetAspectRatio(const SemanticSpace& space) const;
    bool IsCirculationSpace(const SemanticSpace& space) const;
    bool RequiresExteriorWall(const SemanticSpace& space) const;
    std::vector<const SemanticSpace*> BuildPlacementOrder(const SemanticFloor& floor) const;
    GridSize2D CalculateOptimalDimensions(float area, float aspectRatio) const;
    GridPos2D SnapToGrid(const GridPos2D& v) const;
    Rect CreateRect(const std::string& rectId, const GridPos2D& origin, const GridSize2D& size) const;
    bool RectanglesOverlap(const GridPos2D& aPos, const GridSize2D& aSize,
                           const GridPos2D& bPos, const GridSize2D& bSize) const;
    bool FitsWithoutOverlap(const GridPos2D& position,
                            const GridSize2D& size,
                            const std::vector<AllocatedSpace>& placedSpaces) const;
    bool FindFirstFitPosition(const GridSize2D& size,
                              const std::vector<AllocatedSpace>& placedSpaces,
                              GridPos2D& outPosition) const;
    bool TryPlaceNearConnectedSpace(const SemanticSpace& space,
                                    const GridSize2D& size,
                                    const std::vector<AllocatedSpace>& placedSpaces,
                                    GridPos2D& outPosition) const;
    bool TryPlaceUsingStairAlignment(const SemanticSpace& space,
                                     const GridSize2D& size,
                                     const std::vector<AllocatedSpace>& placedSpaces,
                                     GridPos2D& outPosition) const;
    bool TryPlaceInCirculationBand(const SemanticSpace& space,
                                   const GridSize2D& size,
                                   const std::vector<AllocatedSpace>& placedSpaces,
                                   GridPos2D& outPosition) const;
    bool TryPlaceOnExteriorWall(const SemanticSpace& space,
                                const GridSize2D& size,
                                const std::vector<AllocatedSpace>& placedSpaces,
                                GridPos2D& outPosition) const;
    
    float m_gridSize = 0.5f;
    bool m_verbose = false;
    
    GridSize2D m_footprint;
    std::vector<AllocatedSpace> m_allocatedSpaces;
};

// ============================================================================
// V11 JSON Parser
// ============================================================================

class SemanticBuildingParser {
public:
    static bool ParseFromFile(
        const std::string& filePath,
        SemanticBuilding& building,
        std::string& error);
    
    static bool ParseFromString(
        const std::string& jsonString,
        SemanticBuilding& building,
        std::string& error);
};

// ============================================================================
// BuildingDefinition Serializer (V10 JSON output)
// ============================================================================

class BuildingDefinitionSerializer {
public:
    static std::string Serialize(const BuildingDefinition& building);
};

} // namespace Building
} // namespace Moon
