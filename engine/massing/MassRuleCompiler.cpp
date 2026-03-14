#include "MassRuleCompiler.h"

namespace Moon {
namespace Massing {

namespace {

float MetersToCentimeters(float meters) {
    return meters * 100.0f;
}

json Pos3(float x, float y, float z) {
    return json::array({x, y, z});
}

json Rotation3(float x, float y, float z) {
    return json::array({x, y, z});
}

json Scale3(float x, float y, float z) {
    return json::array({x, y, z});
}

json CompileTransform(const RuleTransform& transform) {
    json result;
    result["position"] = Pos3(
        MetersToCentimeters(transform.position[0]),
        MetersToCentimeters(transform.position[1]),
        MetersToCentimeters(transform.position[2]));
    result["rotation"] = Rotation3(transform.rotation[0], transform.rotation[1], transform.rotation[2]);
    result["scale"] = Scale3(transform.scale[0], transform.scale[1], transform.scale[2]);
    return result;
}

json ApplyMeterParams(const json& params) {
    json result = params;

    auto convertIfExists = [&](const char* key) {
        auto it = result.find(key);
        if (it != result.end() && it->is_number()) {
            *it = MetersToCentimeters(it->get<float>());
        }
    };

    convertIfExists("size_x");
    convertIfExists("size_y");
    convertIfExists("size_z");
    convertIfExists("radius");
    convertIfExists("radius_inner");
    convertIfExists("radius_outer");
    convertIfExists("height");
    convertIfExists("depth");
    convertIfExists("width");
    convertIfExists("thickness");

    return result;
}

json MakePlaceholderNode(const RuleNode& node, const char* reason) {
    json placeholder;
    placeholder["name"] = node.name.empty() ? node.id : node.name;
    placeholder["type"] = "primitive";
    placeholder["primitive"] = "cube";

    json params;
    params["size_x"] = MetersToCentimeters(node.params.value("fallback_size_x", 8.0f));
    params["size_y"] = MetersToCentimeters(node.params.value("fallback_size_y", 8.0f));
    params["size_z"] = MetersToCentimeters(node.params.value("fallback_size_z", 8.0f));
    placeholder["params"] = params;
    placeholder["transform"] = CompileTransform(node.transform);
    placeholder["material"] = node.material.empty() ? "concrete" : node.material;
    placeholder["comment"] = reason;
    return placeholder;
}

json OffsetArrayTransform(const json& sourceTransform, float offsetX, float offsetY, float offsetZ) {
    json transform = sourceTransform;
    transform["position"][0] = transform["position"][0].get<float>() + MetersToCentimeters(offsetX);
    transform["position"][1] = transform["position"][1].get<float>() + MetersToCentimeters(offsetY);
    transform["position"][2] = transform["position"][2].get<float>() + MetersToCentimeters(offsetZ);
    return transform;
}

} // namespace

bool MassRuleCompiler::CompileToBlueprint(const RuleSet& ruleSet, std::string& outBlueprintJson, std::string& outError) {
    json blueprint;
    blueprint["schema_version"] = 1;
    blueprint["version"] = ruleSet.version;
    blueprint["name"] = ruleSet.name.empty() ? "massing_rule" : ruleSet.name;
    blueprint["description"] = ruleSet.description.empty()
        ? "Generated from massing rules"
        : ruleSet.description;

    json rootNode;
    if (!CompileNode(ruleSet.root, rootNode, outError)) {
        return false;
    }

    blueprint["root"] = std::move(rootNode);
    outBlueprintJson = blueprint.dump(2);
    return true;
}

bool MassRuleCompiler::CompileNode(const RuleNode& node, json& outNode, std::string& outError) {
    const std::string nodeName = !node.name.empty() ? node.name : node.id;

    switch (node.type) {
        case RuleNodeType::Primitive: {
            outNode["name"] = nodeName;
            outNode["type"] = "primitive";
            outNode["primitive"] = ToString(node.primitive);
            outNode["params"] = ApplyMeterParams(node.params);
            outNode["transform"] = CompileTransform(node.transform);
            if (!node.material.empty()) {
                outNode["material"] = node.material;
            }
            return true;
        }

        case RuleNodeType::Group: {
            outNode["name"] = nodeName;
            outNode["type"] = "group";
            outNode["transform"] = CompileTransform(node.transform);
            outNode["children"] = json::array();
            for (const RuleNode& child : node.children) {
                json compiledChild;
                if (!CompileNode(child, compiledChild, outError)) {
                    return false;
                }
                outNode["children"].push_back(std::move(compiledChild));
            }
            return true;
        }

        case RuleNodeType::Csg: {
            if (node.children.size() != 2) {
                outError = "CSG node must have exactly 2 children";
                return false;
            }
            outNode["name"] = nodeName;
            outNode["type"] = "csg";
            outNode["operation"] = ToString(node.csgOperation);
            if (!CompileNode(node.children[0], outNode["left"], outError)) {
                return false;
            }
            if (!CompileNode(node.children[1], outNode["right"], outError)) {
                return false;
            }
            return true;
        }

        case RuleNodeType::Array: {
            if (node.children.size() != 1) {
                outError = "Array node must have exactly 1 source child";
                return false;
            }

            const int countX = node.params.value("count_x", 1);
            const int countY = node.params.value("count_y", 1);
            const int countZ = node.params.value("count_z", 1);
            const float spacingX = node.params.value("spacing_x", 0.0f);
            const float spacingY = node.params.value("spacing_y", 0.0f);
            const float spacingZ = node.params.value("spacing_z", 0.0f);

            json compiledSource;
            if (!CompileNode(node.children[0], compiledSource, outError)) {
                return false;
            }

            outNode["name"] = nodeName;
            outNode["type"] = "group";
            outNode["transform"] = CompileTransform(node.transform);
            outNode["children"] = json::array();

            for (int ix = 0; ix < countX; ++ix) {
                for (int iy = 0; iy < countY; ++iy) {
                    for (int iz = 0; iz < countZ; ++iz) {
                        json instance = compiledSource;
                        instance["name"] = nodeName + "_" + std::to_string(ix) + "_" +
                                           std::to_string(iy) + "_" + std::to_string(iz);

                        if (!instance.contains("transform")) {
                            instance["transform"] = CompileTransform(RuleTransform());
                        }
                        instance["transform"] = OffsetArrayTransform(
                            instance["transform"],
                            spacingX * static_cast<float>(ix),
                            spacingY * static_cast<float>(iy),
                            spacingZ * static_cast<float>(iz));
                        outNode["children"].push_back(std::move(instance));
                    }
                }
            }
            return true;
        }

        case RuleNodeType::Extrude:
            outNode = MakePlaceholderNode(node, "Extrude rule is schema-ready but not compiled yet");
            return true;
        case RuleNodeType::Revolve:
            outNode = MakePlaceholderNode(node, "Revolve rule is schema-ready but not compiled yet");
            return true;
        case RuleNodeType::Sweep:
            outNode = MakePlaceholderNode(node, "Sweep rule is schema-ready but not compiled yet");
            return true;
        case RuleNodeType::Loft:
            outNode = MakePlaceholderNode(node, "Loft rule is schema-ready but not compiled yet");
            return true;
        case RuleNodeType::Deform:
            outNode = MakePlaceholderNode(node, "Deform rule is schema-ready but not compiled yet");
            return true;
        default:
            outError = "Unsupported node type in compiler";
            return false;
    }
}

} // namespace Massing
} // namespace Moon
