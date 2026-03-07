#include "SpaceGraphBuilder.h"
#include <cmath>
#include <algorithm>

namespace Moon {
namespace Building {

SpaceGraphBuilder::SpaceGraphBuilder() {}

SpaceGraphBuilder::~SpaceGraphBuilder() {}

void SpaceGraphBuilder::BuildGraph(const BuildingDefinition& definition,
                                   std::vector<SpaceConnection>& outConnections) {
    // Clear previous data
    m_adjacencies.clear();
    m_adjacencySet.clear();
    
    // Use EdgeTopologyBuilder to extract and segment edges
    std::vector<EdgeInfo> edges = EdgeTopologyBuilder::BuildTopology(definition);
    
    // Find adjacencies between spaces
    FindAdjacencies(edges);
    
    // Build connection list
    BuildConnections(outConnections);
}

bool SpaceGraphBuilder::AreSpacesAdjacent(int spaceA, int spaceB) const {
    // O(1) lookup using unordered_set
    return m_adjacencySet.count(MakeAdjacencyKey(spaceA, spaceB)) > 0;
}

bool SpaceGraphBuilder::GetSharedEdge(int spaceA, int spaceB, SpaceAdjacency& outAdjacency) const {
    for (const auto& adj : m_adjacencies) {
        if ((adj.spaceA == spaceA && adj.spaceB == spaceB) ||
            (adj.spaceA == spaceB && adj.spaceB == spaceA)) {
            outAdjacency = adj;
            return true;
        }
    }
    return false;
}

void SpaceGraphBuilder::FindAdjacencies(const std::vector<EdgeInfo>& edges) {
    // Compare all edges to find overlaps
    for (size_t i = 0; i < edges.size(); ++i) {
        for (size_t j = i + 1; j < edges.size(); ++j) {
            const EdgeInfo& edgeA = edges[i];
            const EdgeInfo& edgeB = edges[j];
            
            // Skip if same space or different floor
            if (edgeA.spaceId == edgeB.spaceId) continue;
            if (edgeA.floorLevel != edgeB.floorLevel) continue;
            
            // Check if edges overlap
            GridPos2D overlapStart, overlapEnd;
            float overlapLength;
            
            if (EdgesOverlap(edgeA, edgeB, overlapStart, overlapEnd, overlapLength)) {
                // Check if we already have this adjacency
                bool found = false;
                for (const auto& adj : m_adjacencies) {
                    if ((adj.spaceA == edgeA.spaceId && adj.spaceB == edgeB.spaceId) ||
                        (adj.spaceA == edgeB.spaceId && adj.spaceB == edgeA.spaceId)) {
                        found = true;
                        break;
                    }
                }
                
                if (!found && overlapLength > 0.1f) { // Minimum overlap threshold
                    SpaceAdjacency adj;
                    adj.spaceA = edgeA.spaceId;
                    adj.spaceB = edgeB.spaceId;
                    adj.sharedEdgeLength = overlapLength;
                    adj.sharedEdgeStart = overlapStart;
                    adj.sharedEdgeEnd = overlapEnd;
                    adj.floorLevel = edgeA.floorLevel;
                    m_adjacencies.push_back(adj);
                    
                    // O(1) insert into adjacency set
                    m_adjacencySet.insert(MakeAdjacencyKey(edgeA.spaceId, edgeB.spaceId));
                }
            }
        }
    }
}

void SpaceGraphBuilder::BuildConnections(std::vector<SpaceConnection>& outConnections) {
    outConnections.clear();
    
    for (const auto& adj : m_adjacencies) {
        SpaceConnection conn;
        conn.spaceA = adj.spaceA;
        conn.spaceB = adj.spaceB;
        conn.hasDoor = false; // Will be set by door generator
        outConnections.push_back(conn);
    }
}

bool SpaceGraphBuilder::EdgesOverlap(const EdgeInfo& a, const EdgeInfo& b,
                                     GridPos2D& outStart, GridPos2D& outEnd,
                                     float& outLength) const {
    const float epsilon = 0.001f;
    
    // Check if edges are parallel and collinear
    bool aHorizontal = a.IsHorizontal();
    bool bHorizontal = b.IsHorizontal();
    bool aVertical = a.IsVertical();
    bool bVertical = b.IsVertical();
    
    // Both must be horizontal or both vertical
    if (aHorizontal != bHorizontal) return false;
    if (aVertical != bVertical) return false;
    
    if (aHorizontal && bHorizontal) {
        // Check if on same Y coordinate
        if (std::abs(a.start[1] - b.start[1]) > epsilon) return false;
        
        // Find overlap on X axis
        float aMinX = std::min(a.start[0], a.end[0]);
        float aMaxX = std::max(a.start[0], a.end[0]);
        float bMinX = std::min(b.start[0], b.end[0]);
        float bMaxX = std::max(b.start[0], b.end[0]);
        
        float overlapMinX = std::max(aMinX, bMinX);
        float overlapMaxX = std::min(aMaxX, bMaxX);
        
        if (overlapMaxX > overlapMinX + epsilon) {
            outStart = {overlapMinX, a.start[1]};
            outEnd = {overlapMaxX, a.start[1]};
            outLength = overlapMaxX - overlapMinX;
            return true;
        }
    }
    else if (aVertical && bVertical) {
        // Check if on same X coordinate
        if (std::abs(a.start[0] - b.start[0]) > epsilon) return false;
        
        // Find overlap on Y axis
        float aMinY = std::min(a.start[1], a.end[1]);
        float aMaxY = std::max(a.start[1], a.end[1]);
        float bMinY = std::min(b.start[1], b.end[1]);
        float bMaxY = std::max(b.start[1], b.end[1]);
        
        float overlapMinY = std::max(aMinY, bMinY);
        float overlapMaxY = std::min(aMaxY, bMaxY);
        
        if (overlapMaxY > overlapMinY + epsilon) {
            outStart = {a.start[0], overlapMinY};
            outEnd = {a.start[0], overlapMaxY};
            outLength = overlapMaxY - overlapMinY;
            return true;
        }
    }
    
    return false;
}

} // namespace Building
} // namespace Moon
