#pragma once

#include "BuildingTypes.h"
#include "../../external/nlohmann/json.hpp"
#include <string>

namespace Moon {
namespace Building {

/**
 * @brief Schema Validator
 * Validates and parses moon_building_v8 JSON format
 */
class SchemaValidator {
public:
    SchemaValidator();
    ~SchemaValidator();

    /**
     * @brief Parse and validate JSON string
     * @param jsonStr JSON string to parse
     * @param outDefinition Output building definition
     * @param outError Error message if validation fails
     * @return true if valid, false otherwise
     */
    bool ValidateAndParse(const std::string& jsonStr, 
                         BuildingDefinition& outDefinition,
                         std::string& outError);

    /**
     * @brief Parse and validate JSON object
     * @param json JSON object to parse
     * @param outDefinition Output building definition
     * @param outError Error message if validation fails
     * @return true if valid, false otherwise
     */
    bool ValidateAndParse(const nlohmann::json& json,
                         BuildingDefinition& outDefinition,
                         std::string& outError);

private:
    bool ValidateSchema(const nlohmann::json& json, std::string& outError);
    bool ValidateGrid(const nlohmann::json& json, std::string& outError);
    bool ValidateStyle(const nlohmann::json& json, std::string& outError);
    bool ValidateMasses(const nlohmann::json& json, std::string& outError);
    bool ValidateFloors(const nlohmann::json& json, std::string& outError);
    
    bool ParseStyle(const nlohmann::json& json, BuildingStyle& style, std::string& outError);
    bool ParseMass(const nlohmann::json& json, Mass& mass, std::string& outError);
    bool ParseFloor(const nlohmann::json& json, Floor& floor, std::string& outError);
    bool ParseSpace(const nlohmann::json& json, Space& space, std::string& outError);
    bool ParseRect(const nlohmann::json& json, Rect& rect, std::string& outError);
    bool ParseAnchor(const nlohmann::json& json, Anchor& anchor, std::string& outError);
    bool ParseStairs(const nlohmann::json& json, StairConfig& stairs, std::string& outError);
    
    bool ValidateGridAlignment(float value, std::string& outError);
    bool ValidateGridPos2D(const GridPos2D& pos, std::string& outError);
    bool ValidateGridSize2D(const GridSize2D& size, std::string& outError);
};

} // namespace Building
} // namespace Moon
