#pragma once

#include "MassRules.h"
#include "../../external/nlohmann/json.hpp"

#include <string>

namespace Moon {
namespace Massing {

struct BuildingMassingIntent {
    std::string archetype = "podium_tower";
    std::string buildingType = "mixed_use_tower";
    std::string style = "modern";
    float width = 36.0f;
    float depth = 28.0f;
    int floors = 32;
    float floorHeight = 4.0f;
    int podiumFloors = 5;
    float towerCoverage = 0.62f;
    float setback = 2.0f;
    float taper = 0.10f;
    float twistDegrees = 8.0f;
    bool crown = true;
    bool fins = true;
    float cornerCut = 2.0f;
};

class BuildingMassingPlanner {
public:
    static bool ParseIntent(const nlohmann::json& json,
                            BuildingMassingIntent& outIntent,
                            std::string& outError);

    static bool ParseIntentFromString(const std::string& jsonString,
                                      BuildingMassingIntent& outIntent,
                                      std::string& outError);

    static bool Plan(const BuildingMassingIntent& intent,
                     RuleSet& outRuleSet,
                     std::string& outError);

    static bool PlanToJsonString(const BuildingMassingIntent& intent,
                                 std::string& outRuleJson,
                                 std::string& outError,
                                 int indent = 2);
};

} // namespace Massing
} // namespace Moon
