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
                                            ResolvedBuildingLayout* outResolvedLayout,
                                            std::vector<ProgramBlock>* outDebugBlocks,
                                            std::string& outError) const {
    if (!layoutInput || floorPlates.empty()) {
        return true;
    }

    const std::string buildingType = formInput ? formInput->buildingType : std::string();
    enum class LayoutTypology {
        None,
        Office,
        Residential,
        Retail
    };

    LayoutTypology typology = LayoutTypology::None;
    if (buildingType == "mall" || buildingType == "shopping_center" || buildingType == "retail_center") {
        typology = LayoutTypology::Retail;
    } else if (buildingType == "apartment" || buildingType == "cbd_residential" || buildingType == "villa") {
        typology = LayoutTypology::Residential;
    } else if (buildingType == "office" || buildingType == "office_tower") {
        typology = LayoutTypology::Office;
    } else if (IsRetailLike(definition)) {
        typology = LayoutTypology::Retail;
    } else if (IsResidentialLike(definition)) {
        typology = LayoutTypology::Residential;
    } else if (IsOfficeLike(definition)) {
        typology = LayoutTypology::Office;
    }

    if (typology == LayoutTypology::None) {
        return true;
    }

    if (outDebugBlocks) {
        outDebugBlocks->clear();
    }
    if (outResolvedLayout) {
        outResolvedLayout->buildingType = buildingType;
        outResolvedLayout->floors.clear();
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
        ResolvedFloorLayout resolvedFloor;
        if (typology == LayoutTypology::Office) {
            if (!m_officeSolver.GenerateFloor(
                    definition,
                    *semanticFloor,
                    *plateIt,
                    floorCores,
                    resolvedFloor,
                    outError)) {
                return false;
            }
        } else if (typology == LayoutTypology::Residential) {
            if (!m_residentialSolver.GenerateFloor(
                    definition,
                    *semanticFloor,
                    *plateIt,
                    floorCores,
                    resolvedFloor,
                    outError)) {
                return false;
            }
        } else if (typology == LayoutTypology::Retail) {
            if (!m_retailSolver.GenerateFloor(
                    definition,
                    *semanticFloor,
                    *plateIt,
                    floorCores,
                    resolvedFloor,
                    outError)) {
                return false;
            }
        }

        if (outDebugBlocks) {
            outDebugBlocks->insert(outDebugBlocks->end(),
                                   resolvedFloor.debugBlocks.begin(),
                                   resolvedFloor.debugBlocks.end());
        }
        if (outResolvedLayout) {
            outResolvedLayout->floors.push_back(resolvedFloor);
        }

        BuildFloorSpacesFromResolvedLayout(resolvedFloor, nextSpaceId, floor.spaces);
    }

    return true;
}

} // namespace Building
} // namespace Moon
