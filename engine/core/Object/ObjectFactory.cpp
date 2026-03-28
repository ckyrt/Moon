#include "ObjectFactory.h"
#include "../Logging/Logger.h"

namespace Moon {
namespace Object {

void ObjectFactory::SetBlueprintDatabase(BlueprintDatabase* database) {
    m_blueprintDatabase = database;
}

GeneratedObject ObjectFactory::BuildObject(const ObjectBuildRequest& request, std::string& outError) const {
    GeneratedObject result;
    outError.clear();

    if (!m_blueprintDatabase) {
        outError = "ObjectFactory requires a BlueprintDatabase";
        return result;
    }

    const Blueprint* blueprint = m_blueprintDatabase->GetBlueprint(request.objectId);
    if (!blueprint) {
        outError = "Object asset not found in index: " + request.objectId;
        return result;
    }

    std::unordered_map<std::string, float> resolvedParameters;
    for (const auto& entry : blueprint->GetParameters()) {
        resolvedParameters[entry.first] = entry.second.defaultValue;
    }

    for (const auto& entry : request.parameterOverrides) {
        if (!blueprint->HasParameter(entry.first)) {
            outError = "Unknown parameter override for object asset '" + request.objectId + "': " + entry.first;
            return result;
        }
        resolvedParameters[entry.first] = entry.second;
    }

    CSG::CSGBuilder builder;
    builder.SetBlueprintDatabase(m_blueprintDatabase);

    CSG::BuildResult buildResult = builder.Build(blueprint, resolvedParameters, outError);
    if (!outError.empty()) {
        return result;
    }

    result.objectId = request.objectId;
    result.graphAsset.graphAssetId = request.objectId;
    result.resolvedParameters = std::move(resolvedParameters);
    result.buildResult = std::move(buildResult);

    MOON_LOG_INFO("ObjectFactory", "Built object asset '%s' directly from index registration",
        result.objectId.c_str());

    return result;
}

} // namespace Object
} // namespace Moon
