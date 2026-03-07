#pragma once

#include "BuildingTypes.h"
#include "SpaceGraphBuilder.h"
#include "EdgeTopologyBuilder.h"
#include <vector>
#include <unordered_map>
#include <cmath>

namespace Moon {
namespace Building {

/**
 * @brief Wall Generator
 * Generates wall segments from space boundaries
 * Rules:
 * - Same space ID -> no wall
 * - Different space ID -> interior wall
 * - No neighbor -> exterior wall
 */
class WallGenerator {
public:
    WallGenerator();
    ~WallGenerator();

    /**
     * @brief Generate walls from building definition
     * @param definition Building definition
     * @param spaceGraph Space connectivity graph
     * @param outWalls Output wall segments
     */
    void GenerateWalls(const BuildingDefinition& definition,
                      const SpaceGraphBuilder& spaceGraph,
                      std::vector<WallSegment>& outWalls);

    /**
     * @brief Set wall thickness
     */
    void SetWallThickness(float thickness);

    /**
     * @brief Set default wall height
     */
    void SetDefaultWallHeight(float height);

private:
    // Edge to spaces mapping
    struct EdgeSpaces {
        std::vector<int> spaceIds;      // All spaces that share this edge
        std::vector<float> heights;     // Heights for each space
        std::vector<bool> isOutdoors;   // Outdoor flags for each space
    };
    
    // Hash and equality for EdgeInfo (using grid-snapped coordinates)
    struct EdgeHash {
        size_t operator()(const EdgeInfo& e) const;
    };
    
    struct EdgeEqual {
        bool operator()(const EdgeInfo& a, const EdgeInfo& b) const;
    };

    void BuildEdgeMap(const std::vector<EdgeInfo>& edges,
                     std::unordered_map<EdgeInfo, EdgeSpaces, EdgeHash, EdgeEqual>& outEdgeMap);
    
    void ClassifyEdges(const std::unordered_map<EdgeInfo, EdgeSpaces, EdgeHash, EdgeEqual>& edgeMap,
                      std::vector<WallSegment>& outWalls);
    
    void MergeCollinearWalls(std::vector<WallSegment>& walls);
    
    bool EdgesMatch(const EdgeInfo& a, const EdgeInfo& b) const;
    
    bool EdgesOverlap(const EdgeInfo& a, const EdgeInfo& b,
                      GridPos2D& outStart, GridPos2D& outEnd,
                      float& outLength) const;
    
    void CreateInteriorWall(const GridPos2D& start, const GridPos2D& end,
                           int spaceIdA, int spaceIdB, int floorLevel,
                           float height, float thickness, bool hasOutdoor,
                           std::vector<WallSegment>& outWalls);
    
    void CreateExteriorWall(const GridPos2D& start, const GridPos2D& end,
                           int spaceId, int floorLevel, float height,
                           float thickness, bool isOutdoor,
                           std::vector<WallSegment>& outWalls);
    
    float GetOutdoorWallHeight(int floorLevel, float defaultHeight) const;
    
    float m_wallThickness;
    float m_defaultWallHeight;
    int m_nextWallId;  // For assigning unique wall IDs
};

} // namespace Building
} // namespace Moon
