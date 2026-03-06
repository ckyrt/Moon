#include "DoorGenerator.h"
#include <cmath>

namespace Moon {
namespace Building {

DoorGenerator::DoorGenerator()
    : m_minDoorWidth(1.2f)         // 1.2m minimum for door placement
    , m_defaultDoorWidth(0.9f)     // 0.9m standard door width
    , m_defaultDoorHeight(2.1f)    // 2.1m standard door height
{
}

DoorGenerator::~DoorGenerator() {}

void DoorGenerator::SetDefaultDoorSize(float width, float height) {
    m_defaultDoorWidth = width;
    m_defaultDoorHeight = height;
}

void DoorGenerator::GenerateDoors(const BuildingDefinition& definition,
                                  const SpaceGraphBuilder& spaceGraph,
                                  const std::vector<WallSegment>& walls,
                                  std::vector<Door>& outDoors) {
    outDoors.clear();
    
    const auto& adjacencies = spaceGraph.GetAdjacencies();
    
    for (const auto& adjacency : adjacencies) {
        // Check if door should be placed
        if (!ShouldPlaceDoor(adjacency, definition)) {
            continue;
        }
        
        // Create door
        Door door;
        door.spaceA = adjacency.spaceA;
        door.spaceB = adjacency.spaceB;
        door.position = CalculateDoorPosition(adjacency, definition);
        door.rotation = CalculateDoorRotation(adjacency);
        door.type = DetermineDoorType(adjacency, definition);
        door.width = m_defaultDoorWidth;
        door.height = m_defaultDoorHeight;
        
        outDoors.push_back(door);
    }
}

bool DoorGenerator::ShouldPlaceDoor(const SpaceAdjacency& adjacency,
                                    const BuildingDefinition& definition) const {
    // Check if shared edge is long enough
    if (adjacency.sharedEdgeLength < m_minDoorWidth) {
        return false;
    }
    
    // Check if either space is outdoor (may want different rules)
    const Space* spaceA = FindSpace(definition, adjacency.spaceA);
    const Space* spaceB = FindSpace(definition, adjacency.spaceB);
    
    if (!spaceA || !spaceB) {
        return false;
    }
    
    // Don't place doors between two outdoor spaces
    if (spaceA->properties.isOutdoor && spaceB->properties.isOutdoor) {
        return false;
    }
    
    // Always place door between indoor spaces
    return true;
}

GridPos2D DoorGenerator::CalculateDoorPosition(const SpaceAdjacency& adjacency,
                                               const BuildingDefinition& definition) const {
    const Space* spaceA = FindSpace(definition, adjacency.spaceA);
    const Space* spaceB = FindSpace(definition, adjacency.spaceB);
    
    // Check for door hints in either space
    if (spaceA && HasDoorHint(*spaceA, adjacency)) {
        for (const auto& anchor : spaceA->anchors) {
            if (anchor.type == AnchorType::DoorHint) {
                return anchor.position;
            }
        }
    }
    
    if (spaceB && HasDoorHint(*spaceB, adjacency)) {
        for (const auto& anchor : spaceB->anchors) {
            if (anchor.type == AnchorType::DoorHint) {
                return anchor.position;
            }
        }
    }
    
    // Default: place door at center of shared edge
    GridPos2D center;
    center[0] = (adjacency.sharedEdgeStart[0] + adjacency.sharedEdgeEnd[0]) * 0.5f;
    center[1] = (adjacency.sharedEdgeStart[1] + adjacency.sharedEdgeEnd[1]) * 0.5f;
    
    return center;
}

DoorType DoorGenerator::DetermineDoorType(const SpaceAdjacency& adjacency,
                                          const BuildingDefinition& definition) const {
    const Space* spaceA = FindSpace(definition, adjacency.spaceA);
    const Space* spaceB = FindSpace(definition, adjacency.spaceB);
    
    if (!spaceA || !spaceB) {
        return DoorType::Interior;
    }
    
    // Balcony door if one space is outdoor
    if (spaceA->properties.isOutdoor || spaceB->properties.isOutdoor) {
        return DoorType::Balcony;
    }
    
    // Glass door for modern buildings
    // (Would need to check building style, defaulting to interior for now)
    
    // Check usage hints
    std::string usageA = spaceA->properties.usageHint;
    std::string usageB = spaceB->properties.usageHint;
    
    // Sliding door for bathrooms or closets
    if (usageA == "bathroom" || usageB == "bathroom" ||
        usageA == "closet" || usageB == "closet") {
        return DoorType::Sliding;
    }
    
    // Default interior door
    return DoorType::Interior;
}

float DoorGenerator::CalculateDoorRotation(const SpaceAdjacency& adjacency) const {
    // Calculate rotation based on edge direction
    float dx = adjacency.sharedEdgeEnd[0] - adjacency.sharedEdgeStart[0];
    float dy = adjacency.sharedEdgeEnd[1] - adjacency.sharedEdgeStart[1];
    
    // Normalize
    float length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.001f) return 0.0f;
    
    dx /= length;
    dy /= length;
    
    // Calculate angle
    float angle = std::atan2(dy, dx);
    
    // Convert to degrees
    float degrees = angle * 180.0f / 3.14159265f;
    
    return degrees;
}

bool DoorGenerator::HasDoorHint(const Space& space, const SpaceAdjacency& adjacency) const {
    for (const auto& anchor : space.anchors) {
        if (anchor.type == AnchorType::DoorHint) {
            // Check if hint is near shared edge (simplified check)
            // In a full implementation, would check if anchor is along the shared edge
            return true;
        }
    }
    return false;
}

const Space* DoorGenerator::FindSpace(const BuildingDefinition& definition, int spaceId) const {
    for (const auto& floor : definition.floors) {
        for (const auto& space : floor.spaces) {
            if (space.spaceId == spaceId) {
                return &space;
            }
        }
    }
    return nullptr;
}

} // namespace Building
} // namespace Moon
