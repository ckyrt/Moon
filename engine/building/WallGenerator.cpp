#include "WallGenerator.h"
#include <cmath>
#include <algorithm>

namespace Moon {
namespace Building {

WallGenerator::WallGenerator()
    : m_wallThickness(0.2f)      // 0.2m default thickness
    , m_defaultWallHeight(3.0f)   // 3m default height
{
}

WallGenerator::~WallGenerator() {}

void WallGenerator::SetWallThickness(float thickness) {
    m_wallThickness = thickness;
}

void WallGenerator::SetDefaultWallHeight(float height) {
    m_defaultWallHeight = height;
}

void WallGenerator::GenerateWalls(const BuildingDefinition& definition,
                                  const SpaceGraphBuilder& spaceGraph,
                                  std::vector<WallSegment>& outWalls) {
    outWalls.clear();
    
    // Extract all space edges
    std::vector<EdgeInfo> edges;
    ExtractSpaceEdges(definition, edges);
    
    // Classify edges as interior or exterior walls
    ClassifyEdges(edges, spaceGraph, outWalls);
}

void WallGenerator::ExtractSpaceEdges(const BuildingDefinition& definition,
                                      std::vector<EdgeInfo>& outEdges) {
    for (const auto& floor : definition.floors) {
        for (const auto& space : floor.spaces) {
            float height = space.properties.ceilingHeight;
            if (height <= 0) {
                height = m_defaultWallHeight;
            }
            
            for (const auto& rect : space.rects) {
                float x = rect.origin[0];
                float y = rect.origin[1];
                float w = rect.size[0];
                float h = rect.size[1];
                
                // Extract 4 edges from rectangle
                
                // Bottom edge (Y min)
                EdgeInfo bottom;
                bottom.start = {x, y};
                bottom.end = {x + w, y};
                bottom.spaceId = space.spaceId;
                bottom.floorLevel = floor.level;
                bottom.height = height;
                outEdges.push_back(bottom);
                
                // Right edge (X max)
                EdgeInfo right;
                right.start = {x + w, y};
                right.end = {x + w, y + h};
                right.spaceId = space.spaceId;
                right.floorLevel = floor.level;
                right.height = height;
                outEdges.push_back(right);
                
                // Top edge (Y max)
                EdgeInfo top;
                top.start = {x + w, y + h};
                top.end = {x, y + h};
                top.spaceId = space.spaceId;
                top.floorLevel = floor.level;
                top.height = height;
                outEdges.push_back(top);
                
                // Left edge (X min)
                EdgeInfo left;
                left.start = {x, y + h};
                left.end = {x, y};
                left.spaceId = space.spaceId;
                left.floorLevel = floor.level;
                left.height = height;
                outEdges.push_back(left);
            }
        }
    }
}

void WallGenerator::ClassifyEdges(const std::vector<EdgeInfo>& edges,
                                  const SpaceGraphBuilder& spaceGraph,
                                  std::vector<WallSegment>& outWalls) {
    std::vector<bool> processed(edges.size(), false);
    
    for (size_t i = 0; i < edges.size(); ++i) {
        if (processed[i]) continue;
        
        const EdgeInfo& edge = edges[i];
        bool foundMatch = false;
        
        // Look for matching edge from different space on same floor
        for (size_t j = i + 1; j < edges.size(); ++j) {
            if (processed[j]) continue;
            
            const EdgeInfo& otherEdge = edges[j];
            
            // Skip if same space or different floor
            if (edge.spaceId == otherEdge.spaceId) continue;
            if (edge.floorLevel != otherEdge.floorLevel) continue;
            
            // Check if edges match (same position but opposite direction)
            if (EdgesMatch(edge, otherEdge)) {
                // Interior wall between two spaces
                WallSegment wall;
                wall.start = edge.start;
                wall.end = edge.end;
                wall.type = WallType::Interior;
                wall.spaceId = edge.spaceId;
                wall.neighborSpaceId = otherEdge.spaceId;
                wall.height = std::max(edge.height, otherEdge.height);
                wall.thickness = m_wallThickness;
                
                outWalls.push_back(wall);
                
                processed[i] = true;
                processed[j] = true;
                foundMatch = true;
                break;
            }
        }
        
        if (!foundMatch) {
            // Exterior wall (no matching edge)
            WallSegment wall;
            wall.start = edge.start;
            wall.end = edge.end;
            wall.type = WallType::Exterior;
            wall.spaceId = edge.spaceId;
            wall.neighborSpaceId = -1;
            wall.height = edge.height;
            wall.thickness = m_wallThickness;
            
            outWalls.push_back(wall);
            
            processed[i] = true;
        }
    }
}

bool WallGenerator::EdgesMatch(const EdgeInfo& a, const EdgeInfo& b) const {
    const float epsilon = 0.001f;
    
    // Check if edges are at same position
    // Edge matches if start and end points are the same (in any order)
    bool match1 = (std::abs(a.start[0] - b.start[0]) < epsilon &&
                   std::abs(a.start[1] - b.start[1]) < epsilon &&
                   std::abs(a.end[0] - b.end[0]) < epsilon &&
                   std::abs(a.end[1] - b.end[1]) < epsilon);
    
    bool match2 = (std::abs(a.start[0] - b.end[0]) < epsilon &&
                   std::abs(a.start[1] - b.end[1]) < epsilon &&
                   std::abs(a.end[0] - b.start[0]) < epsilon &&
                   std::abs(a.end[1] - b.start[1]) < epsilon);
    
    return match1 || match2;
}

} // namespace Building
} // namespace Moon
