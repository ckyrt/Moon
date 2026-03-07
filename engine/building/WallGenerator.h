#pragma once

#include "BuildingTypes.h"
#include "SpaceGraphBuilder.h"
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
    struct EdgeInfo {
        GridPos2D start;
        GridPos2D end;
        int spaceId;
        int floorLevel;
        float height;
        bool isOutdoor;  // Is this edge from an outdoor space?
        
        // Canonical form: ensure start <= end for consistent comparison
        void Normalize() {
            if (start[0] > end[0] || (start[0] == end[0] && start[1] > end[1])) {
                std::swap(start, end);
            }
        }
        
        // Hash for use in unordered_map
        struct Hash {
            size_t operator()(const EdgeInfo& e) const {
                // Snap to grid precision to ensure consistent hashing
                // Use coarser precision than Equal epsilon to ensure hash consistency
                // precision = 0.01m (1cm), epsilon = 0.001m (1mm)
                // This guarantees: if |a-b| < 0.001m, then snap(a) == snap(b) with high probability
                const float precision = 0.01f;  // 1cm grid for hash
                
                auto SnapToGrid = [precision](float value) -> int {
                    return static_cast<int>(std::round(value / precision));
                };
                
                // Hash based on snapped integer coordinates
                int x1 = SnapToGrid(e.start[0]);
                int y1 = SnapToGrid(e.start[1]);
                int x2 = SnapToGrid(e.end[0]);
                int y2 = SnapToGrid(e.end[1]);
                int level = e.floorLevel;
                
                // Combine hashes
                size_t h1 = std::hash<int>{}(x1);
                size_t h2 = std::hash<int>{}(y1);
                size_t h3 = std::hash<int>{}(x2);
                size_t h4 = std::hash<int>{}(y2);
                size_t h5 = std::hash<int>{}(level);
                
                return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
            }
        };
        
        // Equality based on position and floor level (not spaceId)
        struct Equal {
            bool operator()(const EdgeInfo& a, const EdgeInfo& b) const {
                const float epsilon = 0.01f;  // 1cm - MUST match Hash precision!
                return std::abs(a.start[0] - b.start[0]) < epsilon &&
                       std::abs(a.start[1] - b.start[1]) < epsilon &&
                       std::abs(a.end[0] - b.end[0]) < epsilon &&
                       std::abs(a.end[1] - b.end[1]) < epsilon &&
                       a.floorLevel == b.floorLevel;
            }
        };
    };
    
    // Edge to spaces mapping
    struct EdgeSpaces {
        std::vector<int> spaceIds;      // All spaces that share this edge
        std::vector<float> heights;     // Heights for each space
        std::vector<bool> isOutdoors;   // Outdoor flags for each space
    };

    void ExtractSpaceEdges(const BuildingDefinition& definition,
                          std::vector<EdgeInfo>& outEdges);
    
    void SegmentEdges(std::vector<EdgeInfo>& edges);
    
    void BuildEdgeMap(const std::vector<EdgeInfo>& edges,
                     std::unordered_map<EdgeInfo, EdgeSpaces, EdgeInfo::Hash, EdgeInfo::Equal>& outEdgeMap);
    
    void ClassifyEdges(const std::unordered_map<EdgeInfo, EdgeSpaces, EdgeInfo::Hash, EdgeInfo::Equal>& edgeMap,
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
};

} // namespace Building
} // namespace Moon
