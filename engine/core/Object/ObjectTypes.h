#pragma once

#include "../CSG/CSGBuilder.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Moon {
namespace Object {

struct ObjectGraphAssetRef {
    std::string graphAssetId;
};

struct ObjectParameterRange {
    bool enabled = false;
    float minValue = 0.0f;
    float maxValue = 0.0f;
};

struct ObjectParameterSpec {
    std::string name;
    float defaultValue = 0.0f;
    bool allowOverrides = true;
    ObjectParameterRange randomRange;
};

struct ObjectVariant {
    std::string id;
    ObjectGraphAssetRef graphAsset;
    std::string description;
    std::vector<std::string> tags;
    std::unordered_map<std::string, float> parameterValues;
};

struct ObjectDefinition {
    std::string id;
    std::string baseObjectId;
    std::string displayName;
    std::string description;
    std::string category;
    ObjectGraphAssetRef graphAsset;
    std::vector<std::string> tags;
    std::unordered_map<std::string, ObjectParameterSpec> parameters;
    std::unordered_map<std::string, float> defaultParameters;
    std::vector<ObjectVariant> variants;
};

struct ObjectBuildRequest {
    std::string objectId;
    std::string variantId;
    std::unordered_map<std::string, float> parameterOverrides;
    uint32_t seed = 0;
    bool randomizeMissingParameters = false;
};

struct GeneratedObject {
    std::string objectId;
    std::string variantId;
    ObjectGraphAssetRef graphAsset;
    std::unordered_map<std::string, float> resolvedParameters;
    CSG::BuildResult buildResult;
};

} // namespace Object
} // namespace Moon
