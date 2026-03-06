#include "FacadeGenerator.h"
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
                                     std::vector<Window>& outWindows,
                                     std::vector<FacadeElement>& outElements) {
    outWindows.clear();
    outElements.clear();
    
    // Generate windows on exterior walls
    GenerateWindows(definition, walls, outWindows);
    
    // Generate balconies (if style supports it)
    GenerateBalconies(definition, walls, outElements);
}

void FacadeGenerator::GenerateWindows(const BuildingDefinition& definition,
                                      const std::vector<WallSegment>& walls,
                                      std::vector<Window>& outWindows) {
    // Process only exterior walls
    for (const auto& wall : walls) {
        if (wall.type != WallType::Exterior) continue;
        
        // Get floor level from space ID
        int floorLevel = 0; // Would need to look this up from definition
        
        // Place windows based on style
        PlaceWindowsOnWall(wall, definition.style, floorLevel, outWindows);
    }
}

void FacadeGenerator::GenerateBalconies(const BuildingDefinition& definition,
                                        const std::vector<WallSegment>& walls,
                                        std::vector<FacadeElement>& outElements) {
    // Check if style supports balconies
    if (definition.style.category != "modern") {
        return; // Only modern buildings get balconies in this simple implementation
    }
    
    // Find outdoor spaces
    for (const auto& floor : definition.floors) {
        if (floor.level == 0) continue; // No balconies on ground floor
        
        for (const auto& space : floor.spaces) {
            if (space.properties.isOutdoor && space.properties.usageHint == "balcony") {
                // Create balcony element
                // (In full implementation, would create railing and floor)
                // For now, just mark it as a facade element
            }
        }
    }
}

void FacadeGenerator::PlaceWindowsOnWall(const WallSegment& wall,
                                         const BuildingStyle& style,
                                         int floorLevel,
                                         std::vector<Window>& outWindows) {
    // Calculate wall length
    float dx = wall.end[0] - wall.start[0];
    float dy = wall.end[1] - wall.start[1];
    float wallLength = std::sqrt(dx * dx + dy * dy);
    
    // Get window spacing based on style
    float spacing = GetWindowSpacing(style);
    
    // Calculate number of windows that fit
    int numWindows = static_cast<int>(wallLength / spacing);
    if (numWindows < 1) return;
    
    // Calculate actual spacing
    float actualSpacing = wallLength / (numWindows + 1);
    
    // Calculate wall rotation
    float rotation = std::atan2(dy, dx) * 180.0f / 3.14159265f;
    
    // Place windows
    for (int i = 0; i < numWindows; ++i) {
        float t = (i + 1) * actualSpacing / wallLength;
        
        Window window;
        window.position[0] = wall.start[0] + dx * t;
        window.position[1] = wall.start[1] + dy * t;
        window.rotation = rotation;
        window.width = m_windowWidth;
        window.height = m_windowHeight;
        window.sillHeight = m_windowSillHeight;
        window.spaceId = wall.spaceId;
        
        outWindows.push_back(window);
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
