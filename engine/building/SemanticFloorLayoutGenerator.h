#pragma once

#include "BuildingGenerationInputs.h"
#include "OfficeFloorLayoutSolver.h"
#include "ResidentialFloorLayoutSolver.h"
#include "RetailFloorLayoutSolver.h"
#include "BuildingTypes.h"
#include <string>
#include <vector>

namespace Moon {
namespace Building {

class SemanticFloorLayoutGenerator {
public:
    bool Generate(const BuildingDefinition& definition,
                  const BuildingFormInput* formInput,
                  const BuildingLayoutInput* layoutInput,
                  const std::vector<FloorPlate>& floorPlates,
                  const std::vector<VerticalCore>& verticalCores,
                  std::vector<Floor>& ioFloors,
                  std::vector<ProgramBlock>* outDebugBlocks,
                  std::string& outError) const;

private:
    OfficeFloorLayoutSolver m_officeSolver;
    ResidentialFloorLayoutSolver m_residentialSolver;
    RetailFloorLayoutSolver m_retailSolver;
};

} // namespace Building
} // namespace Moon
