#pragma once

#include "BuildingTypes.h"
#include "../../external/nlohmann/json.hpp"
#include <string>
#include <unordered_set>

namespace Moon {
namespace Building {

struct SemanticBuilding;

/**
 * @brief Schema Validator
 * Validates semantic building JSON and resolves it to internal geometry
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

    bool ValidateAndParseBestEffort(const std::string& jsonStr,
                                    BuildingDefinition& outDefinition,
                                    BestEffortGenerationReport& outReport,
                                    std::string& outError);

    bool ValidateAndParseBestEffort(const nlohmann::json& json,
                                    BuildingDefinition& outDefinition,
                                    BestEffortGenerationReport& outReport,
                                    std::string& outError);

private:
    bool ValidateSemanticJson(const nlohmann::json& json, std::string& outError);
    bool ValidateSchema(const nlohmann::json& json, std::string& outError);
    bool ValidateGrid(const nlohmann::json& json, std::string& outError);
    bool ValidateBuildingType(const nlohmann::json& json, std::string& outError);
    bool ValidateStyle(const nlohmann::json& json, std::string& outError);
    bool ValidateMass(const nlohmann::json& json, std::string& outError);
    bool ValidateProgram(const nlohmann::json& json, std::string& outError);
    bool ValidateFloors(const nlohmann::json& floorsJson, int massFloors, std::string& outError);
    bool ValidateFloor(const nlohmann::json& floorJson, std::string& outError);
    bool ValidateSpace(const nlohmann::json& spaceJson,
                       std::unordered_set<std::string>& knownSpaceIds,
                       std::string& outError);
    bool ValidateAdjacency(const nlohmann::json& adjacencyJson, std::string& outError);
    bool ValidateConstraints(const nlohmann::json& constraintsJson, std::string& outError);
    bool ValidateTypology(const nlohmann::json& json, std::string& outError);
    bool ValidateResolvedSemanticConsistency(const SemanticBuilding& semanticBuilding,
                                            const BuildingDefinition& resolvedBuilding,
                                            std::string& outError);
};

} // namespace Building
} // namespace Moon
