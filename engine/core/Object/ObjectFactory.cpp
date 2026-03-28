#include "ObjectFactory.h"
#include "../Logging/Logger.h"
#include <random>

namespace Moon {
namespace Object {

void ObjectFactory::SetBlueprintDatabase(BlueprintDatabase* database) {
    m_blueprintDatabase = database;
}

void ObjectFactory::SetCatalog(const ObjectCatalog* catalog) {
    m_catalog = catalog;
}

GeneratedObject ObjectFactory::BuildObject(const ObjectBuildRequest& request, std::string& outError) const {
    GeneratedObject result;
    outError.clear();

    if (!m_catalog) {
        outError = "ObjectFactory requires an ObjectCatalog";
        return result;
    }

    if (!m_blueprintDatabase) {
        outError = "ObjectFactory requires a BlueprintDatabase";
        return result;
    }

    const ObjectDefinition* definition = m_catalog->GetDefinition(request.objectId);
    if (!definition) {
        outError = "Unknown object definition: " + request.objectId;
        return result;
    }

    const ObjectVariant* variant = FindVariant(*definition, request.variantId);
    if (!request.variantId.empty() && !variant) {
        outError = "Unknown object variant '" + request.variantId + "' for object '" + request.objectId + "'";
        return result;
    }

    std::unordered_map<std::string, float> resolvedParameters = definition->defaultParameters;

    if (variant) {
        for (const auto& entry : variant->parameterValues) {
            resolvedParameters[entry.first] = entry.second;
        }
    }

    ApplyRandomizedDefaults(*definition, request, resolvedParameters);

    for (const auto& entry : request.parameterOverrides) {
        auto specIt = definition->parameters.find(entry.first);
        if (specIt != definition->parameters.end() && !specIt->second.allowOverrides) {
            outError = "Parameter override is not allowed: " + entry.first;
            return result;
        }
        resolvedParameters[entry.first] = entry.second;
    }

    ObjectGraphAssetRef graphAsset = definition->graphAsset;
    if (variant && !variant->graphAsset.graphAssetId.empty()) {
        graphAsset = variant->graphAsset;
    }

    if (graphAsset.graphAssetId.empty()) {
        outError = "Object '" + request.objectId + "' resolved to an empty graph asset";
        return result;
    }

    const Blueprint* blueprint = m_blueprintDatabase->GetBlueprint(graphAsset.graphAssetId);
    if (!blueprint) {
        outError = "Object graph asset not found: " + graphAsset.graphAssetId;
        return result;
    }

    CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(m_blueprintDatabase);

    CSG::BuildResult buildResult = builder.Build(blueprint, resolvedParameters, outError);
    if (!outError.empty()) {
        return result;
    }

    result.objectId = definition->id;
    result.variantId = variant ? variant->id : "";
    result.graphAsset = graphAsset;
    result.resolvedParameters = std::move(resolvedParameters);
    result.buildResult = std::move(buildResult);

    MOON_LOG_INFO("ObjectFactory", "Built object '%s' using graph asset '%s'",
        result.objectId.c_str(), result.graphAsset.graphAssetId.c_str());

    return result;
}

const ObjectVariant* ObjectFactory::FindVariant(const ObjectDefinition& definition, const std::string& variantId) const {
    if (variantId.empty()) {
        return nullptr;
    }

    for (const auto& variant : definition.variants) {
        if (variant.id == variantId) {
            return &variant;
        }
    }

    return nullptr;
}

uint32_t ObjectFactory::MakeSeed(const ObjectBuildRequest& request) const {
    if (request.seed != 0) {
        return request.seed;
    }

    std::hash<std::string> hasher;
    const size_t combined = hasher(request.objectId + "::" + request.variantId);
    return static_cast<uint32_t>(combined & 0xffffffffu);
}

void ObjectFactory::ApplyRandomizedDefaults(const ObjectDefinition& definition,
                                            const ObjectBuildRequest& request,
                                            std::unordered_map<std::string, float>& inOutParameters) const {
    if (!request.randomizeMissingParameters) {
        return;
    }

    std::mt19937 rng(MakeSeed(request));

    for (const auto& entry : definition.parameters) {
        const ObjectParameterSpec& spec = entry.second;
        if (!spec.randomRange.enabled) {
            continue;
        }

        if (request.parameterOverrides.find(spec.name) != request.parameterOverrides.end()) {
            continue;
        }

        std::uniform_real_distribution<float> distribution(spec.randomRange.minValue, spec.randomRange.maxValue);
        inOutParameters[spec.name] = distribution(rng);
    }
}

} // namespace Object
} // namespace Moon
