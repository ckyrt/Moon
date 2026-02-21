#include "BlueprintLoader.h"
#include "../Logging/Logger.h"
#include "../../../external/nlohmann/json.hpp"
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace Moon {
namespace CSG {

std::unique_ptr<Blueprint> BlueprintLoader::LoadFromFile(const std::string& filepath, std::string& outError) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        outError = "Failed to open file: " + filepath;
        MOON_LOG_ERROR("BlueprintLoader", "%s", outError.c_str());
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    return ParseFromString(content, outError);
}

std::unique_ptr<Blueprint> BlueprintLoader::ParseFromString(const std::string& jsonContent, std::string& outError) {
    try {
        MOON_LOG_INFO("BlueprintLoader", "Parsing JSON string (%zu bytes)...", jsonContent.size());
        json j = json::parse(jsonContent);
        MOON_LOG_INFO("BlueprintLoader", "JSON parsed successfully");
        
        auto blueprint = std::make_unique<Blueprint>();

        // 1. 解析元数据
        MOON_LOG_INFO("BlueprintLoader", "Parsing metadata...");
        if (!ParseMetadata(&j, blueprint.get(), outError)) {
            return nullptr;
        }

        // 2. 解析参数
        if (j.contains("parameters")) {
            MOON_LOG_INFO("BlueprintLoader", "Parsing parameters...");
            if (!ParseParameters(&j["parameters"], blueprint.get(), outError)) {
                return nullptr;
            }
        }

        // 3. 解析 root 节点
        if (!j.contains("root")) {
            outError = "Blueprint missing 'root' node";
            MOON_LOG_ERROR("BlueprintLoader", "%s", outError.c_str());
            return nullptr;
        }

        MOON_LOG_INFO("BlueprintLoader", "Parsing root node...");
        auto rootNode = ParseNode(&j["root"], outError);
        if (!rootNode) {
            return nullptr;
        }

        blueprint->SetRootNode(std::move(rootNode));

        // 4. 验证
        if (!blueprint->Validate(outError)) {
            MOON_LOG_ERROR("BlueprintLoader", "Blueprint validation failed: %s", outError.c_str());
            return nullptr;
        }

        MOON_LOG_INFO("BlueprintLoader", "Successfully loaded blueprint: %s", blueprint->GetId().c_str());
        return blueprint;

    } catch (const json::exception& e) {
        outError = std::string("JSON parse error: ") + e.what();
        MOON_LOG_ERROR("BlueprintLoader", "%s", outError.c_str());
        return nullptr;
    }
}

bool BlueprintLoader::ParseMetadata(const void* jsonPtr, Blueprint* blueprint, std::string& outError) {
    const json& j = *static_cast<const json*>(jsonPtr);

    // schema_version (必需) - 整数版本号
    if (!j.contains("schema_version")) {
        outError = "Missing 'schema_version' field";
        return false;
    }
    blueprint->SetSchemaVersion(j["schema_version"].get<int>());

    // name (作为id使用，如果没有id字段的话)
    std::string blueprintName;
    if (j.contains("name")) {
        blueprintName = j["name"].get<std::string>();
    }

    // id (可选，如果没有则使用name)
    if (j.contains("id")) {
        blueprint->SetId(j["id"].get<std::string>());
    } else if (!blueprintName.empty()) {
        blueprint->SetId(blueprintName);
    } else {
        outError = "Missing both 'id' and 'name' fields";
        return false;
    }

    // category (可选)
    if (j.contains("category")) {
        blueprint->SetCategory(j["category"].get<std::string>());
    }

    // tags (可选)
    if (j.contains("tags") && j["tags"].is_array()) {
        std::vector<std::string> tags;
        for (const auto& tag : j["tags"]) {
            tags.push_back(tag.get<std::string>());
        }
        blueprint->SetTags(tags);
    }

    return true;
}

bool BlueprintLoader::ParseParameters(const void* jsonPtr, Blueprint* blueprint, std::string& outError) {
    const json& j = *static_cast<const json*>(jsonPtr);

    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string& paramName = it.key();
        const json& paramDef = it.value();

        ParameterDef def;

        // 支持简化格式：直接数值
        if (paramDef.is_number()) {
            def.type = ParameterDef::Type::Float;
            def.defaultValue = paramDef.get<float>();
            blueprint->AddParameter(paramName, def);
            continue;
        }

        // 完整格式：对象
        if (!paramDef.is_object()) {
            outError = "Parameter definition must be a number or object";
            return false;
        }

        // type
        if (paramDef.contains("type")) {
            std::string typeStr = paramDef["type"].get<std::string>();
            if (typeStr == "float") {
                def.type = ParameterDef::Type::Float;
            } else if (typeStr == "int") {
                def.type = ParameterDef::Type::Int;
            } else if (typeStr == "bool") {
                def.type = ParameterDef::Type::Bool;
            }
        }

        // default
        if (paramDef.contains("default")) {
            def.defaultValue = paramDef["default"].get<float>();
        }

        // min/max
        if (paramDef.contains("min")) {
            def.minValue = paramDef["min"].get<float>();
        }
        if (paramDef.contains("max")) {
            def.maxValue = paramDef["max"].get<float>();
        }

        blueprint->AddParameter(paramName, def);
    }

    return true;
}

