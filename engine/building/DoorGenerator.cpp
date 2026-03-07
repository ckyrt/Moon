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
                                  const BuildingIndex& index,
                                  std::vector<Door>& outDoors) {
    outDoors.clear();
    
    const auto& adjacencies = spaceGraph.GetAdjacencies();
    
    for (const auto& adjacency : adjacencies) {
        // Check if door should be placed
        if (!ShouldPlaceDoor(adjacency, index)) {
            continue;
        }
        
        // Use BuildingIndex to find wall (O(1) instead of O(n) loop)
        const WallSegment* wall = index.FindWall(adjacency.spaceA, adjacency.spaceB, adjacency.floorLevel);
        
        if (!wall) {
            // No wall found for this adjacency, skip door placement
            continue;
        }
        
        // Create door
        Door door;
        door.wallId = wall->wallId;  // Record which wall this door is in
        door.spaceA = adjacency.spaceA;
        door.spaceB = adjacency.spaceB;
        door.position = CalculateDoorPosition(adjacency, index);
        door.rotation = CalculateDoorRotation(adjacency);
        door.type = DetermineDoorType(adjacency, index);
        door.width = m_defaultDoorWidth;
        door.height = m_defaultDoorHeight;
        door.floorLevel = adjacency.floorLevel;  // Use floor level from adjacency
        
        outDoors.push_back(door);
    }
}

bool DoorGenerator::ShouldPlaceDoor(const SpaceAdjacency& adjacency,
                                    const BuildingIndex& index) const {
    // Check if shared edge is long enough
    if (adjacency.sharedEdgeLength < m_minDoorWidth) {
        return false;
    }
    
    // Use BuildingIndex for O(1) space lookup
    const Space* spaceA = index.GetSpace(adjacency.spaceA);
    const Space* spaceB = index.GetSpace(adjacency.spaceB);
    
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
                                               const BuildingIndex& index) const {
    const Space* spaceA = index.GetSpace(adjacency.spaceA);
    const Space* spaceB = index.GetSpace(adjacency.spaceB);
    
    const float maxDistanceToEdge = 0.5f;
    
    // Check for door hints in either space - but they must be near this edge
    auto findClosestHint = [&](const Space* space) -> GridPos2D* {
        static GridPos2D hintPos;
        float bestDist = maxDistanceToEdge;
        bool found = false;
        
        if (!space) return nullptr;
        
        for (const auto& anchor : space->anchors) {
            if (anchor.type == AnchorType::DoorHint) {
                // Calculate distance from anchor to the edge line segment
                float ax = anchor.position[0];
                float ay = anchor.position[1];
                float x1 = adjacency.sharedEdgeStart[0];
                float y1 = adjacency.sharedEdgeStart[1];
                float x2 = adjacency.sharedEdgeEnd[0];
                float y2 = adjacency.sharedEdgeEnd[1];
                
                float dx = x2 - x1;
                float dy = y2 - y1;
                float edgeLength = std::sqrt(dx*dx + dy*dy);
                
                if (edgeLength < 0.01f) continue;
                
                float nx = dx / edgeLength;
                float ny = dy / edgeLength;
                float vx = ax - x1;
                float vy = ay - y1;
                float t = vx*nx + vy*ny;
                t = std::max(0.0f, std::min(t, edgeLength));
                float px = x1 + t * nx;
                float py = y1 + t * ny;
                float dist = std::sqrt((ax-px)*(ax-px) + (ay-py)*(ay-py));
                
                if (dist < bestDist) {
                    bestDist = dist;
                    hintPos[0] = px;  // Use projected position on edge
                    hintPos[1] = py;
                    found = true;
                }
            }
        }
        
        return found ? &hintPos : nullptr;
    };
    
    // Try spaceA hints first
    if (GridPos2D* hint = findClosestHint(spaceA)) {
        return *hint;
    }
    
    // Try spaceB hints  
    if (GridPos2D* hint = findClosestHint(spaceB)) {
        return *hint;
    }
    
    // Default: place door at center of shared edge
    GridPos2D center;
    center[0] = (adjacency.sharedEdgeStart[0] + adjacency.sharedEdgeEnd[0]) * 0.5f;
    center[1] = (adjacency.sharedEdgeStart[1] + adjacency.sharedEdgeEnd[1]) * 0.5f;
    
    return center;
}

DoorType DoorGenerator::DetermineDoorType(const SpaceAdjacency& adjacency,
                                          const BuildingIndex& index) const {
    const Space* spaceA = index.GetSpace(adjacency.spaceA);
    const Space* spaceB = index.GetSpace(adjacency.spaceB);
    
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
    const float maxDistanceToEdge = 0.5f;  // DoorHint must be within 0.5m of edge
    
    for (const auto& anchor : space.anchors) {
        if (anchor.type == AnchorType::DoorHint) {
            // Check if hint is near the shared edge
            // Calculate distance from anchor to the edge line segment
            float ax = anchor.position[0];
            float ay = anchor.position[1];
            float x1 = adjacency.sharedEdgeStart[0];
            float y1 = adjacency.sharedEdgeStart[1];
            float x2 = adjacency.sharedEdgeEnd[0];
            float y2 = adjacency.sharedEdgeEnd[1];
            
            // Vector from edge start to end
            float dx = x2 - x1;
            float dy = y2 - y1;
            float edgeLength = std::sqrt(dx*dx + dy*dy);
            
            if (edgeLength < 0.01f) continue;  // Degenerate edge
            
            // Normalized direction
            float nx = dx / edgeLength;
            float ny = dy / edgeLength;
            
            // Vector from edge start to anchor
            float vx = ax - x1;
            float vy = ay - y1;
            
            // Project onto edge
            float t = vx*nx + vy*ny;
            
            // Clamp t to [0, edgeLength]
            t = std::max(0.0f, std::min(t, edgeLength));
            
            // Closest point on edge
            float px = x1 + t * nx;
            float py = y1 + t * ny;
            
            // Distance from anchor to edge
            float dist = std::sqrt((ax-px)*(ax-px) + (ay-py)*(ay-py));
            
            if (dist < maxDistanceToEdge) {
                return true;
            }
        }
    }
    return false;
}

} // namespace Building
} // namespace Moon
