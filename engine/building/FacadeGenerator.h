#pragma once

#include "BuildingTypes.h"
#include <vector>

namespace Moon {
namespace Building {

// Forward declaration
class BuildingIndex;

/**
 * @brief Facade element types
 */
enum class FacadeElementType {
    Window,
    Balcony,
    Column,
    Panel,
    Railing
};

/**
 * @brief Facade element
 */
struct FacadeElement {
    FacadeElementType type;
    GridPos2D position;         // 2D position on facade
    float height;               // Height on facade (Z coordinate)
    float width;
    float elementHeight;
    std::string style;          // Style identifier
    int floorLevel;             // Floor level
    int spaceId;                // Associated space ID
};

/**
 * @brief Facade Generator
 * Generates architectural elements for exterior walls
 * Pipeline:
 * - Identify exterior walls
 * - Create facade grid
 * - Place windows based on style
 * - Add balconies, columns, panels
 * - Generate railings
 */
class FacadeGenerator {
public:
    FacadeGenerator();
    ~FacadeGenerator();

    /**
     * @brief Generate facade elements
     * @param definition Building definition
     * @param walls Wall segments
     * @param index Building index for fast lookups
     * @param outWindows Output window placements
     * @param outElements Output facade elements
     */
    void GenerateFacade(const BuildingDefinition& definition,
                       const std::vector<WallSegment>& walls,
                       const BuildingIndex& index,
                       std::vector<Window>& outWindows,
                       std::vector<FacadeElement>& outElements);

    /**
     * @brief Set window parameters
     */
    void SetWindowParameters(float width, float height, float sillHeight);

private:
    void GenerateWindows(const BuildingDefinition& definition,
                        const std::vector<WallSegment>& walls,
                        const BuildingIndex& index,
                        std::vector<Window>& outWindows);
    
    void GenerateBalconies(const BuildingDefinition& definition,
                          const std::vector<WallSegment>& walls,
                          const BuildingIndex& index,
                          std::vector<FacadeElement>& outElements);
    
    void PlaceWindowsOnWall(const WallSegment& wall,
                           const BuildingDefinition& definition,
                           const BuildingIndex& index,
                           std::vector<Window>& outWindows);
    
    float GetWindowSpacing(const BuildingStyle& style) const;
    
    float m_windowWidth;
    float m_windowHeight;
    float m_windowSillHeight;
};

} // namespace Building
} // namespace Moon
