#include "SemanticFloorLayoutGenerator.h"

#include <algorithm>
#include <cctype>

namespace {

using namespace Moon::Building;

bool IsOfficeLike(const BuildingDefinition& definition) {
    return definition.style.category == "commercial";
}

bool IsRetailLike(const BuildingDefinition& definition) {
    return definition.style.category == "retail";
}

bool IsResidentialLike(const BuildingDefinition& definition) {
    return definition.style.category == "residential";
}

const FloorLayoutInput* FindFloorLayoutInput(const BuildingLayoutInput* layoutInput, int floorLevel) {
    if (!layoutInput) {
        return nullptr;
    }
    return layoutInput->FindFloor(floorLevel);
}

std::vector<VerticalCore> CollectFloorCores(const std::vector<VerticalCore>& cores, int floorLevel) {
    std::vector<VerticalCore> result;
    for (const auto& core : cores) {
        if (floorLevel >= core.floorFrom && floorLevel <= core.floorTo) {
            result.push_back(core);
        }
    }
    return result;
}


} // namespace

namespace Moon {
namespace Building {

bool SemanticFloorLayoutGenerator::Generate(const BuildingDefinition& definition,
                                            const BuildingFormInput* formInput,
                                            const BuildingLayoutInput* layoutInput,
                                            const std::vector<FloorPlate>& floorPlates,
                                            const std::vector<VerticalCore>& verticalCores,
                                            std::vector<Floor>& ioFloors,
                                            std::vector<ProgramBlock>* outDebugBlocks,
                                            std::string& outError) const {
    if (!layoutInput || floorPlates.empty()) {
        return true;
    }

    const std::string buildingType = formInput ? formInput->buildingType : std::string();
    const bool useOfficeSolver =
        buildingType == "office" || buildingType == "office_tower" || IsOfficeLike(definition);
    const bool useResidentialSolver =
        buildingType == "apartment" || buildingType == "cbd_residential" || buildingType == "villa" ||
        IsResidentialLike(definition);
    const bool useRetailSolver =
        buildingType == "mall" || buildingType == "shopping_center" || buildingType == "retail_center" ||
        IsRetailLike(definition);

    if (!useOfficeSolver && !useResidentialSolver && !useRetailSolver) {
        return true;
    }

    if (outDebugBlocks) {
        outDebugBlocks->clear();
    }

    int nextSpaceId = 1;
    for (const auto& floor : ioFloors) {
        for (const auto& space : floor.spaces) {
            nextSpaceId = std::max(nextSpaceId, space.spaceId + 1);
        }
    }

    for (auto& floor : ioFloors) {
        const FloorLayoutInput* semanticFloor = FindFloorLayoutInput(layoutInput, floor.level);
        if (!semanticFloor) {
            continue;
        }

        auto plateIt = std::find_if(floorPlates.begin(), floorPlates.end(),
            [&](const FloorPlate& plate) { return plate.floorLevel == floor.level; });
        if (plateIt == floorPlates.end()) {
            continue;
        }

        const std::vector<VerticalCore> floorCores = CollectFloorCores(verticalCores, floor.level);
        std::vector<Space> synthesizedSpaces;
        if (useOfficeSolver) {
            if (!m_officeSolver.GenerateFloor(
                    definition,
                    *semanticFloor,
                    *plateIt,
                    floorCores,
                    nextSpaceId,
                    synthesizedSpaces,
                    outDebugBlocks,
                    outError)) {
                return false;
            }
        } else if (useResidentialSolver) {
            if (!m_residentialSolver.GenerateFloor(
                    definition,
                    *semanticFloor,
                    *plateIt,
                    floorCores,
                    nextSpaceId,
                    synthesizedSpaces,
                    outDebugBlocks,
                    outError)) {
                return false;
            }
        } else if (useRetailSolver) {
            if (!m_retailSolver.GenerateFloor(
                    definition,
                    *semanticFloor,
                    *plateIt,
                    floorCores,
                    nextSpaceId,
                    synthesizedSpaces,
                    outDebugBlocks,
                    outError)) {
                return false;
            }
        }

        floor.spaces = std::move(synthesizedSpaces);
    }

    return true;
}

} // namespace Building
} // namespace Moon
