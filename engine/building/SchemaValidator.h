#pragma once

#include "BuildingTypes.h"
#include "SemanticBuildingResolver.h"
#include "SemanticBuildingValidator.h"
#include "../../external/nlohmann/json.hpp"
#include <string>

namespace Moon {
namespace Building {

/**
 * @brief Schema Validator
 * Thin facade that keeps the legacy API while delegating to explicit
 * authoring-validation and semantic-resolution modules.
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
    SemanticBuildingValidator m_semanticValidator;
    SemanticBuildingResolver m_semanticResolver;
};

} // namespace Building
} // namespace Moon
