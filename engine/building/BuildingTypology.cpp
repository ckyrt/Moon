#include "BuildingTypology.h"

namespace Moon {
namespace Building {

namespace {

constexpr float kTypicalOfficeFloorHeight = 3.8f;
constexpr float kTypicalOfficeGroundFloorHeight = 4.5f;
constexpr float kTypicalRetailFloorHeight = 4.5f;
constexpr float kTypicalRetailGroundFloorHeight = 5.0f;
constexpr float kTypicalResidentialFloorHeight = 3.0f;
constexpr float kTypicalResidentialGroundFloorHeight = 3.5f;
constexpr float kTypicalFallbackFloorHeight = 3.0f;

} // namespace

BuildingTypology ClassifyBuildingType(const std::string& buildingType) {
    if (buildingType == "villa") {
        return BuildingTypology::Villa;
    }
    if (buildingType == "apartment" || buildingType == "cbd_residential") {
        return BuildingTypology::Residential;
    }
    if (buildingType == "office" || buildingType == "office_tower") {
        return BuildingTypology::Office;
    }
    if (buildingType == "mall" ||
        buildingType == "shopping_mall" ||
        buildingType == "shopping_center" ||
        buildingType == "retail_center") {
        return BuildingTypology::Retail;
    }
    return BuildingTypology::Unknown;
}

BuildingTypology InferBuildingTypology(const std::string& buildingType,
                                       const BuildingStyle& style) {
    const BuildingTypology explicitTypology = ClassifyBuildingType(buildingType);
    if (explicitTypology != BuildingTypology::Unknown) {
        return explicitTypology;
    }

    if (style.category == "retail") {
        return BuildingTypology::Retail;
    }
    if (style.category == "residential") {
        return BuildingTypology::Residential;
    }
    if (style.category == "commercial") {
        return BuildingTypology::Office;
    }

    return BuildingTypology::Unknown;
}

bool IsResidentialTypology(BuildingTypology typology) {
    return typology == BuildingTypology::Residential;
}

bool IsOfficeTypology(BuildingTypology typology) {
    return typology == BuildingTypology::Office;
}

bool IsRetailTypology(BuildingTypology typology) {
    return typology == BuildingTypology::Retail;
}

bool IsVillaTypology(BuildingTypology typology) {
    return typology == BuildingTypology::Villa;
}

bool IsSupportedBuildingType(const std::string& buildingType) {
    return ClassifyBuildingType(buildingType) != BuildingTypology::Unknown;
}

float GetDefaultFootprintAspectRatio(BuildingTypology typology) {
    switch (typology) {
        case BuildingTypology::Villa:
            return 1.2f;
        case BuildingTypology::Residential:
            return 1.8f;
        case BuildingTypology::Office:
            return 2.2f;
        case BuildingTypology::Retail:
            return 3.0f;
        default:
            return 1.5f;
    }
}

float GetTypicalFloorHeight(BuildingTypology typology, int floorLevel) {
    switch (typology) {
        case BuildingTypology::Retail:
            return floorLevel == 0 ? kTypicalRetailGroundFloorHeight : kTypicalRetailFloorHeight;
        case BuildingTypology::Office:
            return floorLevel == 0 ? kTypicalOfficeGroundFloorHeight : kTypicalOfficeFloorHeight;
        case BuildingTypology::Residential:
            return floorLevel == 0 ? kTypicalResidentialGroundFloorHeight : kTypicalResidentialFloorHeight;
        default:
            return kTypicalFallbackFloorHeight;
    }
}

float GetReasonableMaxFloorHeight(BuildingTypology typology, int floorLevel) {
    switch (typology) {
        case BuildingTypology::Retail:
            return floorLevel == 0 ? 8.0f : 6.5f;
        case BuildingTypology::Office:
            return floorLevel <= 1 ? 7.5f : 5.5f;
        case BuildingTypology::Residential:
            return floorLevel == 0 ? 5.0f : 4.2f;
        default:
            return 4.5f;
    }
}

} // namespace Building
} // namespace Moon