std::unique_ptr<Node> BlueprintLoader::ParseNode(const void* jsonPtr, std::string& outError) {
    const json& j = *static_cast<const json*>(jsonPtr);

    if (!j.contains("type")) {
        outError = "Node missing 'type' field";
        return nullptr;
    }

    std::string typeStr = j["type"].get<std::string>();
    NodeType nodeType;

    if (typeStr == "primitive") {
        nodeType = NodeType::Primitive;
    } else if (typeStr == "csg") {
        nodeType = NodeType::Csg;
    } else if (typeStr == "group") {
        nodeType = NodeType::Group;
    } else if (typeStr == "reference") {
        nodeType = NodeType::Reference;
    } else {
        outError = "Unknown node type: " + typeStr;
        return nullptr;
    }

    auto node = std::make_unique<Node>(nodeType);

    // 根据类型解析不同的数据
    switch (nodeType) {
        case NodeType::Primitive: {
            PrimitiveNode* prim = node->AsPrimitive();
            
            // primitive type
            if (!j.contains("primitive")) {
                outError = "Primitive node missing 'primitive' field";
                return nullptr;
            }
            std::string primStr = j["primitive"].get<std::string>();
            if (primStr == "cube") prim->primitive = PrimitiveType::Cube;
            else if (primStr == "sphere") prim->primitive = PrimitiveType::Sphere;
            else if (primStr == "cylinder") prim->primitive = PrimitiveType::Cylinder;
            else if (primStr == "capsule") prim->primitive = PrimitiveType::Capsule;
            else if (primStr == "cone") prim->primitive = PrimitiveType::Cone;
            else if (primStr == "torus") prim->primitive = PrimitiveType::Torus;
            else {
                outError = "Unknown primitive type: " + primStr;
                return nullptr;
            }

            // params
            if (j.contains("params")) {
                for (auto it = j["params"].begin(); it != j["params"].end(); ++it) {
                    prim->params[it.key()] = ParseValueExpr(&it.value(), outError);
                }
            }

            // transform
            if (j.contains("transform")) {
                ParseTransform(&j["transform"], prim->localTransform, outError);
            }

            // material
            if (j.contains("material")) {
                prim->material = j["material"].get<std::string>();
            }

            break;
        }

        case NodeType::Csg: {
            CsgNode* csg = node->AsCsg();

            // operation
            if (!j.contains("operation")) {
                outError = "CSG node missing 'operation' field";
                return nullptr;
            }
            std::string opStr = j["operation"].get<std::string>();
            if (opStr == "union") csg->operation = CsgOp::Union;
            else if (opStr == "subtract") csg->operation = CsgOp::Subtract;
            else if (opStr == "intersect") csg->operation = CsgOp::Intersect;
            else {
                outError = "Unknown CSG operation: " + opStr;
                return nullptr;
            }

            // options
            if (j.contains("options")) {
                const json& opts = j["options"];
                if (opts.contains("solver")) {
                    csg->options.solver = opts["solver"].get<std::string>();
                }
                if (opts.contains("weld_epsilon")) {
                    csg->options.weldEpsilon = opts["weld_epsilon"].get<float>();
                }
                if (opts.contains("recompute_normals")) {
                    csg->options.recomputeNormals = opts["recompute_normals"].get<bool>();
                }
            }

            // left & right
            if (!j.contains("left") || !j.contains("right")) {
                outError = "CSG node missing 'left' or 'right' child";
                return nullptr;
            }

            csg->left = ParseNode(&j["left"], outError);
            if (!csg->left) return nullptr;

            csg->right = ParseNode(&j["right"], outError);
            if (!csg->right) return nullptr;

            break;
        }

        case NodeType::Group: {
            GroupNode* group = node->AsGroup();

            if (!j.contains("children") || !j["children"].is_array()) {
                outError = "Group node missing 'children' array";
                return nullptr;
            }

            for (const auto& childJson : j["children"]) {
                auto child = ParseNode(&childJson, outError);
                if (!child) return nullptr;
                group->children.push_back(std::move(child));
            }

            // output mode
            if (j.contains("output") && j["output"].contains("mode")) {
                std::string mode = j["output"]["mode"].get<std::string>();
                if (mode == "separate") {
                    group->outputMode = GroupOutputMode::Separate;
                } else if (mode == "merge") {
                    group->outputMode = GroupOutputMode::Merge;
                }
            }

            break;
        }

        case NodeType::Reference: {
            RefNode* ref = node->AsRef();

            if (!j.contains("ref")) {
                outError = "Reference node missing 'ref' field";
                return nullptr;
            }
            ref->refId = j["ref"].get<std::string>();

            // transform
            if (j.contains("transform")) {
                ParseTransform(&j["transform"], ref->localTransform, outError);
            }

            // overrides
            if (j.contains("overrides")) {
                for (auto it = j["overrides"].begin(); it != j["overrides"].end(); ++it) {
                    ref->overrides[it.key()] = ParseValueExpr(&it.value(), outError);
                }
            }

            break;
        }
    }

    return node;
}

