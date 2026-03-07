#pragma once

#include "BuildingTypes.h"
#include "EdgeTopologyBuilder.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace Moon {
namespace Building {

/**
 * @brief Space adjacency information
 */
struct SpaceAdjacency {
    int spaceA;
    int spaceB;
    float sharedEdgeLength;     // Length of shared wall
    GridPos2D sharedEdgeStart;  // Start point of shared edge
    GridPos2D sharedEdgeEnd;    // End point of shared edge
    int floorLevel;             // Floor level where spaces are adjacent
};

/**
 * @brief Space Graph Builder
 * Analyzes spatial relationships and builds connectivity graph
 */
class SpaceGraphBuilder {
public:
    SpaceGraphBuilder();
    ~SpaceGraphBuilder();

    /**
     * @brief Build space graph from building definition
     * @param definition Building definition
     * @param outConnections Output space connections
     */
    void BuildGraph(const BuildingDefinition& definition,
                   std::vector<SpaceConnection>& outConnections);

    /**
     * @brief Get adjacency information
     */
    const std::vector<SpaceAdjacency>& GetAdjacencies() const { return m_adjacencies; }

    /**
     * @brief Check if two spaces are adjacent
     */
    bool AreSpacesAdjacent(int spaceA, int spaceB) const;

    /**
     * @brief Get shared edge between two spaces
     */
    bool GetSharedEdge(int spaceA, int spaceB, SpaceAdjacency& outAdjacency) const;

private:
    void FindAdjacencies(const std::vector<EdgeInfo>& edges);
    void BuildConnections(std::vector<SpaceConnection>& outConnections);
    
    bool EdgesOverlap(const EdgeInfo& a, const EdgeInfo& b, 
                     GridPos2D& outStart, GridPos2D& outEnd, 
                     float& outLength) const;
    
    // Helper function to create unique key for space pair
    static uint64_t MakeAdjacencyKey(int spaceA, int spaceB) {
        int minSpace = (spaceA < spaceB) ? spaceA : spaceB;
        int maxSpace = (spaceA < spaceB) ? spaceB : spaceA;
        return ((uint64_t)minSpace << 32) | (uint64_t)maxSpace;
    }
    
    std::vector<SpaceAdjacency> m_adjacencies;
    std::unordered_set<uint64_t> m_adjacencySet;  // O(1) lookup: key = (min(A,B) << 32) | max(A,B)
};

} // namespace Building
} // namespace Moon
