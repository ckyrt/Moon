#include "WallGenerator.h"
#include "EdgeTopologyBuilder.h"
#include "GridCoord.h"
#include "core/Logging/Logger.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace Moon {
namespace Building {

WallGenerator::WallGenerator()
    : m_wallThickness(0.2f)      // 0.2m default thickness
    , m_defaultWallHeight(3.0f)   // 3m default height
    , m_nextWallId(0)            // Start wall ID counter
{
}

WallGenerator::~WallGenerator() {}

void WallGenerator::SetWallThickness(float thickness) {
    m_wallThickness = thickness;
}

void WallGenerator::SetDefaultWallHeight(float height) {
    m_defaultWallHeight = height;
}

size_t WallGenerator::EdgeHash::operator()(const EdgeInfo& e) const {
    // Use GridCoord for consistent hashing with integer units
    return GridCoord::HashEdge(e.start, e.end) ^ std::hash<int>{}(e.floorLevel);
}

bool WallGenerator::EdgeEqual::operator()(const EdgeInfo& a, const EdgeInfo& b) const {
    // Use GridCoord for consistent equality with integer units
    return GridCoord::EdgesEqual(a.start, a.end, b.start, b.end) && 
           a.floorLevel == b.floorLevel;
}

void WallGenerator::GenerateWalls(const BuildingDefinition& definition,
                                  const SpaceGraphBuilder& spaceGraph,
                                  std::vector<WallSegment>& outWalls) {
    outWalls.clear();
    m_nextWallId = 0;  // Reset wall ID counter
    
    MOON_LOG_INFO("Building", "=== Starting Wall Generation ===");
    
    // Step 1: Use EdgeTopologyBuilder to extract and segment edges
    // This replaces ExtractSpaceEdges() and SegmentEdges()
    std::vector<EdgeInfo> edges = EdgeTopologyBuilder::BuildTopology(definition);
    MOON_LOG_INFO("Building", "EdgeTopologyBuilder generated %zu segmented edges", edges.size());
    
    // Step 2: Build edge-to-spaces mapping (deduplicates and merges)
    std::unordered_map<EdgeInfo, EdgeSpaces, EdgeHash, EdgeEqual> edgeMap;
    BuildEdgeMap(edges, edgeMap);
    MOON_LOG_INFO("Building", "After deduplication: %zu unique edges", edgeMap.size());
    
    // Step 2.5: Remove internal edges (rect seams within same space)
    // If an edge appears twice with same spaceId, it's an internal seam
    for (auto it = edgeMap.begin(); it != edgeMap.end(); ) {
        const auto& spaces = it->second;
        
        // Check if same space appears twice (rect seam)
        if (spaces.spaceIds.size() == 2 && 
            spaces.spaceIds[0] == spaces.spaceIds[1]) {
            // Internal edge: both sides are same space (rect seam)
            it = edgeMap.erase(it);
        } else {
            ++it;
        }
    }
    MOON_LOG_INFO("Building", "After removing internal seams: %zu boundary edges", edgeMap.size());
    
    // Step 3: Classify edges as interior or exterior walls
    ClassifyEdges(edgeMap, outWalls);
    MOON_LOG_INFO("Building", "Generated %zu wall segments", outWalls.size());
    
    // Step 4: Merge collinear walls of same type
    // This fixes T-junction segmentation side-effects where one wall is split into
    // multiple segments (some interior, some exterior)
    MergeCollinearWalls(outWalls);
    MOON_LOG_INFO("Building", "After merging collinear walls: %zu walls", outWalls.size());
    
    MOON_LOG_INFO("Building", "=== Wall Generation Complete ===");
}

void WallGenerator::BuildEdgeMap(const std::vector<EdgeInfo>& edges,
                                 std::unordered_map<EdgeInfo, EdgeSpaces, EdgeHash, EdgeEqual>& outEdgeMap) {
    // ============================================================================
    // Edge Count Algorithm for Rect Union
    // ============================================================================
    // Problem: Space can have multiple rects (e.g., L-shaped = 2 rects).
    //          Rect seams should NOT generate walls.
    //
    // Solution: Count how many times each edge appears per space:
    //           - Edge appears 1x → Boundary (keep)
    //           - Edge appears 2x with same space → Internal seam (remove later)
    //
    // Example:
    //   +----+----+
    //   | A1 | A2 |  ← Same space A
    //   +----+----+
    //        ↑ Middle edge appears 2x: [A, A] → Removed in next step
    //
    // This is the standard method used in Unity/Sims/Roblox building systems.
    // No polygon union needed - just edge counting!
    // ============================================================================
    
    for (const auto& edge : edges) {
        // Find or create entry for this edge (position-based)
        auto it = outEdgeMap.find(edge);
        if (it == outEdgeMap.end()) {
            // First time seeing this edge
            EdgeSpaces spaces;
            spaces.spaceIds.push_back(edge.spaceId);
            spaces.heights.push_back(edge.height);
            spaces.isOutdoors.push_back(edge.isOutdoor);
            outEdgeMap[edge] = spaces;
        } else {
            // Edge already exists, add this space to the list
            // ✅ IMPORTANT: Do NOT filter duplicates here!
            //    We need to count same space twice to detect rect seams
            it->second.spaceIds.push_back(edge.spaceId);
            it->second.heights.push_back(edge.height);
            it->second.isOutdoors.push_back(edge.isOutdoor);
        }
    }
    
    int singleSpace = 0, twoSpaces = 0, multiSpace = 0;
    for (const auto& pair : outEdgeMap) {
        size_t count = pair.second.spaceIds.size();
        if (count == 1) singleSpace++;
        else if (count == 2) twoSpaces++;
        else multiSpace++;
    }
    MOON_LOG_INFO("Building", "Edge map: 1 space (exterior): %d, 2 spaces (interior): %d", singleSpace, twoSpaces);
    if (multiSpace > 0) {
        MOON_LOG_INFO("Building", "ERROR: 3+ spaces sharing edge: %d", multiSpace);
    }
}

void WallGenerator::ClassifyEdges(const std::unordered_map<EdgeInfo, EdgeSpaces, EdgeHash, EdgeEqual>& edgeMap,
                                  std::vector<WallSegment>& outWalls) {
    // ============================================================================
    // Wall Classification Rules (Correct)
    // ============================================================================
    // edge owners = 1 → exterior wall
    // edge owners = 2 → interior wall (shared edge = interior wall, always!)
    // edge owners > 2 → invalid layout (impossible in 2D without overlap)
    //
    // DO NOT check adjacency here! If two spaces share an edge, they MUST be
    // interior wall. If adjacency detection fails, fix SpaceGraphBuilder.
    // ============================================================================
    
    for (const auto& pair : edgeMap) {
        const EdgeInfo& edge = pair.first;
        const EdgeSpaces& spaces = pair.second;
        
        if (spaces.spaceIds.size() == 1) {
            // ✅ Only one space → Exterior wall
            CreateExteriorWall(edge.start, edge.end, spaces.spaceIds[0], 
                             edge.floorLevel, spaces.heights[0], 
                             m_wallThickness, spaces.isOutdoors[0], outWalls);
        } 
        else if (spaces.spaceIds.size() == 2) {
            // ✅ Two spaces share this edge → Interior wall (guaranteed!)
            int spaceA = spaces.spaceIds[0];
            int spaceB = spaces.spaceIds[1];
            bool bothOutdoor = spaces.isOutdoors[0] && spaces.isOutdoors[1];
            
            // ✅ Safety rule: If both spaces are outdoor (e.g., balcony | balcony),
            //    don't generate wall (only railing needed, handled elsewhere)
            if (bothOutdoor) {
                continue;
            }
             
            bool hasOutdoor = spaces.isOutdoors[0] || spaces.isOutdoors[1];
            float maxHeight = std::max(spaces.heights[0], spaces.heights[1]);
            
            CreateInteriorWall(edge.start, edge.end, 
                             spaceA, spaceB, 
                             edge.floorLevel, maxHeight, m_wallThickness, 
                             hasOutdoor, outWalls);
        }
        else {
            // ❌ 3+ spaces sharing one edge → geometry error
            MOON_LOG_INFO("Building", "ERROR: Edge at (%.2f,%.2f)-(%.2f,%.2f) shared by %zu spaces",
                   edge.start[0], edge.start[1], edge.end[0], edge.end[1], spaces.spaceIds.size());
        }
    }
}

void WallGenerator::CreateInteriorWall(const GridPos2D& start, const GridPos2D& end,
                                       int spaceIdA, int spaceIdB, int floorLevel,
                                       float height, float thickness, bool hasOutdoor,
                                       std::vector<WallSegment>& outWalls) {
    WallSegment wall;
    wall.wallId = m_nextWallId++;  // Assign unique wall ID
    wall.start = start;
    wall.end = end;
    wall.type = WallType::Interior;
    wall.spaceId = spaceIdA;
    wall.neighborSpaceId = spaceIdB;
    wall.height = height;
    wall.thickness = thickness;
    wall.floorLevel = floorLevel;
    
    if (hasOutdoor) {
        MOON_LOG_INFO("Building", "Interior wall (balcony connection) at (%.2f,%.2f)-(%.2f,%.2f)",
               start[0], start[1], end[0], end[1]);
    }
    
    outWalls.push_back(wall);
}

void WallGenerator::CreateExteriorWall(const GridPos2D& start, const GridPos2D& end,
                                       int spaceId, int floorLevel, float height,
                                       float thickness, bool isOutdoor,
                                       std::vector<WallSegment>& outWalls) {
    WallSegment wall;
    wall.wallId = m_nextWallId++;  // Assign unique wall ID
    wall.start = start;
    wall.end = end;
    wall.type = WallType::Exterior;
    wall.spaceId = spaceId;
    wall.neighborSpaceId = -1;
    wall.thickness = thickness;
    wall.floorLevel = floorLevel;
    
    if (isOutdoor) {
        wall.height = GetOutdoorWallHeight(floorLevel, height);
    } else {
        wall.height = height;
    }
    
    outWalls.push_back(wall);
}

float WallGenerator::GetOutdoorWallHeight(int floorLevel, float defaultHeight) const {
    // TODO: This should be configurable via WallStyleRule or BuildingStyle
    // For now, use simple heuristics:
    
    // Rooftop terraces (high floors) might need higher railings for safety
    if (floorLevel >= 3) {
        return 1.2f;  // 1.2m for high-rise balconies (safety code)
    }
    
    // Ground level or low-rise balconies
    return 1.0f;  // Standard 1m railing
    
    // Future extension point:
    // - Check space usage (terrace vs balcony vs patio)
    // - Check building style (modern glass vs traditional solid)
    // - Check region/safety codes
    // - Support configurable WallStyleRule:
    //   * Railing (0.9-1.2m)
    //   * Half wall (1.5m)
    //   * Glass wall (full height, transparent)
    //   * Privacy wall (2-2.5m, opaque)
}

void WallGenerator::MergeCollinearWalls(std::vector<WallSegment>& walls) {
    // ============================================================================
    // Merge Collinear Wall Segments
    // ============================================================================
    // Problem: T-junction segmentation splits walls into multiple segments.
    //          If one segment has 2 owners (interior) and another has 1 owner
    //          (exterior), we get "half interior, half exterior" visual bug.
    //
    // Solution: Merge collinear wall segments of the same type:
    //          - Same wall type (interior/exterior)
    //          - Same floor level
    //          - Same space IDs (for interior walls)
    //          - Collinear (same line)
    //          - Connected (end of one = start of another)
    // ============================================================================
    
    if (walls.empty()) return;
    
    const float epsilon = 0.001f;
    
    auto AreCollinear = [epsilon](const WallSegment& a, const WallSegment& b) -> bool {
        // Check if two walls are on the same infinite line
        float dx1 = a.end[0] - a.start[0];
        float dy1 = a.end[1] - a.start[1];
        float dx2 = b.end[0] - b.start[0];
        float dy2 = b.end[1] - b.start[1];
        
        float len1 = std::sqrt(dx1*dx1 + dy1*dy1);
        float len2 = std::sqrt(dx2*dx2 + dy2*dy2);
        
        if (len1 < epsilon || len2 < epsilon) return false;
        
        // Normalize direction vectors
        dx1 /= len1; dy1 /= len1;
        dx2 /= len2; dy2 /= len2;
        
        // Check if parallel (or anti-parallel)
        float dot = dx1*dx2 + dy1*dy2;
        if (std::abs(std::abs(dot) - 1.0f) > epsilon) return false;
        
        // Check if point b.start lies on line through a
        float toStartX = b.start[0] - a.start[0];
        float toStartY = b.start[1] - a.start[1];
        float cross = dx1 * toStartY - dy1 * toStartX;
        
        return std::abs(cross) < epsilon;
    };
    
    auto AreConnected = [epsilon](const WallSegment& a, const WallSegment& b) -> bool {
        // Check if walls share a vertex
        return (std::abs(a.end[0] - b.start[0]) < epsilon && std::abs(a.end[1] - b.start[1]) < epsilon) ||
               (std::abs(a.start[0] - b.end[0]) < epsilon && std::abs(a.start[1] - b.end[1]) < epsilon) ||
               (std::abs(a.end[0] - b.end[0]) < epsilon && std::abs(a.end[1] - b.end[1]) < epsilon) ||
               (std::abs(a.start[0] - b.start[0]) < epsilon && std::abs(a.start[1] - b.start[1]) < epsilon);
    };
    
    auto CanMerge = [&](const WallSegment& a, const WallSegment& b) -> bool {
        // Must be same type
        if (a.type != b.type) return false;
        
        // Must be same floor
        if (a.floorLevel != b.floorLevel) return false;
        
        // For interior walls, must have same space IDs (order doesn't matter)
        if (a.type == WallType::Interior) {
            bool sameSpaces = (a.spaceId == b.spaceId && a.neighborSpaceId == b.neighborSpaceId) ||
                             (a.spaceId == b.neighborSpaceId && a.neighborSpaceId == b.spaceId);
            if (!sameSpaces) return false;
        } else {
            // For exterior walls, must be same space
            if (a.spaceId != b.spaceId) return false;
        }
        
        // Must be collinear and connected
        return AreCollinear(a, b) && AreConnected(a, b);
    };
    
    // Iteratively merge walls until no more merges possible
    bool merged = true;
    int mergeCount = 0;
    while (merged) {
        merged = false;
        
        for (size_t i = 0; i < walls.size(); ++i) {
            for (size_t j = i + 1; j < walls.size(); ++j) {
                if (CanMerge(walls[i], walls[j])) {
                    // Merge wall[j] into wall[i]
                    // Find the two endpoints that are farthest apart
                    std::vector<GridPos2D> points = {
                        walls[i].start, walls[i].end,
                        walls[j].start, walls[j].end
                    };
                    
                    // Find min and max along the wall direction
                    float minDist = 0, maxDist = 0;
                    GridPos2D minPoint = points[0], maxPoint = points[0];
                    
                    for (const auto& p : points) {
                        float dist = (p[0] - points[0][0]) * (p[0] - points[0][0]) +
                                   (p[1] - points[0][1]) * (p[1] - points[0][1]);
                        if (dist < minDist) {
                            minDist = dist;
                            minPoint = p;
                        }
                        if (dist > maxDist) {
                            maxDist = dist;
                            maxPoint = p;
                        }
                    }
                    
                    // Update wall[i] with merged endpoints
                    walls[i].start = minPoint;
                    walls[i].end = maxPoint;
                    walls[i].height = std::max(walls[i].height, walls[j].height);
                    
                    // Remove wall[j]
                    walls.erase(walls.begin() + j);
                    
                    mergeCount++;
                    merged = true;
                    break;
                }
            }
            if (merged) break;
        }
    }
    
    if (mergeCount > 0) {
        MOON_LOG_INFO("Building", "Merged %d collinear wall segments", mergeCount);
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

bool WallGenerator::EdgesOverlap(const EdgeInfo& a, const EdgeInfo& b,
                                 GridPos2D& outStart, GridPos2D& outEnd,
                                 float& outLength) const {
    const float epsilon = 0.001f;
    
    // Check if edges are horizontal or vertical
    float aDx = std::abs(a.end[0] - a.start[0]);
    float aDy = std::abs(a.end[1] - a.start[1]);
    float bDx = std::abs(b.end[0] - b.start[0]);
    float bDy = std::abs(b.end[1] - b.start[1]);
    
    bool aHorizontal = (aDy < epsilon);
    bool aVertical = (aDx < epsilon);
    bool bHorizontal = (bDy < epsilon);
    bool bVertical = (bDx < epsilon);
    
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
