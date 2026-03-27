#include "ObjectCatalog.h"
#include "../Logging/Logger.h"
#include "../../../external/nlohmann/json.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace Moon {
namespace Object {

namespace {

std::vector<std::string> ParseStringArray(const json& value) {
    std::vector<std::string> result;
    if (!value.is_array()) {
        return result;
    }

    for (const auto& item : value) {
        if (item.is_string()) {
            result.push_back(item.get<std::string>());
        }
    }

    return result;
}

ObjectGraphAssetRef ParseGraphAssetRef(const json& objectJson) {
    ObjectGraphAssetRef ref;

    if (objectJson.contains("graph_asset") && objectJson["graph_asset"].is_string()) {
        ref.graphAssetId = objectJson["graph_asset"].get<std::string>();
        return ref;
    }

    // Backward compatibility for older catalog schemas.
    if (objectJson.contains("generator") && objectJson["generator"].is_object()) {
        const json& generatorJson = objectJson["generator"];
        if (generatorJson.contains("asset") && generatorJson["asset"].is_string()) {
            ref.graphAssetId = generatorJson["asset"].get<std::string>();
            return ref;
        }
    }

    if (objectJson.contains("blueprint") && objectJson["blueprint"].is_string()) {
        ref.graphAssetId = objectJson["blueprint"].get<std::string>();
    }

    return ref;
}

void AppendUniqueStrings(std::vector<std::string>& target, const std::vector<std::string>& source) {
    for (const auto& item : source) {
        bool exists = false;
        for (const auto& existing : target) {
            if (existing == item) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            target.push_back(item);
        }
    }
}

void MergeVariants(std::vector<ObjectVariant>& target, const std::vector<ObjectVariant>& source) {
    for (const auto& variant : source) {
        auto it = std::find_if(target.begin(), target.end(),
            [&](const ObjectVariant& existing) { return existing.id == variant.id; });
        if (it == target.end()) {
            target.push_back(variant);
        }
    }
}

} // namespace

bool ObjectCatalog::LoadFromFile(const std::string& filepath, std::string& outError) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        outError = "Failed to open object catalog: " + filepath;
        MOON_LOG_ERROR("ObjectCatalog", "%s", outError.c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    json root;
    try {
        root = json::parse(buffer.str());
    } catch (const json::exception& e) {
        outError = std::string("Object catalog JSON parse error: ") + e.what();
        MOON_LOG_ERROR("ObjectCatalog", "%s", outError.c_str());
        return false;
    }

    if (!root.contains("objects") || !root["objects"].is_array()) {
        outError = "Object catalog missing 'objects' array";
        MOON_LOG_ERROR("ObjectCatalog", "%s", outError.c_str());
        return false;
    }

    Clear();

    for (const auto& objectJson : root["objects"]) {
        ObjectDefinition definition;
        if (!ParseDefinition(&objectJson, definition, outError)) {
            Clear();
            return false;
        }

        if (m_definitions.find(definition.id) != m_definitions.end()) {
            outError = "Duplicate object definition id: " + definition.id;
            MOON_LOG_ERROR("ObjectCatalog", "%s", outError.c_str());
            Clear();
            return false;
        }

        m_definitions.emplace(definition.id, std::move(definition));
    }

    if (!ResolveInheritance(outError)) {
        Clear();
        return false;
    }

    MOON_LOG_INFO("ObjectCatalog", "Loaded %d object definitions from %s",
        static_cast<int>(m_definitions.size()), filepath.c_str());

    return true;
}

void ObjectCatalog::Clear() {
    m_definitions.clear();
}

bool ObjectCatalog::HasDefinition(const std::string& objectId) const {
    return m_definitions.find(objectId) != m_definitions.end();
}

const ObjectDefinition* ObjectCatalog::GetDefinition(const std::string& objectId) const {
    auto it = m_definitions.find(objectId);
    return (it != m_definitions.end()) ? &it->second : nullptr;
}

std::vector<const ObjectDefinition*> ObjectCatalog::FindByCategory(const std::string& category) const {
    std::vector<const ObjectDefinition*> result;
    for (const auto& entry : m_definitions) {
        if (entry.second.category == category) {
            result.push_back(&entry.second);
        }
    }
    return result;
}

std::vector<const ObjectDefinition*> ObjectCatalog::FindByTag(const std::string& tag) const {
    std::vector<const ObjectDefinition*> result;
    for (const auto& entry : m_definitions) {
        for (const auto& existingTag : entry.second.tags) {
            if (existingTag == tag) {
                result.push_back(&entry.second);
                break;
            }
        }
    }
    return result;
}

bool ObjectCatalog::ParseDefinition(const void* jsonValue, ObjectDefinition& outDefinition, std::string& outError) {
    const json& objectJson = *static_cast<const json*>(jsonValue);

    if (!objectJson.contains("id") || !objectJson["id"].is_string()) {
        outError = "Object definition missing string 'id'";
        return false;
    }
    outDefinition.id = objectJson["id"].get<std::string>();

    if (objectJson.contains("base_object") && objectJson["base_object"].is_string()) {
        outDefinition.baseObjectId = objectJson["base_object"].get<std::string>();
    }

    if (objectJson.contains("display_name") && objectJson["display_name"].is_string()) {
        outDefinition.displayName = objectJson["display_name"].get<std::string>();
    } else {
        outDefinition.displayName = outDefinition.id;
    }

    if (objectJson.contains("description") && objectJson["description"].is_string()) {
        outDefinition.description = objectJson["description"].get<std::string>();
    }

    if (objectJson.contains("category") && objectJson["category"].is_string()) {
        outDefinition.category = objectJson["category"].get<std::string>();
    }

    outDefinition.graphAsset = ParseGraphAssetRef(objectJson);

    if (objectJson.contains("tags")) {
        outDefinition.tags = ParseStringArray(objectJson["tags"]);
    }

    if (objectJson.contains("defaults") && objectJson["defaults"].is_object()) {
        for (auto it = objectJson["defaults"].begin(); it != objectJson["defaults"].end(); ++it) {
            if (it.value().is_number()) {
                outDefinition.defaultParameters[it.key()] = it.value().get<float>();
            }
        }
    }

    if (objectJson.contains("parameters") && objectJson["parameters"].is_array()) {
        for (const auto& paramJson : objectJson["parameters"]) {
            if (!paramJson.is_object() || !paramJson.contains("name") || !paramJson["name"].is_string()) {
                outError = "Object definition '" + outDefinition.id + "' has invalid parameter entry";
                return false;
            }

            ObjectParameterSpec spec;
            spec.name = paramJson["name"].get<std::string>();

            auto defaultIt = outDefinition.defaultParameters.find(spec.name);
            if (defaultIt != outDefinition.defaultParameters.end()) {
                spec.defaultValue = defaultIt->second;
            }

            if (paramJson.contains("default") && paramJson["default"].is_number()) {
                spec.defaultValue = paramJson["default"].get<float>();
            }

            if (paramJson.contains("allow_overrides") && paramJson["allow_overrides"].is_boolean()) {
                spec.allowOverrides = paramJson["allow_overrides"].get<bool>();
            }

            if (paramJson.contains("random_range") && paramJson["random_range"].is_object()) {
                const json& rangeJson = paramJson["random_range"];
                if (rangeJson.contains("min") && rangeJson.contains("max") &&
                    rangeJson["min"].is_number() && rangeJson["max"].is_number()) {
                    spec.randomRange.enabled = true;
                    spec.randomRange.minValue = rangeJson["min"].get<float>();
                    spec.randomRange.maxValue = rangeJson["max"].get<float>();
                }
            }

            outDefinition.parameters[spec.name] = spec;
            outDefinition.defaultParameters[spec.name] = spec.defaultValue;
        }
    }

    if (objectJson.contains("variants") && objectJson["variants"].is_array()) {
        for (const auto& variantJson : objectJson["variants"]) {
            if (!variantJson.is_object() || !variantJson.contains("id") || !variantJson["id"].is_string()) {
                outError = "Object definition '" + outDefinition.id + "' has invalid variant entry";
                return false;
            }

            ObjectVariant variant;
            variant.id = variantJson["id"].get<std::string>();

            variant.graphAsset = ParseGraphAssetRef(variantJson);

            if (variantJson.contains("description") && variantJson["description"].is_string()) {
                variant.description = variantJson["description"].get<std::string>();
            }

            if (variantJson.contains("tags")) {
                variant.tags = ParseStringArray(variantJson["tags"]);
            }

            if (variantJson.contains("params") && variantJson["params"].is_object()) {
                for (auto it = variantJson["params"].begin(); it != variantJson["params"].end(); ++it) {
                    if (it.value().is_number()) {
                        variant.parameterValues[it.key()] = it.value().get<float>();
                    }
                }
            }

            outDefinition.variants.push_back(std::move(variant));
        }
    }

    return ValidateDefinition(outDefinition, outError);
}

bool ObjectCatalog::ValidateDefinition(const ObjectDefinition& definition, std::string& outError) const {
    if (definition.id.empty()) {
        outError = "Object definition id is empty";
        return false;
    }

    bool hasBaseGraphAsset = !definition.graphAsset.graphAssetId.empty();
    if (!hasBaseGraphAsset && definition.variants.empty()) {
        outError = "Object definition '" + definition.id + "' must define a base graph asset or at least one variant";
        return false;
    }

    for (const auto& variant : definition.variants) {
        if (variant.id.empty()) {
            outError = "Object definition '" + definition.id + "' has variant with empty id";
            return false;
        }
    }

    return true;
}

bool ObjectCatalog::ResolveInheritance(std::string& outError) {
    std::unordered_map<std::string, int> visitState;
    for (const auto& entry : m_definitions) {
        visitState[entry.first] = 0;
    }

    for (const auto& entry : m_definitions) {
        if (!ResolveDefinitionInheritance(entry.first, visitState, outError)) {
            return false;
        }
    }

    return true;
}

bool ObjectCatalog::ResolveDefinitionInheritance(const std::string& objectId,
                                                 std::unordered_map<std::string, int>& visitState,
                                                 std::string& outError) {
    auto definitionIt = m_definitions.find(objectId);
    if (definitionIt == m_definitions.end()) {
        outError = "Unknown object definition during inheritance resolution: " + objectId;
        return false;
    }

    int& state = visitState[objectId];
    if (state == 2) {
        return true;
    }
    if (state == 1) {
        outError = "Cyclic object inheritance detected at: " + objectId;
        return false;
    }

    state = 1;
    ObjectDefinition& definition = definitionIt->second;

    if (!definition.baseObjectId.empty()) {
        auto baseIt = m_definitions.find(definition.baseObjectId);
        if (baseIt == m_definitions.end()) {
            outError = "Base object not found for '" + definition.id + "': " + definition.baseObjectId;
            return false;
        }

        if (!ResolveDefinitionInheritance(definition.baseObjectId, visitState, outError)) {
            return false;
        }

        const ObjectDefinition& base = baseIt->second;

        if (definition.displayName.empty() || definition.displayName == definition.id) {
            definition.displayName = base.displayName;
        }
        if (definition.description.empty()) {
            definition.description = base.description;
        }
        if (definition.category.empty()) {
            definition.category = base.category;
        }
        if (definition.graphAsset.graphAssetId.empty()) {
            definition.graphAsset = base.graphAsset;
        }

        AppendUniqueStrings(definition.tags, base.tags);

        for (const auto& entry : base.parameters) {
            if (definition.parameters.find(entry.first) == definition.parameters.end()) {
                definition.parameters.emplace(entry.first, entry.second);
            }
        }

        for (const auto& entry : base.defaultParameters) {
            if (definition.defaultParameters.find(entry.first) == definition.defaultParameters.end()) {
                definition.defaultParameters.emplace(entry.first, entry.second);
            }
        }

        MergeVariants(definition.variants, base.variants);
    }

    if (!ValidateDefinition(definition, outError)) {
        return false;
    }

    state = 2;
    return true;
}

} // namespace Object
} // namespace Moon
