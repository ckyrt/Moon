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
    m_edges.clear();
    m_adjacencies.clear();
    m_adjacencyMap.clear();
    
    // Extract all edges from spaces
    ExtractEdges(definition);
    
    // Find adjacencies between spaces
    FindAdjacencies();
    
    // Build connection list
    BuildConnections(outConnections);
}

bool SpaceGraphBuilder::AreSpacesAdjacent(int spaceA, int spaceB) const {
    auto it = m_adjacencyMap.find(spaceA);
    if (it == m_adjacencyMap.end()) return false;
    
    const auto& neighbors = it->second;
    return std::find(neighbors.begin(), neighbors.end(), spaceB) != neighbors.end();
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

float SpaceGraphBuilder::Edge::Length() const {
    float dx = end[0] - start[0];
    float dy = end[1] - start[1];
    return std::sqrt(dx * dx + dy * dy);
}

void SpaceGraphBuilder::ExtractEdges(const BuildingDefinition& definition) {
    for (const auto& floor : definition.floors) {
        for (const auto& space : floor.spaces) {
            for (const auto& rect : space.rects) {
                // Extract 4 edges from each rectangle
                float x = rect.origin[0];
                float y = rect.origin[1];
                float w = rect.size[0];
                float h = rect.size[1];
                
                // Bottom edge
                Edge bottom;
                bottom.start = {x, y};
                bottom.end = {x + w, y};
                bottom.spaceId = space.spaceId;
                bottom.floorLevel = floor.level;
                m_edges.push_back(bottom);
                
                // Right edge
                Edge right;
                right.start = {x + w, y};
                right.end = {x + w, y + h};
                right.spaceId = space.spaceId;
                right.floorLevel = floor.level;
                m_edges.push_back(right);
                
                // Top edge
                Edge top;
                top.start = {x + w, y + h};
                top.end = {x, y + h};
                top.spaceId = space.spaceId;
                top.floorLevel = floor.level;
                m_edges.push_back(top);
                
                // Left edge
                Edge left;
                left.start = {x, y + h};
                left.end = {x, y};
                left.spaceId = space.spaceId;
                left.floorLevel = floor.level;
                m_edges.push_back(left);
            }
        }
    }
}

void SpaceGraphBuilder::FindAdjacencies() {
    // Compare all edges to find overlaps
    for (size_t i = 0; i < m_edges.size(); ++i) {
        for (size_t j = i + 1; j < m_edges.size(); ++j) {
            const Edge& edgeA = m_edges[i];
            const Edge& edgeB = m_edges[j];
            
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
                    
                    // Update adjacency map
                    m_adjacencyMap[edgeA.spaceId].push_back(edgeB.spaceId);
                    m_adjacencyMap[edgeB.spaceId].push_back(edgeA.spaceId);
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

bool SpaceGraphBuilder::EdgesOverlap(const Edge& a, const Edge& b,
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
