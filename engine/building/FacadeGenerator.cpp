#include "FacadeGenerator.h"
#include "BuildingIndex.h"
#include <cmath>

namespace Moon {
namespace Building {

FacadeGenerator::FacadeGenerator()
    : m_windowWidth(1.2f)       // 1.2m default window width
    , m_windowHeight(1.5f)      // 1.5m default window height
    , m_windowSillHeight(0.9f)  // 0.9m sill height from floor
{
}

FacadeGenerator::~FacadeGenerator() {}

void FacadeGenerator::SetWindowParameters(float width, float height, float sillHeight) {
    m_windowWidth = width;
    m_windowHeight = height;
    m_windowSillHeight = sillHeight;
}

void FacadeGenerator::GenerateFacade(const BuildingDefinition& definition,
                                     const std::vector<WallSegment>& walls,
                                     const BuildingIndex& index,
                                     std::vector<Window>& outWindows,
                                     std::vector<FacadeElement>& outElements) {
    outWindows.clear();
    outElements.clear();
    
    // Generate windows on exterior walls
    GenerateWindows(definition, walls, index, outWindows);
    
    // Generate balconies (if style supports it)
    GenerateBalconies(definition, walls, index, outElements);
}

void FacadeGenerator::GenerateWindows(const BuildingDefinition& definition,
                                      const std::vector<WallSegment>& walls,
                                      const BuildingIndex& index,
                                      std::vector<Window>& outWindows) {
    // Process only exterior walls
    for (const auto& wall : walls) {
        if (wall.type != WallType::Exterior) continue;
        PlaceWindowsOnWall(wall, definition, index, outWindows);
    }
}

void FacadeGenerator::GenerateBalconies(const BuildingDefinition& definition,
                                        const std::vector<WallSegment>& walls,
                                        const BuildingIndex& index,
                                        std::vector<FacadeElement>& outElements) {
    // TODO: Implement balcony generation
    // Current implementation only creates placeholder logic
    // Full implementation would:
    // 1. Identify outdoor spaces (balconies, terraces)
    // 2. Generate railing elements around their perimeter  
    // 3. Create floor platforms
    // 4. Add support structures if needed
    
    // For now, just check if style supports balconies
    if (definition.style.category != "modern") {
        return; // Only modern buildings get balconies in this simple implementation
    }
    
    // Find outdoor spaces on upper floors
    for (const auto& floor : definition.floors) {
        if (floor.level == 0) continue; // No balconies on ground floor
        
        for (const auto& space : floor.spaces) {
            if (space.properties.isOutdoor && space.properties.usage == SpaceUsage::Balcony) {
                // Create balcony railing element (simplified)
                FacadeElement element;
                element.type = FacadeElementType::Railing;
                element.floorLevel = floor.level;
                element.spaceId = space.spaceId;
                // In full implementation, would calculate proper position, dimensions, etc.
                outElements.push_back(element);
            }
        }
    }
}

void FacadeGenerator::PlaceWindowsOnWall(const WallSegment& wall,
                                         const BuildingDefinition& definition,
                                         const BuildingIndex& index,
                                         std::vector<Window>& outWindows) {
    // Use BuildingIndex for O(1) space lookup instead of nested loops
    const Space* space = index.GetSpace(wall.spaceId);
    
    if (!space) return;
    
    // Don't place windows on outdoor spaces (balconies, terraces, etc.)
    if (space->properties.isOutdoor) {
        return;
    }
    
    // Determine if this space type needs windows
    SpaceUsage usage = space->properties.usage;
    
    // Spaces that should NOT have windows
    if (usage == SpaceUsage::Bathroom ||    // Privacy
        usage == SpaceUsage::Storage ||     // No need for light
        usage == SpaceUsage::Garage ||      // No need for windows
        usage == SpaceUsage::Laundry ||     // Usually no windows
        usage == SpaceUsage::Closet) {      // Small spaces, no windows
        return;
    }
    
    // Corridors typically don't have windows (unless specified otherwise)
    if (usage == SpaceUsage::Corridor) {
        return;
    }
    
    // Calculate wall length
    float dx = wall.end[0] - wall.start[0];
    float dy = wall.end[1] - wall.start[1];
    float wallLength = std::sqrt(dx * dx + dy * dy);
    
    // Minimum wall length to place windows (walls too short don't get windows)
    const float minWallLength = 2.0f;  // 2m minimum
    if (wallLength < minWallLength) {
        return;
    }
    
    const BuildingStyle& style = definition.style;
    
    // Get window spacing based on space usage and style
    float spacing = GetWindowSpacing(style);
    
    // Adjust window density based on space usage
    if (usage == SpaceUsage::Living || usage == SpaceUsage::Dining) {
        spacing *= 0.8f;  // More windows for living/dining rooms
    } else if (usage == SpaceUsage::Kitchen || usage == SpaceUsage::Office) {
        spacing *= 1.2f;  // Moderate windows for kitchen/office
    } else if (usage == SpaceUsage::Bedroom) {
        spacing *= 1.0f;  // Standard windows for bedrooms
    } else {
        spacing *= 1.5f;  // Fewer windows for other spaces
    }
    
    // Calculate number of windows that fit
    // Consider window width and minimum spacing between windows
    const float minEdgeMargin = 0.3f;  // 30cm minimum margin from wall edges
    const float minWindowSpacing = m_windowWidth / 2.0f + 0.5f;  // Half window width + 0.5m gap
    
    int numWindows = static_cast<int>((wallLength - 2 * minEdgeMargin + minWindowSpacing) / (spacing + m_windowWidth));
    
    // Limit max windows per wall (avoid too many windows on one wall)
    const int maxWindowsPerWall = 4;
    numWindows = std::min(numWindows, maxWindowsPerWall);
    
    if (numWindows < 1) return;
    
    // Calculate actual spacing between window centers
    float totalWindowWidth = numWindows * m_windowWidth;
    float availableSpaceForGaps = wallLength - totalWindowWidth - 2 * minEdgeMargin;
    float actualSpacing = availableSpaceForGaps / (numWindows + 1);
    
    // If spacing is too tight, reduce number of windows
    if (actualSpacing < minWindowSpacing) {
        numWindows = std::max(1, numWindows - 1);
        totalWindowWidth = numWindows * m_windowWidth;
        availableSpaceForGaps = wallLength - totalWindowWidth - 2 * minEdgeMargin;
        actualSpacing = availableSpaceForGaps / (numWindows + 1);
    }
    
    if (actualSpacing < 0.2f) return;  // Wall is too narrow
    
    // Calculate wall rotation
    float rotation = std::atan2(dy, dx) * 180.0f / 3.14159265f;
    
    // Place windows with proper spacing
    float currentOffset = minEdgeMargin + actualSpacing + m_windowWidth / 2.0f;
    for (int i = 0; i < numWindows; ++i) {
        float t = currentOffset / wallLength;
        
        Window window;
        window.position[0] = wall.start[0] + dx * t;
        window.position[1] = wall.start[1] + dy * t;
        window.rotation = rotation;
        window.width = m_windowWidth;
        window.height = m_windowHeight;
        window.sillHeight = m_windowSillHeight;
        window.spaceId = wall.spaceId;
        window.floorLevel = wall.floorLevel;
        
        outWindows.push_back(window);
        
        // Move to next window position
        currentOffset += m_windowWidth + actualSpacing;
    }
}

float FacadeGenerator::GetWindowSpacing(const BuildingStyle& style) const {
    // Different styles have different window spacing
    if (style.windowStyle == "full_height") {
        return 2.0f; // 2m spacing for modern full-height windows
    } else if (style.windowStyle == "standard") {
        return 2.5f; // 2.5m standard spacing
    } else {
        return 3.0f; // 3m for small windows
    }
}

} // namespace Building
} // namespace Moon
