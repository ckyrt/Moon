#pragma once

#include "BuildingTypes.h"

#include <string>

namespace Moon {
namespace Building {

enum class BuildingTypology {
    Unknown,
    Villa,
    Residential,
    Office,
    Retail
};

BuildingTypology ClassifyBuildingType(const std::string& buildingType);
BuildingTypology InferBuildingTypology(const std::string& buildingType,
                                       const BuildingStyle& style);

bool IsResidentialTypology(BuildingTypology typology);
bool IsOfficeTypology(BuildingTypology typology);
bool IsRetailTypology(BuildingTypology typology);
bool IsVillaTypology(BuildingTypology typology);
bool IsSupportedBuildingType(const std::string& buildingType);

float GetDefaultFootprintAspectRatio(BuildingTypology typology);
float GetTypicalFloorHeight(BuildingTypology typology, int floorLevel);
float GetReasonableMaxFloorHeight(BuildingTypology typology, int floorLevel);

} // namespace Building
} // namespace Moon
