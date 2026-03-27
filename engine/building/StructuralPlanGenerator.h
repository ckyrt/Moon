#pragma once

#include "BuildingTypes.h"
#include <string>
#include <vector>

namespace Moon {
namespace Building {

class StructuralPlanGenerator {
public:
    bool Generate(const BuildingDefinition& definition,
                  const std::vector<FloorPlate>& slicedFloorPlates,
                  GeneratedBuilding& outBuilding,
                  std::string& outError) const;
};

} // namespace Building
} // namespace Moon
