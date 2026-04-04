#pragma once

#include "BuildingQualityChecks.h"

namespace Moon {
namespace Building {

void AppendElementRuleViolations(const GeneratedBuilding& building,
                                 BuildingQualityReport& report);

} // namespace Building
} // namespace Moon
