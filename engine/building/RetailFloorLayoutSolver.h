#pragma once

#include "BuildingGenerationInputs.h"
#include "BuildingTypes.h"
#include <string>
#include <vector>

namespace Moon {
namespace Building {

class RetailFloorLayoutSolver {
public:
    bool GenerateFloor(const BuildingDefinition& definition,
                       const FloorLayoutInput& layoutInput,
                       const FloorPlate& floorPlate,
                       const std::vector<VerticalCore>& floorCores,
                       ResolvedFloorLayout& outResolvedFloor,
                       std::string& outError) const;
};

} // namespace Building
} // namespace Moon
