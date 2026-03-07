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

        // Determine which floor spaceA lives on
        for (const auto& floor : definition.floors) {
            for (const auto& space : floor.spaces) {
                if (space.spaceId == adjacency.spaceA) {
                    door.floorLevel = floor.level;
                    break;
                }
            }
        }
        
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
    
    // Get usage types
    SpaceUsage usageA = spaceA->properties.usage;
    SpaceUsage usageB = spaceB->properties.usage;
    
    // Spaces that don't need doors (only need openings):
    // - Small closets (walk-in closets are open)
    // - Small storage rooms
    // - Corridor to living/open areas
    // - Open plan connections
    
    // Check if this is an open-plan connection (corridor to living/kitchen)
    if ((usageA == SpaceUsage::Corridor && (usageB == SpaceUsage::Living || usageB == SpaceUsage::Kitchen)) ||
        (usageB == SpaceUsage::Corridor && (usageA == SpaceUsage::Living || usageA == SpaceUsage::Kitchen))) {
        return false;  // No door needed for open plan
    }
    
    // Check if this is a closet connection - only small closets (<4m²) need no door
    if (usageA == SpaceUsage::Closet || usageB == SpaceUsage::Closet) {
        const Space* closet = (usageA == SpaceUsage::Closet) ? spaceA : spaceB;
        float closetArea = 0.0f;
        for (const auto& rect : closet->rects) {
            closetArea += rect.size[0] * rect.size[1];
        }
        // Small walk-in closets (< 4m²) can be open, larger ones need doors
        if (closetArea < 4.0f) {
            return false;  // Small closet - just an opening
        }
    }
    
    // Living room to dining room - open plan
    if ((usageA == SpaceUsage::Living && usageB == SpaceUsage::Dining) || 
        (usageA == SpaceUsage::Dining && usageB == SpaceUsage::Living)) {
        return false;  // Open plan living/dining
    }
    
    // All other indoor connections need doors (privacy, sound isolation)
    // - Bedrooms need doors
    // - Bathrooms need doors
    // - Offices need doors
    // - Larger storage rooms need doors
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
    
    // Check usage types
    SpaceUsage usageA = spaceA->properties.usage;
    SpaceUsage usageB = spaceB->properties.usage;
    
    // Sliding door for bathrooms or closets
    if (usageA == SpaceUsage::Bathroom || usageB == SpaceUsage::Bathroom ||
        usageA == SpaceUsage::Closet || usageB == SpaceUsage::Closet) {
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
    
    // Calculate angle from edge direction
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
