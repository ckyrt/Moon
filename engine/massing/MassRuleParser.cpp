#include "MassRuleParser.h"
#include <unordered_set>

namespace Moon {
namespace Massing {

namespace {

bool TryReadVec2(const json& value, Vec2& outVec) {
    if (value.is_array() && value.size() == 2) {
        outVec[0] = value[0].get<float>();
        outVec[1] = value[1].get<float>();
        return true;
    }

    if (value.is_object() && value.contains("x") && value.contains("y")) {
        outVec[0] = value["x"].get<float>();
        outVec[1] = value["y"].get<float>();
        return true;
    }

    return false;
}

bool TryReadVec3(const json& value, Vec3& outVec) {
    if (value.is_array() && value.size() == 3) {
        outVec[0] = value[0].get<float>();
        outVec[1] = value[1].get<float>();
        outVec[2] = value[2].get<float>();
        return true;
    }

    if (value.is_object() && value.contains("x") && value.contains("y") && value.contains("z")) {
        outVec[0] = value["x"].get<float>();
        outVec[1] = value["y"].get<float>();
        outVec[2] = value["z"].get<float>();
        return true;
    }

    return false;
}

json VecToJson(const Vec2& value) {
    return json::array({value[0], value[1]});
}

json VecToJson(const Vec3& value) {
    return json::array({value[0], value[1], value[2]});
}

void MergeInlineParams(const json& nodeJson, json& ioParams) {
    static const std::unordered_set<std::string> kReservedKeys = {
        "id", "name", "type", "primitive", "operation", "material",
        "transform", "editor", "params",
        "profile", "profiles", "path", "paths",
        "child", "children"
    };

    for (auto it = nodeJson.begin(); it != nodeJson.end(); ++it) {
        if (kReservedKeys.find(it.key()) != kReservedKeys.end()) {
            continue;
        }
        if (!ioParams.contains(it.key())) {
            ioParams[it.key()] = it.value();
        }
    }
}

} // namespace

bool MassRuleParser::ParseFromString(const std::string& jsonString, RuleSet& outRuleSet, std::string& outError) {
    try {
        const json document = json::parse(jsonString);
        return ParseRuleSetJson(document, outRuleSet, outError);
    } catch (const json::exception& e) {
        outError = std::string("Failed to parse massing rule JSON: ") + e.what();
        return false;
    }
}

json MassRuleParser::ToJson(const RuleSet& ruleSet) {
    json document;
    document["schema_version"] = ruleSet.schemaVersion;
    document["version"] = ruleSet.version;
    document["name"] = ruleSet.name;
    document["description"] = ruleSet.description;
    document["unit"] = ruleSet.unit;
    if (!ruleSet.ai.empty()) {
        document["ai"] = ruleSet.ai;
    }
    if (!ruleSet.editor.empty()) {
        document["editor"] = ruleSet.editor;
    }
    document["root"] = SerializeNode(ruleSet.root);
    return document;
}

std::string MassRuleParser::ToJsonString(const RuleSet& ruleSet, int indent) {
    return ToJson(ruleSet).dump(indent);
}

bool MassRuleParser::ParseRuleSetJson(const json& document, RuleSet& outRuleSet, std::string& outError) {
    if (!document.is_object()) {
        outError = "Massing rule document must be a JSON object";
        return false;
    }

    outRuleSet = RuleSet();
    outRuleSet.schemaVersion = document.value("schema_version", 1);
    outRuleSet.version = document.value("version", 1);
    outRuleSet.name = document.value("name", "");
    outRuleSet.description = document.value("description", "");
    outRuleSet.unit = document.value("unit", "meter");
    outRuleSet.ai = document.value("ai", json::object());
    outRuleSet.editor = document.value("editor", json::object());

    if (!document.contains("root")) {
        outError = "Massing rule document is missing 'root'";
        return false;
    }

    return ParseNode(document["root"], outRuleSet.root, outError);
}

bool MassRuleParser::ParseNode(const json& nodeJson, RuleNode& outNode, std::string& outError) {
    if (!nodeJson.is_object()) {
        outError = "Rule node must be a JSON object";
        return false;
    }

    const std::string typeString = nodeJson.value("type", "");
    if (typeString.empty()) {
        outError = "Rule node is missing 'type'";
        return false;
    }

    outNode = RuleNode();
    outNode.id = nodeJson.value("id", "");
    outNode.name = nodeJson.value("name", "");
    outNode.material = nodeJson.value("material", "");
    outNode.params = nodeJson.value("params", json::object());
    outNode.editor = nodeJson.value("editor", json::object());
    MergeInlineParams(nodeJson, outNode.params);

    if (!TryParseRuleNodeType(typeString, outNode.type)) {
        outError = "Unsupported rule node type: " + typeString;
        return false;
    }

    if (nodeJson.contains("transform")) {
        if (!ParseTransform(nodeJson["transform"], outNode.transform, outError)) {
            return false;
        }
    }

    if (outNode.type == RuleNodeType::Primitive) {
        const std::string primitiveString = nodeJson.value("primitive", "");
        if (primitiveString.empty()) {
            outError = "Primitive node is missing 'primitive'";
            return false;
        }
        if (!TryParsePrimitiveType(primitiveString, outNode.primitive)) {
            outError = "Unsupported primitive type: " + primitiveString;
            return false;
        }
    }

    if (outNode.type == RuleNodeType::Csg) {
        const std::string operationString = nodeJson.value("operation", "union");
        if (!TryParseCsgOperation(operationString, outNode.csgOperation)) {
            outError = "Unsupported CSG operation: " + operationString;
            return false;
        }
    }

    if (nodeJson.contains("profiles")) {
        if (!nodeJson["profiles"].is_array()) {
            outError = "'profiles' must be an array";
            return false;
        }
        for (const auto& profileJson : nodeJson["profiles"]) {
            Curve2D curve;
            if (!ParseCurve2D(profileJson, curve, outError)) {
                return false;
            }
            outNode.profiles.push_back(std::move(curve));
        }
    }

    if (nodeJson.contains("profile")) {
        Curve2D curve;
        if (!ParseCurve2D(nodeJson["profile"], curve, outError)) {
            return false;
        }
        outNode.profiles.push_back(std::move(curve));
    }

    if (nodeJson.contains("paths")) {
        if (!nodeJson["paths"].is_array()) {
            outError = "'paths' must be an array";
            return false;
        }
        for (const auto& pathJson : nodeJson["paths"]) {
            Curve3D curve;
            if (!ParseCurve3D(pathJson, curve, outError)) {
                return false;
            }
            outNode.paths.push_back(std::move(curve));
        }
    }

    if (nodeJson.contains("path")) {
        Curve3D curve;
        if (!ParseCurve3D(nodeJson["path"], curve, outError)) {
            return false;
        }
        outNode.paths.push_back(std::move(curve));
    }

    if (nodeJson.contains("children")) {
        if (!nodeJson["children"].is_array()) {
            outError = "'children' must be an array";
            return false;
        }
        for (const auto& childJson : nodeJson["children"]) {
            RuleNode child;
            if (!ParseNode(childJson, child, outError)) {
                return false;
            }
            outNode.children.push_back(std::move(child));
        }
    }

    return true;
}

bool MassRuleParser::ParseTransform(const json& transformJson, RuleTransform& outTransform, std::string& outError) {
    if (!transformJson.is_object()) {
        outError = "Transform must be a JSON object";
        return false;
    }

    if (transformJson.contains("position") && !TryReadVec3(transformJson["position"], outTransform.position)) {
        outError = "Transform 'position' must be a float[3]";
        return false;
    }
    if (transformJson.contains("rotation") && !TryReadVec3(transformJson["rotation"], outTransform.rotation)) {
        outError = "Transform 'rotation' must be a float[3]";
        return false;
    }
    if (transformJson.contains("scale") && !TryReadVec3(transformJson["scale"], outTransform.scale)) {
        outError = "Transform 'scale' must be a float[3]";
        return false;
    }

    return true;
}

bool MassRuleParser::ParseCurve2D(const json& curveJson, Curve2D& outCurve, std::string& outError) {
    if (!curveJson.is_object()) {
        outError = "2D curve must be a JSON object";
        return false;
    }
    if (!curveJson.contains("points") || !curveJson["points"].is_array()) {
        outError = "2D curve is missing 'points'";
        return false;
    }

    outCurve = Curve2D();
    outCurve.id = curveJson.value("id", "");
    outCurve.closed = curveJson.value("closed", false);

    for (const auto& pointJson : curveJson["points"]) {
        Vec2 point {};
        if (!TryReadVec2(pointJson, point)) {
            outError = "2D curve point must be float[2]";
            return false;
        }
        outCurve.points.push_back(point);
    }

    return !outCurve.points.empty();
}

bool MassRuleParser::ParseCurve3D(const json& curveJson, Curve3D& outCurve, std::string& outError) {
    if (!curveJson.is_object()) {
        outError = "3D curve must be a JSON object";
        return false;
    }
    if (!curveJson.contains("points") || !curveJson["points"].is_array()) {
        outError = "3D curve is missing 'points'";
        return false;
    }

    outCurve = Curve3D();
    outCurve.id = curveJson.value("id", "");
    outCurve.closed = curveJson.value("closed", false);

    for (const auto& pointJson : curveJson["points"]) {
        Vec3 point {};
        if (!TryReadVec3(pointJson, point)) {
            outError = "3D curve point must be float[3]";
            return false;
        }
        outCurve.points.push_back(point);
    }

    return !outCurve.points.empty();
}

json MassRuleParser::SerializeNode(const RuleNode& node) {
    json result;
    result["id"] = node.id;
    result["name"] = node.name;
    result["type"] = ToString(node.type);
    if (node.type == RuleNodeType::Primitive) {
        result["primitive"] = ToString(node.primitive);
    }
    if (node.type == RuleNodeType::Csg) {
        result["operation"] = ToString(node.csgOperation);
    }
    result["transform"] = SerializeTransform(node.transform);
    if (!node.material.empty()) {
        result["material"] = node.material;
    }
    if (!node.params.empty()) {
        result["params"] = node.params;
    }
    if (!node.editor.empty()) {
        result["editor"] = node.editor;
    }
    if (!node.profiles.empty()) {
        result["profiles"] = json::array();
        for (const Curve2D& curve : node.profiles) {
            json curveJson;
            curveJson["id"] = curve.id;
            curveJson["closed"] = curve.closed;
            curveJson["points"] = json::array();
            for (const Vec2& point : curve.points) {
                curveJson["points"].push_back(VecToJson(point));
            }
            result["profiles"].push_back(std::move(curveJson));
        }
    }
    if (!node.paths.empty()) {
        result["paths"] = json::array();
        for (const Curve3D& curve : node.paths) {
            json curveJson;
            curveJson["id"] = curve.id;
            curveJson["closed"] = curve.closed;
            curveJson["points"] = json::array();
            for (const Vec3& point : curve.points) {
                curveJson["points"].push_back(VecToJson(point));
            }
            result["paths"].push_back(std::move(curveJson));
        }
    }
    if (!node.children.empty()) {
        result["children"] = json::array();
        for (const RuleNode& child : node.children) {
            result["children"].push_back(SerializeNode(child));
        }
    }
    return result;
}

json MassRuleParser::SerializeTransform(const RuleTransform& transform) {
    json result;
    result["position"] = VecToJson(transform.position);
    result["rotation"] = VecToJson(transform.rotation);
    result["scale"] = VecToJson(transform.scale);
    return result;
}

} // namespace Massing
} // namespace Moon
