#pragma once

#include "BuildingTypes.h"

#include <string>
#include <vector>

namespace Moon {
namespace Building {

struct BuildingQualityIssue {
    std::string code;
    std::string message;
    int floorLevel = -1;
};

struct BuildingQualityReport {
    bool passed = true;
    std::vector<BuildingQualityIssue> errors;
    std::vector<BuildingQualityIssue> warnings;

    bool HasErrorCode(const std::string& code) const;
};

BuildingQualityReport EvaluateBuildingQuality(const GeneratedBuilding& building);

} // namespace Building
} // namespace Moon
