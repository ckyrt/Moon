#pragma once

#include "BuildingTypes.h"
#include "SpaceGraphBuilder.h"
#include "BuildingIndex.h"
#include <vector>

namespace Moon {
namespace Building {

/**
 * @brief Door Generator
 * Places doors between connected spaces
 * Algorithm:
 * 1. Use BuildingIndex to find wall between spaces (O(1))
 * 2. Check edge length >= minimum width
 * 3. Check door_hint anchors
 * 4. Place door and record wallId
 */
class DoorGenerator {
public:
    DoorGenerator();
    ~DoorGenerator();

    /**
     * @brief Generate doors from space connectivity
     * @param definition Building definition
     * @param spaceGraph Space connectivity graph
     * @param walls Wall segments
     * @param index Building index for fast lookups
     * @param outDoors Output door placements
     */
    void GenerateDoors(const BuildingDefinition& definition,
                      const SpaceGraphBuilder& spaceGraph,
                      const std::vector<WallSegment>& walls,
                      const BuildingIndex& index,
                      std::vector<Door>& outDoors);

    /**
     * @brief Set minimum door width
     */
    void SetMinDoorWidth(float width) { m_minDoorWidth = width; }

    /**
     * @brief Set default door dimensions
     */
    void SetDefaultDoorSize(float width, float height);

private:
    bool ShouldPlaceDoor(const SpaceAdjacency& adjacency,
                        const BuildingIndex& index) const;
    
    GridPos2D CalculateDoorPosition(const SpaceAdjacency& adjacency,
                                   const BuildingIndex& index) const;
    
    DoorType DetermineDoorType(const SpaceAdjacency& adjacency,
                               const BuildingIndex& index) const;
    
    float CalculateDoorRotation(const SpaceAdjacency& adjacency) const;
    
    bool HasDoorHint(const Space& space, const SpaceAdjacency& adjacency) const;
    
    float m_minDoorWidth;
    float m_defaultDoorWidth;
    float m_defaultDoorHeight;
};

} // namespace Building
} // namespace Moon
