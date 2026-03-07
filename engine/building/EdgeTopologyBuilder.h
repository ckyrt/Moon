#pragma once

#include "BuildingTypes.h"
#include <vector>
#include <array>

namespace Moon {
namespace Building {

/**
 * @brief Edge information with space ownership
 */
struct EdgeInfo {
    GridPos2D start;
    GridPos2D end;
    int spaceId;
    int floorLevel;
    float height;
    bool isOutdoor;  // Is this edge from an outdoor space?
    
    // Canonical form: ensure start <= end for consistent comparison
    void Normalize();
    
    bool IsHorizontal() const;
    bool IsVertical() const;
    float Length() const;
};

/**
 * @brief Edge Topology Builder
 * Unified edge segmentation algorithm for T-junction handling
 * This ensures that all modules (SpaceGraph, WallGenerator, etc.) 
 * use consistent edge data.
 */
class EdgeTopologyBuilder {
public:
    EdgeTopologyBuilder();
    ~EdgeTopologyBuilder();

    /**
     * @brief Extract edges from building definition
     * @param definition Building definition
     * @return Raw edges (before segmentation)
     */
    static std::vector<EdgeInfo> ExtractEdges(const BuildingDefinition& definition);

    /**
     * @brief Segment edges at T-junctions
     * Splits edges where other edges intersect them, ensuring consistent
     * topology for adjacency detection and wall generation.
     * @param edges Input edges (will be modified in place)
     */
    static void SegmentEdges(std::vector<EdgeInfo>& edges);

    /**
     * @brief Extract and segment edges in one step
     * @param definition Building definition
     * @return Segmented edges
     */
    static std::vector<EdgeInfo> BuildTopology(const BuildingDefinition& definition);

private:
    // Helper: Check if point is on edge (between start and end)
    static bool PointOnEdge(const GridPos2D& point, 
                           const GridPos2D& start, 
                           const GridPos2D& end,
                           float epsilon = 0.001f);
    
    // Helper: Check if two edges are collinear
    static bool EdgesCollinear(const EdgeInfo& a, 
                              const EdgeInfo& b,
                              float epsilon = 0.001f);
};

} // namespace Building
} // namespace Moon
