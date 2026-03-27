#pragma once

#include "LayoutResolver.h"
#include <string>
#include <vector>

namespace Moon {
namespace Building {

struct BuildingFormInput {
    std::string buildingType;
    BuildingStyle style;
    MassConstraints mass;
};

struct FloorLayoutInput {
    int level = 0;
    std::string name;
    std::vector<SemanticSpace> spaces;
};

struct BuildingLayoutInput {
    std::vector<FloorLayoutInput> floors;

    const FloorLayoutInput* FindFloor(int level) const;
};

BuildingFormInput ExtractBuildingFormInput(const SemanticBuilding& building);
BuildingLayoutInput ExtractBuildingLayoutInput(const SemanticBuilding& building);

} // namespace Building
} // namespace Moon