bool BlueprintLoader::ParseTransform(const void* jsonPtr, TransformTRS& transform, std::string& outError) {
    const json& j = *static_cast<const json*>(jsonPtr);

    // position
    if (j.contains("position") && j["position"].is_array() && j["position"].size() == 3) {
        transform.positionX = ParseValueExpr(&j["position"][0], outError);
        transform.positionY = ParseValueExpr(&j["position"][1], outError);
        transform.positionZ = ParseValueExpr(&j["position"][2], outError);
    }

    // rotation (euler angles in degrees)
    if (j.contains("rotation_euler") && j["rotation_euler"].is_array() && j["rotation_euler"].size() == 3) {
        transform.rotationX = ParseValueExpr(&j["rotation_euler"][0], outError);
        transform.rotationY = ParseValueExpr(&j["rotation_euler"][1], outError);
        transform.rotationZ = ParseValueExpr(&j["rotation_euler"][2], outError);
    }

    // scale
    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() == 3) {
        transform.scaleX = ParseValueExpr(&j["scale"][0], outError);
        transform.scaleY = ParseValueExpr(&j["scale"][1], outError);
        transform.scaleZ = ParseValueExpr(&j["scale"][2], outError);
    }

    return true;
}

ValueExpr BlueprintLoader::ParseValueExpr(const void* jsonPtr, std::string& outError) {
    const json& j = *static_cast<const json*>(jsonPtr);

    MOON_LOG_INFO("BlueprintLoader", "ParseValueExpr: type=%s, value=%s", j.type_name(), j.dump().c_str());

    if (j.is_number()) {
        return ValueExpr::Constant(j.get<float>());
    } else if (j.is_string()) {
        std::string str = j.get<std::string>();
        
        // 检查是否是表达式（包含运算符、括号、空格或以负号开头）
        bool isExpression = (str.find('+') != std::string::npos ||
                            str.find('-') != std::string::npos ||
                            str.find('*') != std::string::npos ||
                            str.find('/') != std::string::npos ||
                            str.find('(') != std::string::npos ||
                            str.find(')') != std::string::npos ||
                            str.find(' ') != std::string::npos);
        
        if (isExpression) {
            // 复杂表达式
            return ValueExpr::Expression(str);
        } else if (!str.empty() && str[0] == '$') {
            // 简单参数引用
            return ValueExpr::ParamRef(str.substr(1)); // 去掉 '$' 前缀
        } else {
            outError = "String value must start with '$' for parameter reference or be an expression";
            MOON_LOG_ERROR("BlueprintLoader", "Invalid string value: %s", str.c_str());
            return ValueExpr::Constant(0.0f);
        }
    }

    outError = "Invalid value expression, must be number or string (type: " + std::string(j.type_name()) + ")";
    MOON_LOG_ERROR("BlueprintLoader", "%s", outError.c_str());
    return ValueExpr::Constant(0.0f);
}

} // namespace CSG
} // namespace Moon
