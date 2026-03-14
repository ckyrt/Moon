#pragma once

#include "MassRules.h"
#include <string>

namespace Moon {
namespace Massing {

class MassRuleCompiler {
public:
    static bool CompileToBlueprint(const RuleSet& ruleSet, std::string& outBlueprintJson, std::string& outError);

private:
    static bool CompileNode(const RuleNode& node, json& outNode, std::string& outError);
};

} // namespace Massing
} // namespace Moon
