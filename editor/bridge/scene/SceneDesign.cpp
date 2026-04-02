#include "SceneDesign.h"

#include <cmath>
#include <unordered_set>

namespace Moon {
namespace Editor {
namespace SceneDesign {

namespace {

using json = nlohmann::json;

bool IsFiniteNumber(const json& value) {
    return value.is_number() && std::isfinite(value.get<double>());
}

bool EnsureVector3(const json& value, const char* fieldName, std::string& outError) {
    if (!value.is_array() || value.size() != 3) {
        outError = std::string(fieldName) + " must be an array of 3 numbers";
        return false;
    }

    for (size_t i = 0; i < 3; ++i) {
        if (!IsFiniteNumber(value[i])) {
            outError = std::string(fieldName) + " must contain only finite numbers";
            return false;
        }
    }

    return true;
}

bool NormalizeTransform(json& instance, std::string& outError) {
    if (!instance.contains("transform") || !instance["transform"].is_object()) {
        instance["transform"] = {
            {"position", json::array({0.0, 0.0, 0.0})},
            {"rotation_euler", json::array({0.0, 0.0, 0.0})},
            {"scale", json::array({1.0, 1.0, 1.0})}
        };
        return true;
    }

    json& transform = instance["transform"];
    if (!transform.contains("position")) {
        transform["position"] = json::array({0.0, 0.0, 0.0});
    }
    if (!transform.contains("rotation_euler")) {
        transform["rotation_euler"] = json::array({0.0, 0.0, 0.0});
    }
    if (!transform.contains("scale")) {
        transform["scale"] = json::array({1.0, 1.0, 1.0});
    }

    if (!EnsureVector3(transform["position"], "transform.position", outError) ||
        !EnsureVector3(transform["rotation_euler"], "transform.rotation_euler", outError) ||
        !EnsureVector3(transform["scale"], "transform.scale", outError)) {
        return false;
    }

    return true;
}

bool NormalizeBuildingInstance(json& instance, std::string& outError) {
    if (!instance.is_object()) {
        outError = "Each building instance must be an object";
        return false;
    }

    if (!instance.contains("instance_id") || !instance["instance_id"].is_string() ||
        instance["instance_id"].get<std::string>().empty()) {
        outError = "Building instance is missing a non-empty instance_id";
        return false;
    }

    if (!instance.contains("name") || !instance["name"].is_string()) {
        instance["name"] = instance["instance_id"].get<std::string>();
    }

    if (!instance.contains("building_json") || !instance["building_json"].is_string() ||
        instance["building_json"].get<std::string>().empty()) {
        outError = "Building instance '" + instance["instance_id"].get<std::string>() + "' is missing building_json";
        return false;
    }

    return NormalizeTransform(instance, outError);
}

bool NormalizeObjectInstance(json& instance, std::string& outError) {
    if (!instance.is_object()) {
        outError = "Each object instance must be an object";
        return false;
    }

    if (!instance.contains("instance_id") || !instance["instance_id"].is_string() ||
        instance["instance_id"].get<std::string>().empty()) {
        outError = "Object instance is missing a non-empty instance_id";
        return false;
    }

    if (!instance.contains("name") || !instance["name"].is_string()) {
        instance["name"] = instance["instance_id"].get<std::string>();
    }

    const bool hasAssetId = instance.contains("asset_id") && instance["asset_id"].is_string() &&
        !instance["asset_id"].get<std::string>().empty();
    const bool hasObjectJson = instance.contains("object_json") && instance["object_json"].is_string() &&
        !instance["object_json"].get<std::string>().empty();
    if (!hasAssetId && !hasObjectJson) {
        outError = "Object instance '" + instance["instance_id"].get<std::string>() +
            "' must include asset_id or object_json";
        return false;
    }

    if (!instance.contains("parameter_overrides") || !instance["parameter_overrides"].is_object()) {
        instance["parameter_overrides"] = json::object();
    }

    return NormalizeTransform(instance, outError);
}

bool NormalizeSceneJson(json& scene, std::string& outError) {
    if (!scene.is_object()) {
        outError = "Scene design root must be an object";
        return false;
    }

    scene["schema"] = "moon_scene_design";
    if (!scene.contains("version") || !scene["version"].is_number_integer()) {
        scene["version"] = 1;
    }
    if (!scene.contains("building_instances") || !scene["building_instances"].is_array()) {
        scene["building_instances"] = json::array();
    }
    if (!scene.contains("object_instances") || !scene["object_instances"].is_array()) {
        scene["object_instances"] = json::array();
    }

    std::unordered_set<std::string> knownIds;
    for (json& instance : scene["building_instances"]) {
        if (!NormalizeBuildingInstance(instance, outError)) {
            return false;
        }
        const std::string id = instance["instance_id"].get<std::string>();
        if (!knownIds.insert(id).second) {
            outError = "Duplicate instance_id: " + id;
            return false;
        }
    }

    for (json& instance : scene["object_instances"]) {
        if (!NormalizeObjectInstance(instance, outError)) {
            return false;
        }
        const std::string id = instance["instance_id"].get<std::string>();
        if (!knownIds.insert(id).second) {
            outError = "Duplicate instance_id: " + id;
            return false;
        }
    }

    return true;
}

json* FindInstanceById(json& scene, const std::string& instanceId) {
    for (json& instance : scene["building_instances"]) {
        if (instance.value("instance_id", std::string()) == instanceId) {
            return &instance;
        }
    }
    for (json& instance : scene["object_instances"]) {
        if (instance.value("instance_id", std::string()) == instanceId) {
            return &instance;
        }
    }
    return nullptr;
}

bool RemoveInstanceById(json& scene, const std::string& instanceId) {
    auto removeFrom = [&](json& array) {
        for (auto it = array.begin(); it != array.end(); ++it) {
            if (it->value("instance_id", std::string()) == instanceId) {
                array.erase(it);
                return true;
            }
        }
        return false;
    };

    return removeFrom(scene["building_instances"]) || removeFrom(scene["object_instances"]);
}

bool ApplyMoveInstance(json& scene, const json& operation, std::string& outError) {
    const std::string instanceId = operation.value("instance_id", std::string());
    json* instance = FindInstanceById(scene, instanceId);
    if (!instance) {
        outError = "move_instance could not find instance_id '" + instanceId + "'";
        return false;
    }

    json& position = (*instance)["transform"]["position"];
    if (operation.contains("position")) {
        json next = operation["position"];
        if (!EnsureVector3(next, "position", outError)) {
            return false;
        }
        position = std::move(next);
        return true;
    }

    if (operation.contains("delta")) {
        const json& delta = operation["delta"];
        if (!EnsureVector3(delta, "delta", outError)) {
            return false;
        }
        for (size_t i = 0; i < 3; ++i) {
            position[i] = position[i].get<double>() + delta[i].get<double>();
        }
        return true;
    }

    outError = "move_instance requires position or delta";
    return false;
}

bool ApplyReplaceInstanceAsset(json& scene, const json& operation, std::string& outError) {
    const std::string instanceId = operation.value("instance_id", std::string());
    json* instance = FindInstanceById(scene, instanceId);
    if (!instance) {
        outError = "replace_instance_asset could not find instance_id '" + instanceId + "'";
        return false;
    }

    if (instance->contains("building_json")) {
        if (!operation.contains("building_json") || !operation["building_json"].is_string()) {
            outError = "replace_instance_asset for a building requires building_json";
            return false;
        }
        (*instance)["building_json"] = operation["building_json"];
        if (operation.contains("name") && operation["name"].is_string()) {
            (*instance)["name"] = operation["name"];
        }
        return true;
    }

    const bool hasAssetId = operation.contains("asset_id") && operation["asset_id"].is_string();
    const bool hasObjectJson = operation.contains("object_json") && operation["object_json"].is_string();
    if (!hasAssetId && !hasObjectJson) {
        outError = "replace_instance_asset for an object requires asset_id or object_json";
        return false;
    }

    if (hasAssetId) {
        (*instance)["asset_id"] = operation["asset_id"];
        instance->erase("object_json");
    }
    if (hasObjectJson) {
        (*instance)["object_json"] = operation["object_json"];
        instance->erase("asset_id");
    }
    if (operation.contains("parameter_overrides") && operation["parameter_overrides"].is_object()) {
        (*instance)["parameter_overrides"] = operation["parameter_overrides"];
    }
    if (operation.contains("name") && operation["name"].is_string()) {
        (*instance)["name"] = operation["name"];
    }
    return NormalizeObjectInstance(*instance, outError);
}

} // namespace

bool ParseSceneDesign(const std::string& sceneJson,
                      json& outScene,
                      std::string& outError) {
    outError.clear();
    outScene = json::parse(sceneJson, nullptr, false);
    if (outScene.is_discarded()) {
        outError = "Scene design JSON is invalid";
        return false;
    }

    if (!NormalizeSceneJson(outScene, outError)) {
        return false;
    }

    return true;
}

bool ParseSceneEditOps(const std::string& opsJson,
                       json& outOps,
                       std::string& outError) {
    outError.clear();
    outOps = json::parse(opsJson, nullptr, false);
    if (outOps.is_discarded()) {
        outError = "Scene edit ops JSON is invalid";
        return false;
    }
    if (!outOps.is_object() || !outOps.contains("operations") || !outOps["operations"].is_array()) {
        outError = "Scene edit ops root must contain an operations array";
        return false;
    }
    return true;
}

bool NormalizeSceneDesign(const std::string& sceneJson,
                          std::string& outSceneJson,
                          std::string& outError) {
    json scene;
    if (!ParseSceneDesign(sceneJson, scene, outError)) {
        return false;
    }
    outSceneJson = scene.dump(2);
    return true;
}

bool ApplySceneOperations(const std::string& sceneJson,
                          const std::string& opsJson,
                          std::string& outSceneJson,
                          std::string& outError) {
    json scene;
    if (!ParseSceneDesign(sceneJson, scene, outError)) {
        return false;
    }

    json ops;
    if (!ParseSceneEditOps(opsJson, ops, outError)) {
        return false;
    }

    for (const json& operation : ops["operations"]) {
        if (!operation.is_object()) {
            outError = "Each scene operation must be an object";
            return false;
        }

        const std::string op = operation.value("op", std::string());
        if (op == "place_building") {
            if (!operation.contains("instance")) {
                outError = "place_building requires instance";
                return false;
            }
            json instance = operation["instance"];
            if (!NormalizeBuildingInstance(instance, outError)) {
                return false;
            }
            if (FindInstanceById(scene, instance["instance_id"].get<std::string>())) {
                outError = "place_building found duplicate instance_id '" +
                    instance["instance_id"].get<std::string>() + "'";
                return false;
            }
            scene["building_instances"].push_back(std::move(instance));
        } else if (op == "place_object") {
            if (!operation.contains("instance")) {
                outError = "place_object requires instance";
                return false;
            }
            json instance = operation["instance"];
            if (!NormalizeObjectInstance(instance, outError)) {
                return false;
            }
            if (FindInstanceById(scene, instance["instance_id"].get<std::string>())) {
                outError = "place_object found duplicate instance_id '" +
                    instance["instance_id"].get<std::string>() + "'";
                return false;
            }
            scene["object_instances"].push_back(std::move(instance));
        } else if (op == "move_instance") {
            if (!ApplyMoveInstance(scene, operation, outError)) {
                return false;
            }
        } else if (op == "remove_instance") {
            const std::string instanceId = operation.value("instance_id", std::string());
            if (instanceId.empty()) {
                outError = "remove_instance requires instance_id";
                return false;
            }
            if (!RemoveInstanceById(scene, instanceId)) {
                outError = "remove_instance could not find instance_id '" + instanceId + "'";
                return false;
            }
        } else if (op == "replace_instance_asset") {
            if (!ApplyReplaceInstanceAsset(scene, operation, outError)) {
                return false;
            }
        } else {
            outError = "Unsupported scene op: " + op;
            return false;
        }
    }

    outSceneJson = scene.dump(2);
    return true;
}

} // namespace SceneDesign
} // namespace Editor
} // namespace Moon
