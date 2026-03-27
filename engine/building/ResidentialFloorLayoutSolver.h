#pragma once

#include "BuildingGenerationInputs.h"
#include "BuildingTypes.h"
#include <string>
#include <vector>

namespace Moon {
namespace Building {

class ResidentialFloorLayoutSolver {
public:
    bool GenerateFloor(const BuildingDefinition& definition,
                       const FloorLayoutInput& layoutInput,
                       const FloorPlate& floorPlate,
                       const std::vector<VerticalCore>& floorCores,
                       int& ioNextSpaceId,
                       std::vector<Space>& outSpaces,
                       std::vector<ProgramBlock>* outDebugBlocks,
                       std::string& outError) const;
};

} // namespace Building
} // namespace Moon
