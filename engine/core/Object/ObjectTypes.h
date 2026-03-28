#pragma once

#include "../CSG/CSGBuilder.h"
#include <string>
#include <unordered_map>

namespace Moon {
namespace Object {

struct ObjectGraphAssetRef {
    std::string graphAssetId;
};

struct ObjectBuildRequest {
    std::string objectId;
    std::unordered_map<std::string, float> parameterOverrides;
};

struct GeneratedObject {
    std::string objectId;
    ObjectGraphAssetRef graphAsset;
    std::unordered_map<std::string, float> resolvedParameters;
    CSG::BuildResult buildResult;
};

} // namespace Object
} // namespace Moon
