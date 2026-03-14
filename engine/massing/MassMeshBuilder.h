#pragma once

#include "MassRules.h"
#include "../core/Mesh/Mesh.h"
#include <memory>
#include <string>
#include <vector>

namespace Moon {
namespace Massing {

struct MassBuildItem {
    std::string name;
    std::string material;
    std::shared_ptr<Mesh> mesh;
};

struct MassBuildResult {
    std::vector<MassBuildItem> items;
    std::vector<std::string> warnings;
};

class MassMeshBuilder {
public:
    static bool Build(const RuleSet& ruleSet, MassBuildResult& outResult, std::string& outError);
};

} // namespace Massing
} // namespace Moon
