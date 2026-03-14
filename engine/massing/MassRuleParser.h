#pragma once

#include "MassRules.h"
#include <string>

namespace Moon {
namespace Massing {

class MassRuleParser {
public:
    static bool ParseFromString(const std::string& jsonString, RuleSet& outRuleSet, std::string& outError);
    static json ToJson(const RuleSet& ruleSet);
    static std::string ToJsonString(const RuleSet& ruleSet, int indent = 2);

private:
    static bool ParseRuleSetJson(const json& document, RuleSet& outRuleSet, std::string& outError);
    static bool ParseNode(const json& nodeJson, RuleNode& outNode, std::string& outError);
    static bool ParseTransform(const json& transformJson, RuleTransform& outTransform, std::string& outError);
    static bool ParseCurve2D(const json& curveJson, Curve2D& outCurve, std::string& outError);
    static bool ParseCurve3D(const json& curveJson, Curve3D& outCurve, std::string& outError);
    static json SerializeNode(const RuleNode& node);
    static json SerializeTransform(const RuleTransform& transform);
};

} // namespace Massing
} // namespace Moon
