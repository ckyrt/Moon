#include "SchemaValidator.h"

#include "LayoutResolver.h"

namespace Moon {
namespace Building {

SchemaValidator::SchemaValidator() {}

SchemaValidator::~SchemaValidator() {}

bool SchemaValidator::ValidateAndParse(const std::string& jsonStr,
                                       BuildingDefinition& outDefinition,
                                       std::string& outError) {
    try {
        nlohmann::json json = nlohmann::json::parse(jsonStr);
        return ValidateAndParse(json, outDefinition, outError);
    }
    catch (const nlohmann::json::parse_error& e) {
        outError = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

bool SchemaValidator::ValidateAndParse(const nlohmann::json& json,
                                       BuildingDefinition& outDefinition,
                                       std::string& outError) {
    if (!m_semanticValidator.Validate(json, outError)) {
        return false;
    }

    SemanticBuilding semanticBuilding;
    if (!SemanticBuildingParser::ParseFromString(json.dump(), semanticBuilding, outError)) {
        return false;
    }

    if (!m_semanticResolver.Resolve(semanticBuilding, outDefinition, outError)) {
        return false;
    }

    return m_semanticResolver.ValidateResolvedConsistency(semanticBuilding, outDefinition, outError);
}

} // namespace Building
} // namespace Moon
