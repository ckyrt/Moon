#include "BuildingIndex.h"
#include "SpaceGraphBuilder.h"

namespace Moon {
namespace Building {

BuildingIndex::BuildingIndex() {}
BuildingIndex::~BuildingIndex() {}

void BuildingIndex::Build(const BuildingDefinition& definition,
                         const SpaceGraphBuilder* spaceGraph,
                         const std::vector<WallSegment>* walls) {
    // Clear existing indices
    m_spaceMap.clear();
    m_spaceToFloorLevel.clear();
    m_floorMap.clear();
    m_massMap.clear();
    m_wallMap.clear();
    m_adjacencyMap.clear();
    m_spaceToWalls.clear();
    
    // Build mass lookup
    for (const auto& mass : definition.masses) {
        m_massMap[mass.massId] = &mass;
    }
    
    // Build space and floor lookups
    for (const auto& floor : definition.floors) {
        m_floorMap[floor.level] = &floor;
        
        for (const auto& space : floor.spaces) {
            m_spaceMap[space.spaceId] = &space;
            m_spaceToFloorLevel[space.spaceId] = floor.level;
        }
    }
    
    // Build wall lookup
    if (walls) {
        for (const auto& wall : *walls) {
            // Interior walls: index by both spaces
            if (wall.type == WallType::Interior && wall.neighborSpaceId >= 0) {
                AdjacencyKey key = AdjacencyKey::Make(wall.spaceId, wall.neighborSpaceId, wall.floorLevel);
                m_wallMap[key] = &wall;
            }
            
            // Build space-to-walls mapping
            m_spaceToWalls[wall.spaceId].push_back(&wall);
            if (wall.neighborSpaceId >= 0) {
                m_spaceToWalls[wall.neighborSpaceId].push_back(&wall);
            }
        }
    }
    
    // Build adjacency lookup
    if (spaceGraph) {
        const auto& adjacencies = spaceGraph->GetAdjacencies();
        for (const auto& adjacency : adjacencies) {
            AdjacencyKey key = AdjacencyKey::Make(adjacency.spaceA, adjacency.spaceB, adjacency.floorLevel);
            m_adjacencyMap[key] = &adjacency;
        }
    }
}

const Space* BuildingIndex::GetSpace(int spaceId) const {
    auto it = m_spaceMap.find(spaceId);
    return (it != m_spaceMap.end()) ? it->second : nullptr;
}

int BuildingIndex::GetFloorLevel(int spaceId) const {
    auto it = m_spaceToFloorLevel.find(spaceId);
    return (it != m_spaceToFloorLevel.end()) ? it->second : -1;
}

const Floor* BuildingIndex::GetFloor(int floorLevel) const {
    auto it = m_floorMap.find(floorLevel);
    return (it != m_floorMap.end()) ? it->second : nullptr;
}

const Mass* BuildingIndex::GetMass(const std::string& massId) const {
    auto it = m_massMap.find(massId);
    return (it != m_massMap.end()) ? it->second : nullptr;
}

const WallSegment* BuildingIndex::FindWall(int spaceA, int spaceB, int floorLevel) const {
    AdjacencyKey key = AdjacencyKey::Make(spaceA, spaceB, floorLevel);
    auto it = m_wallMap.find(key);
    return (it != m_wallMap.end()) ? it->second : nullptr;
}

const SpaceAdjacency* BuildingIndex::GetAdjacency(int spaceA, int spaceB, int floorLevel) const {
    AdjacencyKey key = AdjacencyKey::Make(spaceA, spaceB, floorLevel);
    auto it = m_adjacencyMap.find(key);
    return (it != m_adjacencyMap.end()) ? it->second : nullptr;
}

std::vector<const WallSegment*> BuildingIndex::GetWallsForSpace(int spaceId) const {
    auto it = m_spaceToWalls.find(spaceId);
    return (it != m_spaceToWalls.end()) ? it->second : std::vector<const WallSegment*>{};
}

} // namespace Building
} // namespace Moon
