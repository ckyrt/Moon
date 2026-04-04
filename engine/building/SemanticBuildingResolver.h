#pragma once

#include "BuildingTypes.h"

#include <string>

namespace Moon {
namespace Building {

struct SemanticBuilding;

class SemanticBuildingResolver {
public:
    bool Resolve(const SemanticBuilding& semanticBuilding,
                 BuildingDefinition& outDefinition,
                 std::string& outError) const;

    bool ValidateResolvedConsistency(const SemanticBuilding& semanticBuilding,
                                     const BuildingDefinition& resolvedBuilding,
                                     std::string& outError) const;
};

} // namespace Building
} // namespace Moon
