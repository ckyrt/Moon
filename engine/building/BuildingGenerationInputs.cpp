#include "BuildingGenerationInputs.h"

namespace Moon {
namespace Building {

const FloorLayoutInput* BuildingLayoutInput::FindFloor(int level) const {
    for (const auto& floor : floors) {
        if (floor.level == level) {
            return &floor;
        }
    }
    return nullptr;
}

BuildingFormInput ExtractBuildingFormInput(const SemanticBuilding& building) {
    BuildingFormInput input;
    input.buildingType = building.buildingType;
    input.style = building.style;
    input.mass = building.mass;
    return input;
}

BuildingLayoutInput ExtractBuildingLayoutInput(const SemanticBuilding& building) {
    BuildingLayoutInput input;
    input.floors.reserve(building.floors.size());

    for (const auto& floor : building.floors) {
        FloorLayoutInput layoutFloor;
        layoutFloor.level = floor.level;
        layoutFloor.name = floor.name;
        layoutFloor.spaces = floor.spaces;
        input.floors.push_back(std::move(layoutFloor));
    }

    return input;
}

} // namespace Building
} // namespace Moon
