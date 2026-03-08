#include "EdgeTopologyBuilder.h"
#include <cmath>
#include <algorithm>

namespace Moon {
namespace Building {

void EdgeInfo::Normalize() {
    if (start[0] > end[0] || (start[0] == end[0] && start[1] > end[1])) {
        std::swap(start, end);
    }
}

bool EdgeInfo::IsHorizontal() const {
    return std::abs(start[1] - end[1]) < 0.001f;
}

bool EdgeInfo::IsVertical() const {
    return std::abs(start[0] - end[0]) < 0.001f;
}

float EdgeInfo::Length() const {
    float dx = end[0] - start[0];
    float dy = end[1] - start[1];
    return std::sqrt(dx * dx + dy * dy);
}

EdgeTopologyBuilder::EdgeTopologyBuilder() {}
EdgeTopologyBuilder::~EdgeTopologyBuilder() {}

std::vector<EdgeInfo> EdgeTopologyBuilder::ExtractEdges(const BuildingDefinition& definition) {
    std::vector<EdgeInfo> edges;
    
    for (const auto& floor : definition.floors) {
        for (const auto& space : floor.spaces) {
            // Extract edges from each rectangle in the space
            for (const auto& rect : space.rects) {
                GridPos2D bottomLeft = rect.origin;
                GridPos2D bottomRight = {rect.origin[0] + rect.size[0], rect.origin[1]};
                GridPos2D topRight = {rect.origin[0] + rect.size[0], rect.origin[1] + rect.size[1]};
                GridPos2D topLeft = {rect.origin[0], rect.origin[1] + rect.size[1]};
                
                // Create 4 edges (bottom, right, top, left)
                const float wallHeight = space.properties.ceilingHeight > 0.0f
                    ? space.properties.ceilingHeight
                    : 0.0f;
                EdgeInfo edges_arr[4] = {
                    {bottomLeft, bottomRight, space.spaceId, floor.level, wallHeight, space.properties.isOutdoor},
                    {bottomRight, topRight, space.spaceId, floor.level, wallHeight, space.properties.isOutdoor},
                    {topRight, topLeft, space.spaceId, floor.level, wallHeight, space.properties.isOutdoor},
                    {topLeft, bottomLeft, space.spaceId, floor.level, wallHeight, space.properties.isOutdoor}
                };
                
                for (auto& edge : edges_arr) {
                    edge.Normalize();
                    edges.push_back(edge);
                }
            }
        }
    }
    
    return edges;
}

bool EdgeTopologyBuilder::PointOnEdge(const GridPos2D& point, 
                                     const GridPos2D& start, 
                                     const GridPos2D& end,
                                     float epsilon) {
    float dx = end[0] - start[0];
    float dy = end[1] - start[1];
    float edgeLength = std::sqrt(dx * dx + dy * dy);
    if (edgeLength < epsilon) return false;
    
    // Distance from point to line
    float cross = std::abs((point[0] - start[0]) * dy - (point[1] - start[1]) * dx);
    if (cross / edgeLength > epsilon) return false;  // Not collinear
    
    // Check if point is between start and end
    float dot = (point[0] - start[0]) * dx + (point[1] - start[1]) * dy;
    if (dot < epsilon || dot > edgeLength * edgeLength - epsilon) return false;  // Outside range
    
    return true;
}

bool EdgeTopologyBuilder::EdgesCollinear(const EdgeInfo& a, 
                                        const EdgeInfo& b,
                                        float epsilon) {
    float dx = a.end[0] - a.start[0];
    float dy = a.end[1] - a.start[1];
    float edgeLength = std::sqrt(dx * dx + dy * dy);
    if (edgeLength < epsilon) return false;
    
    // Check if b.start and b.end are on the line defined by a
    float cross1 = std::abs((b.start[0] - a.start[0]) * dy - (b.start[1] - a.start[1]) * dx);
    float cross2 = std::abs((b.end[0] - a.start[0]) * dy - (b.end[1] - a.start[1]) * dx);
    
    return (cross1 / edgeLength < epsilon) && (cross2 / edgeLength < epsilon);
}

void EdgeTopologyBuilder::SegmentEdges(std::vector<EdgeInfo>& edges) {
    const float epsilon = 0.001f;
    
    // Collect split points for each edge
    std::vector<std::vector<GridPos2D>> splitPoints(edges.size());
    
    for (size_t i = 0; i < edges.size(); ++i) {
        const EdgeInfo& edge = edges[i];
        
        // Look for other edges that might create split points
        for (size_t j = 0; j < edges.size(); ++j) {
            if (i == j) continue;
            if (edge.floorLevel != edges[j].floorLevel) continue;
            
            const EdgeInfo& other = edges[j];
            
            // Check if edges are collinear
            if (!EdgesCollinear(edge, other)) continue;
            
            // Check if other.start is on edge (not at endpoints)
            if (PointOnEdge(other.start, edge.start, edge.end, epsilon)) {
                splitPoints[i].push_back(other.start);
            }
            
            // Check if other.end is on edge (not at endpoints)
            if (PointOnEdge(other.end, edge.start, edge.end, epsilon)) {
                splitPoints[i].push_back(other.end);
            }
        }
    }
    
    // Create new segmented edges
    std::vector<EdgeInfo> newEdges;
    
    for (size_t i = 0; i < edges.size(); ++i) {
        if (splitPoints[i].empty()) {
            // No splits needed
            newEdges.push_back(edges[i]);
            continue;
        }
        
        // Sort split points along the edge
        const EdgeInfo& edge = edges[i];
        std::vector<GridPos2D> points = splitPoints[i];
        
        // Add edge endpoints
        points.push_back(edge.start);
        points.push_back(edge.end);
        
        // Sort by distance from start
        std::sort(points.begin(), points.end(), [&](const GridPos2D& a, const GridPos2D& b) {
            float distA = std::sqrt(std::pow(a[0] - edge.start[0], 2.0f) + std::pow(a[1] - edge.start[1], 2.0f));
            float distB = std::sqrt(std::pow(b[0] - edge.start[0], 2.0f) + std::pow(b[1] - edge.start[1], 2.0f));
            return distA < distB;
        });
        
        // Remove duplicates
        auto last = std::unique(points.begin(), points.end(), [epsilon](const GridPos2D& a, const GridPos2D& b) {
            return std::abs(a[0] - b[0]) < epsilon && std::abs(a[1] - b[1]) < epsilon;
        });
        points.erase(last, points.end());
        
        // Create segments
        for (size_t k = 0; k + 1 < points.size(); ++k) {
            EdgeInfo segment = edge;
            segment.start = points[k];
            segment.end = points[k + 1];
            segment.Normalize();
            
            float length = std::sqrt(std::pow(segment.end[0] - segment.start[0], 2.0f) + 
                                   std::pow(segment.end[1] - segment.start[1], 2.0f));
            if (length > epsilon) {
                newEdges.push_back(segment);
            }
        }
    }
    
    edges = newEdges;
}

std::vector<EdgeInfo> EdgeTopologyBuilder::BuildTopology(const BuildingDefinition& definition) {
    std::vector<EdgeInfo> edges = ExtractEdges(definition);
    SegmentEdges(edges);
    return edges;
}

} // namespace Building
} // namespace Moon
