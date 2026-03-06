#pragma once

#include "BuildingTypes.h"
#include "SpaceGraphBuilder.h"
#include <vector>

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
    struct EdgeInfo {
        GridPos2D start;
        GridPos2D end;
        int spaceId;
        int floorLevel;
        float height;
    };

    void ExtractSpaceEdges(const BuildingDefinition& definition,
                          std::vector<EdgeInfo>& outEdges);
    
    void ClassifyEdges(const std::vector<EdgeInfo>& edges,
                      const SpaceGraphBuilder& spaceGraph,
                      std::vector<WallSegment>& outWalls);
    
    bool EdgesMatch(const EdgeInfo& a, const EdgeInfo& b) const;
    
    float m_wallThickness;
    float m_defaultWallHeight;
};

} // namespace Building
} // namespace Moon
