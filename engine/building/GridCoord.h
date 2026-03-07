#pragma once

#include "BuildingTypes.h"
#include <cmath>
#include <array>

namespace Moon {
namespace Building {

/**
 * @brief Grid coordinate utilities
 * Enforces integer grid coordinates to prevent floating-point errors
 * in edge comparison, hashing, and adjacency detection.
 * 
 * All coordinates are stored as integer multiples of GRID_SIZE (0.5m).
 * This eliminates issues like:
 * - 0.499999 vs 0.5 comparison failures
 * - Hash inconsistencies due to float precision
 * - Edge matching failures in T-junction detection
 */
class GridCoord {
public:
    /**
     * @brief Snap float value to grid
     * @param value Float value in meters
     * @return Grid-aligned float value
     */
    static float SnapToGrid(float value) {
        return std::round(value / GRID_SIZE) * GRID_SIZE;
    }
    
    /**
     * @brief Convert float value to integer grid units
     * @param value Float value in meters
     * @return Integer grid units (value / GRID_SIZE, rounded)
     */
    static int ToGridUnits(float value) {
        return static_cast<int>(std::round(value / GRID_SIZE));
    }
    
    /**
     * @brief Convert integer grid units to float meters
     * @param gridUnits Integer grid units
     * @return Float value in meters
     */
    static float FromGridUnits(int gridUnits) {
        return static_cast<float>(gridUnits) * GRID_SIZE;
    }
    
    /**
     * @brief Snap 2D position to grid
     */
    static GridPos2D SnapToGrid(const GridPos2D& pos) {
        return {SnapToGrid(pos[0]), SnapToGrid(pos[1])};
    }
    
    /**
     * @brief Convert 2D position to integer grid units
     */
    static std::array<int, 2> ToGridUnits(const GridPos2D& pos) {
        return {ToGridUnits(pos[0]), ToGridUnits(pos[1])};
    }
    
    /**
     * @brief Convert 2D size to integer grid units (same as position, since types are identical)
     */
    static std::array<int, 2> SizeToGridUnits(const GridSize2D& size) {
        return {ToGridUnits(size[0]), ToGridUnits(size[1])};
    }
    
    /**
     * @brief Hash function for grid positions (using integer units)
     * Ensures consistent hashing without floating-point errors
     */
    static size_t HashPosition(const GridPos2D& pos) {
        int x = ToGridUnits(pos[0]);
        int y = ToGridUnits(pos[1]);
        size_t h1 = std::hash<int>{}(x);
        size_t h2 = std::hash<int>{}(y);
        return h1 ^ (h2 << 1);
    }
    
    /**
     * @brief Hash function for edge (using integer units)
     */
    static size_t HashEdge(const GridPos2D& start, const GridPos2D& end) {
        int x1 = ToGridUnits(start[0]);
        int y1 = ToGridUnits(start[1]);
        int x2 = ToGridUnits(end[0]);
        int y2 = ToGridUnits(end[1]);
        
        size_t h1 = std::hash<int>{}(x1);
        size_t h2 = std::hash<int>{}(y1);
        size_t h3 = std::hash<int>{}(x2);
        size_t h4 = std::hash<int>{}(y2);
        
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
    
    /**
     * @brief Compare two positions for equality (using integer units)
     */
    static bool PositionsEqual(const GridPos2D& a, const GridPos2D& b) {
        return ToGridUnits(a[0]) == ToGridUnits(b[0]) && 
               ToGridUnits(a[1]) == ToGridUnits(b[1]);
    }
    
    /**
     * @brief Compare two edges for equality (using integer units)
     */
    static bool EdgesEqual(const GridPos2D& a_start, const GridPos2D& a_end,
                          const GridPos2D& b_start, const GridPos2D& b_end) {
        return PositionsEqual(a_start, b_start) && PositionsEqual(a_end, b_end);
    }
};

} // namespace Building
} // namespace Moon
