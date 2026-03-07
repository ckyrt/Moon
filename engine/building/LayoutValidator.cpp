#include "LayoutValidator.h"
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace Moon {
namespace Building {

LayoutValidator::LayoutValidator()
    : m_minRoomWidth(1.5f)    // 1.5m minimum width
    , m_minRoomDepth(1.5f)    // 1.5m minimum depth
    , m_wallThickness(0.2f)   // 0.2m wall thickness
{
}

LayoutValidator::~LayoutValidator() {}

void LayoutValidator::SetMinRoomSize(float width, float depth) {
    m_minRoomWidth = width;
    m_minRoomDepth = depth;
}

void LayoutValidator::SetWallThickness(float thickness) {
    m_wallThickness = thickness;
}

ValidationResult LayoutValidator::Validate(const BuildingDefinition& definition) {
    ValidationResult result;
    result.valid = true;

    // Validate masses
    if (!ValidateMasses(definition, result)) {
        result.valid = false;
    }

    // Validate floor-mass references
    if (!ValidateFloorMassReferences(definition, result)) {
        result.valid = false;
    }

    // Validate space overlaps
    if (!ValidateSpaceOverlaps(definition, result)) {
        result.valid = false;
    }

    // Validate space boundaries
    if (!ValidateSpaceBoundaries(definition, result)) {
        result.valid = false;
    }

    // Validate minimum sizes
    if (!ValidateMinimumSizes(definition, result)) {
        result.valid = false;
    }

    // Validate grid alignment
    if (!ValidateGridAlignment(definition, result)) {
        result.valid = false;
    }

    // Validate stair connections
    if (!ValidateStairConnections(definition, result)) {
        result.valid = false;
    }

    return result;
}

bool LayoutValidator::ValidateMasses(const BuildingDefinition& definition, ValidationResult& result) {
    bool valid = true;

    if (definition.masses.empty()) {
        result.errors.push_back("Building must have at least one mass");
        return false;
    }

    // Check for duplicate mass IDs
    std::unordered_set<std::string> massIds;
    for (const auto& mass : definition.masses) {
        if (mass.massId.empty()) {
            result.errors.push_back("Mass has empty mass_id");
            valid = false;
            continue;
        }

        if (massIds.count(mass.massId)) {
            result.errors.push_back("Duplicate mass_id: " + mass.massId);
            valid = false;
        } else {
            massIds.insert(mass.massId);
        }

        // Check mass dimensions
        if (mass.size[0] <= 0 || mass.size[1] <= 0) {
            result.errors.push_back("Mass '" + mass.massId + "' has invalid size");
            valid = false;
        }

        if (mass.floors < 1) {
            result.errors.push_back("Mass '" + mass.massId + "' must have at least 1 floor");
            valid = false;
        }
    }

    return valid;
}

bool LayoutValidator::ValidateFloorMassReferences(const BuildingDefinition& definition, ValidationResult& result) {
    bool valid = true;

    // Build mass ID set
    std::unordered_set<std::string> massIds;
    std::unordered_map<std::string, int> massFloorCounts;
    for (const auto& mass : definition.masses) {
        massIds.insert(mass.massId);
        massFloorCounts[mass.massId] = mass.floors;
    }

    // Check floor references
    for (const auto& floor : definition.floors) {
        if (!massIds.count(floor.massId)) {
            std::ostringstream oss;
            oss << "Floor level " << floor.level << " references unknown mass_id: " << floor.massId;
            result.errors.push_back(oss.str());
            valid = false;
        } else {
            // Check floor level is valid for this mass
            int maxLevel = massFloorCounts[floor.massId] - 1;
            if (floor.level < 0 || floor.level > maxLevel) {
                std::ostringstream oss;
                oss << "Floor level " << floor.level << " is out of range for mass '" 
                    << floor.massId << "' (max level: " << maxLevel << ")";
                result.errors.push_back(oss.str());
                valid = false;
            }
        }
    }

    return valid;
}

bool LayoutValidator::ValidateSpaceOverlaps(const BuildingDefinition& definition, ValidationResult& result) {
    bool valid = true;

    // Group spaces by floor level
    std::unordered_map<int, std::vector<Rectangle>> floorSpaces;

    for (const auto& floor : definition.floors) {
        for (const auto& space : floor.spaces) {
            for (const auto& rect : space.rects) {
                Rectangle r;
                r.x = rect.origin[0];
                r.y = rect.origin[1];
                r.width = rect.size[0];
                r.height = rect.size[1];
                r.spaceId = space.spaceId;
                r.floorLevel = floor.level;
                floorSpaces[floor.level].push_back(r);
            }
        }
    }

    // Check for overlaps within each floor
    for (auto it = floorSpaces.begin(); it != floorSpaces.end(); ++it) {
        int level = it->first;
        const auto& rects = it->second;
        for (size_t i = 0; i < rects.size(); ++i) {
            for (size_t j = i + 1; j < rects.size(); ++j) {
                if (rects[i].spaceId != rects[j].spaceId && RectanglesOverlap(rects[i], rects[j])) {
                    std::ostringstream oss;
                    oss << "Overlap detected on floor " << level 
                        << " between space " << rects[i].spaceId 
                        << " and space " << rects[j].spaceId;
                    result.errors.push_back(oss.str());
                    valid = false;
                }
            }
        }
    }

    return valid;
}

bool LayoutValidator::ValidateSpaceBoundaries(const BuildingDefinition& definition, ValidationResult& result) {
    bool valid = true;

    // Build mass lookup
    std::unordered_map<std::string, const Mass*> masses;
    for (const auto& mass : definition.masses) {
        masses[mass.massId] = &mass;
    }

    for (const auto& floor : definition.floors) {
        const Mass* mass = masses[floor.massId];
        if (!mass) continue;

        for (const auto& space : floor.spaces) {
            for (const auto& rect : space.rects) {
                Rectangle r;
                r.x = rect.origin[0];
                r.y = rect.origin[1];
                r.width = rect.size[0];
                r.height = rect.size[1];
                r.spaceId = space.spaceId;
                r.floorLevel = floor.level;

                if (!RectangleWithinMass(r, *mass)) {
                    std::ostringstream oss;
                    oss << "Space " << space.spaceId << " on floor " << floor.level
                        << " extends outside mass '" << floor.massId << "' boundaries";
                    result.errors.push_back(oss.str());
                    valid = false;
                }
            }
        }
    }

    return valid;
}

bool LayoutValidator::ValidateMinimumSizes(const BuildingDefinition& definition, ValidationResult& result) {
    bool valid = true;

    for (const auto& floor : definition.floors) {
        for (const auto& space : floor.spaces) {
            for (const auto& rect : space.rects) {
                // Check minimum dimensions - treat as errors (not warnings)
                if (rect.size[0] < m_minRoomWidth || rect.size[1] < m_minRoomDepth) {
                    std::ostringstream oss;
                    oss << "Space " << space.spaceId << " rect '" << rect.rectId 
                        << "' is smaller than minimum size (" << m_minRoomWidth 
                        << "m x " << m_minRoomDepth << "m)";
                    result.errors.push_back(oss.str());
                    valid = false;
                }
            }
        }
    }

    return valid;
}

bool LayoutValidator::ValidateGridAlignment(const BuildingDefinition& definition, ValidationResult& result) {
    bool valid = true;
    const float grid = definition.grid;
    const float epsilon = 0.001f;

    // Helper lambda to check if a value is aligned to grid
    auto isAligned = [grid, epsilon](float value) -> bool {
        float remainder = fmodf(value, grid);
        return (remainder < epsilon) || (remainder > grid - epsilon);
    };

    // Check mass alignment
    for (const auto& mass : definition.masses) {
        if (!isAligned(mass.origin[0]) || !isAligned(mass.origin[1])) {
            std::ostringstream oss;
            oss << "Mass '" << mass.massId << "' origin (" 
                << mass.origin[0] << ", " << mass.origin[1] 
                << ") is not aligned to grid (" << grid << ")";
            result.errors.push_back(oss.str());
            valid = false;
        }
        if (!isAligned(mass.size[0]) || !isAligned(mass.size[1])) {
            std::ostringstream oss;
            oss << "Mass '" << mass.massId << "' size (" 
                << mass.size[0] << ", " << mass.size[1] 
                << ") is not aligned to grid (" << grid << ")";
            result.errors.push_back(oss.str());
            valid = false;
        }
    }

    // Check space alignment
    for (const auto& floor : definition.floors) {
        for (const auto& space : floor.spaces) {
            for (const auto& rect : space.rects) {
                if (!isAligned(rect.origin[0]) || !isAligned(rect.origin[1])) {
                    std::ostringstream oss;
                    oss << "Space " << space.spaceId << " rect '" << rect.rectId 
                        << "' origin (" << rect.origin[0] << ", " << rect.origin[1] 
                        << ") is not aligned to grid (" << grid << ")";
                    result.errors.push_back(oss.str());
                    valid = false;
                }
                if (!isAligned(rect.size[0]) || !isAligned(rect.size[1])) {
                    std::ostringstream oss;
                    oss << "Space " << space.spaceId << " rect '" << rect.rectId 
                        << "' size (" << rect.size[0] << ", " << rect.size[1] 
                        << ") is not aligned to grid (" << grid << ")";
                    result.errors.push_back(oss.str());
                    valid = false;
                }
            }
        }
    }

    return valid;
}

bool LayoutValidator::ValidateStairConnections(const BuildingDefinition& definition, ValidationResult& result) {
    bool valid = true;

    // Build floor level set
    std::unordered_set<int> floorLevels;
    for (const auto& floor : definition.floors) {
        floorLevels.insert(floor.level);
    }

    // Check stair connections
    for (const auto& floor : definition.floors) {
        for (const auto& space : floor.spaces) {
            if (space.properties.hasStairs) {
                int targetLevel = space.stairsConfig.connectToLevel;
                if (!floorLevels.count(targetLevel)) {
                    std::ostringstream oss;
                    oss << "Stairs in space " << space.spaceId << " on floor " 
                        << floor.level << " connect to non-existent floor " << targetLevel;
                    result.errors.push_back(oss.str());
                    valid = false;
                }

                // Check stairs connect to adjacent floor
                int levelDiff = std::abs(targetLevel - floor.level);
                if (levelDiff != 1) {
                    std::ostringstream oss;
                    oss << "Stairs in space " << space.spaceId << " on floor " 
                        << floor.level << " connect to non-adjacent floor " << targetLevel;
                    result.warnings.push_back(oss.str());
                }
            }
        }
    }

    return valid;
}

bool LayoutValidator::RectanglesOverlap(const Rectangle& a, const Rectangle& b) const {
    // Check if rectangles overlap (with small tolerance for floating point)
    const float epsilon = 0.001f;
    
    if (a.x + a.width <= b.x + epsilon) return false;  // a is left of b
    if (b.x + b.width <= a.x + epsilon) return false;  // b is left of a
    if (a.y + a.height <= b.y + epsilon) return false; // a is below b
    if (b.y + b.height <= a.y + epsilon) return false; // b is below a
    
    return true;
}

bool LayoutValidator::RectangleWithinMass(const Rectangle& rect, const Mass& mass) const {
    const float epsilon = 0.001f;
    
    float massMinX = mass.origin[0];
    float massMinY = mass.origin[1];
    float massMaxX = mass.origin[0] + mass.size[0];
    float massMaxY = mass.origin[1] + mass.size[1];
    
    float rectMinX = rect.x;
    float rectMinY = rect.y;
    float rectMaxX = rect.x + rect.width;
    float rectMaxY = rect.y + rect.height;
    
    bool withinX = (rectMinX >= massMinX - epsilon) && (rectMaxX <= massMaxX + epsilon);
    bool withinY = (rectMinY >= massMinY - epsilon) && (rectMaxY <= massMaxY + epsilon);
    
    return withinX && withinY;
}

float LayoutValidator::GetRectangleArea(const Rect& rect) const {
    return rect.size[0] * rect.size[1];
}

} // namespace Building
} // namespace Moon
