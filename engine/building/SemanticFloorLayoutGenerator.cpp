#include "SemanticFloorLayoutGenerator.h"
#include "BuildingTypology.h"

#include <algorithm>
#include <cctype>

namespace {

using namespace Moon::Building;

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

bool FloorRequestsCoreType(const FloorLayoutInput& floor, VerticalCoreType type) {
    for (const auto& space : floor.spaces) {
        if ((space.type == "core" || space.type == "stairs") && type == VerticalCoreType::Stair) {
            return true;
        }
        if (space.type == "elevator" && type == VerticalCoreType::Elevator) {
            return true;
        }
        if (space.type == "mechanical" && type == VerticalCoreType::Service) {
            return true;
        }
    }
    return false;
}

bool LayoutDeclaresTransportTypeForFloor(const BuildingLayoutInput* layoutInput,
                                         int floorLevel,
                                         VerticalCoreType type) {
    if (!layoutInput) {
        return false;
    }

    for (const auto& system : layoutInput->verticalSystems) {
        if (floorLevel < system.floorFrom || floorLevel > system.floorTo) {
            continue;
        }
        if (type == VerticalCoreType::Stair &&
            (system.type == "stair" || system.type == "stairwell")) {
            return true;
        }
        if (type == VerticalCoreType::Elevator && system.type == "elevator") {
            return true;
        }
    }

    return false;
}

std::vector<VerticalCore> FilterFloorCoresForSemanticInput(const std::vector<VerticalCore>& cores,
                                                           const FloorLayoutInput& floor,
                                                           const BuildingLayoutInput* layoutInput,
                                                           int floorLevel) {
    std::vector<VerticalCore> filtered;
    for (const auto& core : cores) {
        if (FloorRequestsCoreType(floor, core.type) ||
            LayoutDeclaresTransportTypeForFloor(layoutInput, floorLevel, core.type)) {
            filtered.push_back(core);
        }
    }
    return filtered;
}

bool FloorDeclaresCoreLikeSpaces(const FloorLayoutInput& floor) {
    for (const auto& space : floor.spaces) {
        if (space.type == "core" || space.type == "stairs" || space.type == "elevator" ||
            space.type == "mechanical") {
            return true;
        }
    }
    return false;
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
    const BuildingTypology typology = InferBuildingTypology(buildingType, definition.style);

    if (typology == BuildingTypology::Unknown || typology == BuildingTypology::Villa) {
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

        std::vector<VerticalCore> floorCores = CollectFloorCores(verticalCores, floor.level);
        const std::vector<VerticalCore> inferredFloorCores = floorCores;
        floorCores = FilterFloorCoresForSemanticInput(floorCores, *semanticFloor, layoutInput, floor.level);
        if (floorCores.empty() && !inferredFloorCores.empty()) {
            floorCores = inferredFloorCores;
        }
        const bool needsStructuredCoreLayout = typology == BuildingTypology::Office ||
            typology == BuildingTypology::Residential;
        if (needsStructuredCoreLayout && floorCores.empty() && !FloorDeclaresCoreLikeSpaces(*semanticFloor) &&
            !LayoutDeclaresTransportTypeForFloor(layoutInput, floor.level, VerticalCoreType::Stair) &&
            !LayoutDeclaresTransportTypeForFloor(layoutInput, floor.level, VerticalCoreType::Elevator)) {
            continue;
        }

        ResolvedFloorLayout resolvedFloor;
        resolvedFloor.verticalTransports.clear();
        if (typology == BuildingTypology::Office) {
            if (!m_officeSolver.GenerateFloor(
                    definition,
                    *semanticFloor,
                    *plateIt,
                    floorCores,
                    resolvedFloor,
                    outError)) {
                return false;
            }
        } else if (typology == BuildingTypology::Residential) {
            if (!m_residentialSolver.GenerateFloor(
                    definition,
                    *semanticFloor,
                    *plateIt,
                    floorCores,
                    resolvedFloor,
                    outError)) {
                return false;
            }
        } else if (typology == BuildingTypology::Retail) {
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
