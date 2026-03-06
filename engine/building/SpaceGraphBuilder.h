#pragma once

#include "BuildingTypes.h"
#include <vector>
#include <unordered_map>

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
    struct Edge {
        GridPos2D start;
        GridPos2D end;
        int spaceId;
        int floorLevel;
        
        bool IsHorizontal() const {
            return std::abs(start[1] - end[1]) < 0.001f;
        }
        
        bool IsVertical() const {
            return std::abs(start[0] - end[0]) < 0.001f;
        }
        
        float Length() const;
    };

    void ExtractEdges(const BuildingDefinition& definition);
    void FindAdjacencies();
    void BuildConnections(std::vector<SpaceConnection>& outConnections);
    
    bool EdgesOverlap(const Edge& a, const Edge& b, 
                     GridPos2D& outStart, GridPos2D& outEnd, 
                     float& outLength) const;
    
    std::vector<Edge> m_edges;
    std::vector<SpaceAdjacency> m_adjacencies;
    std::unordered_map<int, std::vector<int>> m_adjacencyMap;  // spaceId -> list of adjacent spaceIds
};

} // namespace Building
} // namespace Moon
