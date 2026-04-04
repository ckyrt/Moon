#pragma once

#include "BuildingTypology.h"
#include "LayoutResolver.h"

namespace Moon {
namespace Building {

float GetTypologyPlacementScoreBoost(BuildingTypology typology,
                                     const SemanticSpace& space,
                                     int floorLevel);

bool ShouldPreferCirculationAdjacency(BuildingTypology typology,
                                      const SemanticSpace& space);

} // namespace Building
} // namespace Moon
