#pragma once

#include "BuildingTypes.h"
#include <string>
#include <vector>

namespace Moon {
namespace Building {

class MassFloorPlateGenerator {
public:
    bool Generate(const BuildingDefinition& definition,
                  std::vector<FloorPlate>& outPlates,
                  std::string& outError) const;
};

} // namespace Building
} // namespace Moon
