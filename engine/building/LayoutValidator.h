#pragma once

#include "BuildingTypes.h"
#include <vector>
#include <string>

namespace Moon {
namespace Building {

/**
 * @brief Layout Validator
 * Validates the spatial layout of buildings
 * Checks for overlaps, minimum sizes, boundaries, etc.
 */
class LayoutValidator {
public:
    LayoutValidator();
    ~LayoutValidator();

    /**
     * @brief Validate building layout
     * @param definition Building definition to validate
     * @return Validation result with errors and warnings
     */
    ValidationResult Validate(const BuildingDefinition& definition);

    /**
     * @brief Set minimum room dimensions
     */
    void SetMinRoomSize(float width, float depth);

    /**
     * @brief Set wall thickness
     */
    void SetWallThickness(float thickness);

private:
    struct Rectangle {
        float x, y, width, height;
        int spaceId;
        int floorLevel;
    };

    bool ValidateMasses(const BuildingDefinition& definition, ValidationResult& result);
    bool ValidateFloorMassReferences(const BuildingDefinition& definition, ValidationResult& result);
    bool ValidateSpaceOverlaps(const BuildingDefinition& definition, ValidationResult& result);
    bool ValidateSpaceBoundaries(const BuildingDefinition& definition, ValidationResult& result);
    bool ValidateMinimumSizes(const BuildingDefinition& definition, ValidationResult& result);
    bool ValidateStairConnections(const BuildingDefinition& definition, ValidationResult& result);
    
    bool RectanglesOverlap(const Rectangle& a, const Rectangle& b) const;
    bool RectangleWithinMass(const Rectangle& rect, const Mass& mass) const;
    float GetRectangleArea(const Rect& rect) const;

    float m_minRoomWidth;
    float m_minRoomDepth;
    float m_wallThickness;
};

} // namespace Building
} // namespace Moon
