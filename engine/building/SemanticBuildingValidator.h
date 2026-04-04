#pragma once

#include "../../external/nlohmann/json.hpp"

#include <string>
#include <unordered_set>

namespace Moon {
namespace Building {

class SemanticBuildingValidator {
public:
    bool Validate(const nlohmann::json& json, std::string& outError) const;

private:
    bool ValidateSchema(const nlohmann::json& json, std::string& outError) const;
    bool ValidateGrid(const nlohmann::json& json, std::string& outError) const;
    bool ValidateBuildingType(const nlohmann::json& json, std::string& outError) const;
    bool ValidateStyle(const nlohmann::json& json, std::string& outError) const;
    bool ValidateMass(const nlohmann::json& json, std::string& outError) const;
    bool ValidateProgram(const nlohmann::json& json, std::string& outError) const;
    bool ValidateFloors(const nlohmann::json& floorsJson, int massFloors, std::string& outError) const;
    bool ValidateFloor(const nlohmann::json& floorJson, std::string& outError) const;
    bool ValidateSpace(const nlohmann::json& spaceJson,
                       std::unordered_set<std::string>& knownSpaceIds,
                       std::string& outError) const;
    bool ValidateAdjacency(const nlohmann::json& adjacencyJson, std::string& outError) const;
    bool ValidateConstraints(const nlohmann::json& constraintsJson, std::string& outError) const;
    bool ValidateTypology(const nlohmann::json& json, std::string& outError) const;
};

} // namespace Building
} // namespace Moon
