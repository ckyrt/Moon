#pragma once

#include "BuildingTypes.h"
#include "SpaceGraphBuilder.h"
#include <unordered_map>
#include <vector>
#include <string>

namespace Moon {
namespace Building {

/**
 * @brief Adjacency key for fast lookup
 * Key = (spaceA, spaceB, floorLevel) where spaceA < spaceB
 */
struct AdjacencyKey {
    int spaceA;
    int spaceB;
    int floorLevel;
    
    bool operator==(const AdjacencyKey& other) const {
        return spaceA == other.spaceA && spaceB == other.spaceB && floorLevel == other.floorLevel;
    }
    
    // Create canonical key (spaceA < spaceB)
    static AdjacencyKey Make(int a, int b, int floor) {
        if (a > b) std::swap(a, b);
        return {a, b, floor};
    }
};

// Hash function for AdjacencyKey
struct AdjacencyKeyHash {
    size_t operator()(const AdjacencyKey& key) const {
        size_t h1 = std::hash<int>{}(key.spaceA);
        size_t h2 = std::hash<int>{}(key.spaceB);
        size_t h3 = std::hash<int>{}(key.floorLevel);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

/**
 * @brief Building Index
 * Unified lookup data structures for all building components.
 * Eliminates O(n) and O(n²) scanning throughout the codebase.
 * 
 * Usage:
 *   BuildingIndex index(definition, spaceGraph, walls);
 *   const Space* space = index.GetSpace(spaceId);
 *   int floorLevel = index.GetFloorLevel(spaceId);
 *   const WallSegment* wall = index.FindWall(spaceA, spaceB, floorLevel);
 */
class BuildingIndex {
public:
    BuildingIndex();
    ~BuildingIndex();
    
    /**
     * @brief Build indices from building definition and generated data
     * @param definition Building definition
     * @param spaceGraph Space connectivity graph (optional)
     * @param walls Generated walls (optional)
     */
    void Build(const BuildingDefinition& definition,
              const SpaceGraphBuilder* spaceGraph = nullptr,
              const std::vector<WallSegment>* walls = nullptr);
    
    /**
     * @brief Get space by ID (O(1))
     * @return Pointer to space, or nullptr if not found
     */
    const Space* GetSpace(int spaceId) const;
    
    /**
     * @brief Get floor level for a space (O(1))
     * @return Floor level, or -1 if space not found
     */
    int GetFloorLevel(int spaceId) const;
    
    /**
     * @brief Get floor by level (O(1))
     * @return Pointer to floor, or nullptr if not found
     */
    const Floor* GetFloor(int floorLevel) const;
    
    /**
     * @brief Get mass by ID (O(1))
     * @return Pointer to mass, or nullptr if not found
     */
    const Mass* GetMass(const std::string& massId) const;
    
    /**
     * @brief Find wall between two spaces on a specific floor (O(1))
     * @return Pointer to wall, or nullptr if not found
     */
    const WallSegment* FindWall(int spaceA, int spaceB, int floorLevel) const;
    
    /**
     * @brief Get adjacency information between two spaces (O(1))
     * @return Pointer to adjacency, or nullptr if spaces are not adjacent
     */
    const SpaceAdjacency* GetAdjacency(int spaceA, int spaceB, int floorLevel) const;
    
    /**
     * @brief Get all walls for a specific space
     */
    std::vector<const WallSegment*> GetWallsForSpace(int spaceId) const;

private:
    // Space lookup: spaceId -> Space*
    std::unordered_map<int, const Space*> m_spaceMap;
    
    // Space to floor level: spaceId -> floorLevel
    std::unordered_map<int, int> m_spaceToFloorLevel;
    
    // Floor lookup: floorLevel -> Floor*
    std::unordered_map<int, const Floor*> m_floorMap;
    
    // Mass lookup: massId -> Mass*
    std::unordered_map<std::string, const Mass*> m_massMap;
    
    // Wall lookup: (spaceA, spaceB, floorLevel) -> WallSegment*
    // Uses canonical ordering (spaceA < spaceB)
    std::unordered_map<AdjacencyKey, const WallSegment*, AdjacencyKeyHash> m_wallMap;
    
    // Adjacency lookup: (spaceA, spaceB, floorLevel) -> SpaceAdjacency*
    std::unordered_map<AdjacencyKey, const SpaceAdjacency*, AdjacencyKeyHash> m_adjacencyMap;
    
    // Space to walls: spaceId -> vector of WallSegment*
    std::unordered_map<int, std::vector<const WallSegment*>> m_spaceToWalls;
};

} // namespace Building
} // namespace Moon
