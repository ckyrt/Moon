#include "CSGBuilder.h"
#include "CSGOperations.h"
#include "../Logging/Logger.h"
#include <cmath>
#include <functional>
#include <cctype>

namespace Moon {
namespace CSG {

CSGBuilder::CSGBuilder() 
    : m_database(nullptr) {
}

CSGBuilder::~CSGBuilder() {
}

BuildResult CSGBuilder::Build(const Blueprint* blueprint,
                              const std::unordered_map<std::string, float>& parameterOverrides,
                              std::string& outError) {
    if (!blueprint) {
        outError = "Blueprint is null";
        MOON_LOG_ERROR("CSGBuilder", "%s", outError.c_str());
        return BuildResult();
    }

    // 创建根参数作用域
    ParameterScope rootScope;

    // 填充默认参数值
    for (const auto& param : blueprint->GetParameters()) {
        rootScope.SetValue(param.first, param.second.defaultValue);
    }

    // 应用参数覆盖
    for (const auto& override : parameterOverrides) {
        rootScope.SetValue(override.first, override.second);
    }

    // 构建根节点
    const Node* rootNode = blueprint->GetRootNode();
    if (!rootNode) {
        outError = "Blueprint has no root node";
        MOON_LOG_ERROR("CSGBuilder", "%s", outError.c_str());
        return BuildResult();
    }

    BuildResult result = BuildNode(rootNode, rootScope, outError);
    
    // 最终输出：将所有mesh转换为FlatShading（硬边效果）
    // 中间布尔运算保留拓扑以支持嵌套操作，这里统一转换
    for (auto& meshItem : result.meshes) {
        if (meshItem.mesh && meshItem.mesh->IsValid()) {
            meshItem.mesh = ConvertToFlatShading(meshItem.mesh.get());
            if (!meshItem.mesh) {
                outError = "Failed to convert mesh to FlatShading";
                MOON_LOG_ERROR("CSGBuilder", "%s", outError.c_str());
                return BuildResult();
            }
        }
    }
    
    return result;
}

BuildResult CSGBuilder::BuildNode(const Node* node, ParameterScope& scope, std::string& outError) {
    if (!node) {
        outError = "Node is null";
        return BuildResult();
    }

    switch (node->type) {
        case NodeType::Primitive:
            return BuildPrimitive(node->data.primitive, scope, outError);
        
        case NodeType::Csg:
            return BuildCSG(node->data.csg, scope, outError);
        
        case NodeType::Group:
            return BuildGroup(node->data.group, scope, outError);
        
        case NodeType::Reference:
            return BuildReference(node->data.ref, scope, outError);
        
        default:
            outError = "Unknown node type";
            return BuildResult();
    }
}

BuildResult CSGBuilder::BuildPrimitive(const PrimitiveNode* prim, ParameterScope& scope, std::string& outError) {
    if (!prim) {
        outError = "PrimitiveNode is null";
        return BuildResult();
    }

    std::shared_ptr<Mesh> mesh;

    // 解析 Transform
    Vector3 position, scale;
    Quaternion rotation;
    ResolveTransform(prim->localTransform, scope, position, rotation, scale, outError);

    // 根据基础几何体类型创建 Mesh
    switch (prim->primitive) {
        case PrimitiveType::Cube: {
            // 解析参数：支持size（正方体）或width/height/depth（长方体）
            float width = 1.0f;
            float height = 1.0f;
            float depth = 1.0f;
            
            auto itSize = prim->params.find("size");
            if (itSize != prim->params.end()) {
                float size = ResolveValue(itSize->second, scope, outError);
                width = height = depth = size;
            }
            
            auto itWidth = prim->params.find("width");
            if (itWidth != prim->params.end()) {
                width = ResolveValue(itWidth->second, scope, outError);
            }
            
            auto itHeight = prim->params.find("height");
            if (itHeight != prim->params.end()) {
                height = ResolveValue(itHeight->second, scope, outError);
            }
            
            auto itDepth = prim->params.find("depth");
            if (itDepth != prim->params.end()) {
                depth = ResolveValue(itDepth->second, scope, outError);
            }

            mesh = CreateCSGBox(width, height, depth,
                position,
                Vector3(0, 0, 0), // rotation 将通过四元数应用
                scale,
                false); // flatShading = false for CSG operations

            break;
        }

        case PrimitiveType::Sphere: {
            float radius = 1.0f;
            auto it = prim->params.find("radius");
            if (it != prim->params.end()) {
                radius = ResolveValue(it->second, scope, outError);
            }

            mesh = CreateCSGSphere(radius, 32,
                position,
                Vector3(0, 0, 0),
                scale,
                false);

            break;
        }

        case PrimitiveType::Cylinder: {
            float radius = 1.0f;
            float height = 2.0f;
            
            auto itRadius = prim->params.find("radius");
            if (itRadius != prim->params.end()) {
                radius = ResolveValue(itRadius->second, scope, outError);
            }
            
            auto itHeight = prim->params.find("height");
            if (itHeight != prim->params.end()) {
                height = ResolveValue(itHeight->second, scope, outError);
            }

            mesh = CreateCSGCylinder(radius, height, 32,
                position,
                Vector3(0, 0, 0),
                scale,
                false);

            break;
        }

        case PrimitiveType::Cone: {
            float radius = 1.0f;
            float height = 2.0f;
            
            auto itRadius = prim->params.find("radius");
            if (itRadius != prim->params.end()) {
                radius = ResolveValue(itRadius->second, scope, outError);
            }
            
            auto itHeight = prim->params.find("height");
            if (itHeight != prim->params.end()) {
                height = ResolveValue(itHeight->second, scope, outError);
            }

            mesh = CreateCSGCone(radius, height, 32,
                position,
                Vector3(0, 0, 0),
                scale,
                false);

            break;
        }

        case PrimitiveType::Capsule:
        case PrimitiveType::Torus:
            outError = "Primitive type not yet implemented";
            MOON_LOG_WARN("CSGBuilder", "%s", outError.c_str());
            return BuildResult();
    }

    if (!mesh) {
        outError = "Failed to create primitive mesh";
        MOON_LOG_ERROR("CSGBuilder", "%s", outError.c_str());
        return BuildResult();
    }

    // 应用旋转（如果有）
    // TODO: 在 CreateCSG* 函数中应用四元数旋转

    BuildResult result;
    result.AddMesh(MeshItem(mesh, prim->material, ResolvedTransform(position, rotation, scale)));
    return result;
}

BuildResult CSGBuilder::BuildCSG(const CsgNode* csg, ParameterScope& scope, std::string& outError) {
    if (!csg) {
        outError = "CsgNode is null";
        return BuildResult();
    }

    // 构建左右子节点
    BuildResult leftResult = BuildNode(csg->left.get(), scope, outError);
    if (leftResult.meshes.empty()) {
        outError = "CSG left child produced no meshes";
        return BuildResult();
    }

    BuildResult rightResult = BuildNode(csg->right.get(), scope, outError);
    if (rightResult.meshes.empty()) {
        outError = "CSG right child produced no meshes";
        return BuildResult();
    }

    // 第一版限制：左右各只能有一个 mesh
    if (leftResult.meshes.size() > 1 || rightResult.meshes.size() > 1) {
        outError = "CSG operation requires exactly one mesh on each side (M1 limitation)";
        MOON_LOG_ERROR("CSGBuilder", "%s", outError.c_str());
        return BuildResult();
    }

    // 执行 CSG 运算
    Operation op;
    switch (csg->operation) {
        case CsgOp::Union: op = Operation::Union; break;
        case CsgOp::Subtract: op = Operation::Subtract; break;
        case CsgOp::Intersect: op = Operation::Intersect; break;
        default:
            outError = "Unknown CSG operation";
            return BuildResult();
    }

    std::shared_ptr<Mesh> resultMesh = PerformBoolean(
        leftResult.meshes[0].mesh.get(),
        rightResult.meshes[0].mesh.get(),
        op
    );

    if (!resultMesh) {
        outError = "CSG boolean operation failed";
        MOON_LOG_ERROR("CSGBuilder", "%s", outError.c_str());
        return BuildResult();
    }

    // 合并材质（优先使用左侧）
    std::string material = leftResult.meshes[0].material;

    BuildResult result;
    result.AddMesh(MeshItem(resultMesh, material, ResolvedTransform()));
    return result;
}

BuildResult CSGBuilder::BuildGroup(const GroupNode* group, ParameterScope& scope, std::string& outError) {
    if (!group) {
        outError = "GroupNode is null";
        return BuildResult();
    }

    BuildResult result;

    for (const auto& child : group->children) {
        BuildResult childResult = BuildNode(child.get(), scope, outError);
        result.Merge(std::move(childResult));
    }

    return result;
}

BuildResult CSGBuilder::BuildReference(const RefNode* ref, ParameterScope& scope, std::string& outError) {
    if (!ref) {
        outError = "RefNode is null";
        return BuildResult();
    }

    if (!m_database) {
        outError = "Blueprint database not set, cannot resolve reference: " + ref->refId;
        MOON_LOG_ERROR("CSGBuilder", "%s", outError.c_str());
        return BuildResult();
    }

    // 查找被引用的 Blueprint
    const Blueprint* refBlueprint = m_database->GetBlueprint(ref->refId);
    if (!refBlueprint) {
        outError = "Referenced blueprint not found: " + ref->refId;
        MOON_LOG_ERROR("CSGBuilder", "%s", outError.c_str());
        return BuildResult();
    }

    // 创建子作用域
    ParameterScope childScope = scope.CreateChild();

    // 应用 overrides
    for (const auto& override : ref->overrides) {
        float value = ResolveValue(override.second, scope, outError);
        childScope.SetValue(override.first, value);
    }

    // 构建被引用的 Blueprint
    BuildResult childResult = Build(refBlueprint, {}, outError);

    // 解析 Reference 的 transform
    Vector3 refPosition, refScale;
    Quaternion refRotation;
    ResolveTransform(ref->localTransform, scope, refPosition, refRotation, refScale, outError);

    // 应用 transform 到所有生成的 mesh
    for (auto& meshItem : childResult.meshes) {
        // 简化版：直接应用位置偏移
        // TODO: 完整的 transform 组合（包括rotation和scale）
        meshItem.worldTransform.position = meshItem.worldTransform.position + refPosition;
    }

    return childResult;
}

float CSGBuilder::ResolveValue(const ValueExpr& expr, ParameterScope& scope, std::string& outError) {
    switch (expr.kind) {
        case ValueExpr::Kind::Constant:
            return expr.constantValue;
        
        case ValueExpr::Kind::ParamRef: {
            float value;
            if (scope.GetValue(expr.paramName, value)) {
                return value;
            } else {
                outError = "Parameter not found: " + expr.paramName;
                MOON_LOG_ERROR("CSGBuilder", "%s", outError.c_str());
                return 0.0f;
            }
        }
        
        case ValueExpr::Kind::Expression: {
            float result = EvaluateExpression(expr.expression, scope, outError);
            MOON_LOG_INFO("CSGBuilder", "Expression '%s' evaluated to: %.3f", expr.expression.c_str(), result);
            return result;
        }
        
        default:
            outError = "Unknown value expression kind";
            return 0.0f;
    }
}

float CSGBuilder::EvaluateExpression(const std::string& exprStr, ParameterScope& scope, std::string& outError) {
    // 简单的表达式求值器（支持 +, -, *, / 和参数）
    // 算法：递归下降解析
    
    std::string expr = exprStr;
    size_t pos = 0;

    // 跳过空格
    auto skipSpaces = [&]() {
        while (pos < expr.length() && std::isspace(expr[pos])) {
            pos++;
        }
    };

    // 前向声明parseExpression以支持括号
    std::function<float()> parseExpression;

    // 解析数字或参数引用
    std::function<float()> parsePrimary = [&]() -> float {
        skipSpaces();
        
        // 处理负号
        bool negative = false;
        if (pos < expr.length() && expr[pos] == '-') {
            negative = true;
            pos++;
            skipSpaces();
        }
        
        // 处理括号
        if (pos < expr.length() && expr[pos] == '(') {
            pos++; // 跳过 '('
            float value = parseExpression();
            skipSpaces();
            if (pos < expr.length() && expr[pos] == ')') {
                pos++; // 跳过 ')'
                return negative ? -value : value;
            } else {
                outError = "Mismatched parentheses";
                return 0.0f;
            }
        }
        
        // 参数引用
        if (pos < expr.length() && expr[pos] == '$') {
            pos++; // 跳过 $
            size_t start = pos;
            while (pos < expr.length() && (std::isalnum(expr[pos]) || expr[pos] == '_')) {
                pos++;
            }
            std::string paramName = expr.substr(start, pos - start);
            
            float value;
            if (scope.GetValue(paramName, value)) {
                return negative ? -value : value;
            } else {
                outError = "Parameter not found: " + paramName;
                return 0.0f;
            }
        }
        
        // 数字常量
        if (pos < expr.length() && (std::isdigit(expr[pos]) || expr[pos] == '.')) {
            size_t start = pos;
            while (pos < expr.length() && (std::isdigit(expr[pos]) || expr[pos] == '.')) {
                pos++;
            }
            float value = std::stof(expr.substr(start, pos - start));
            return negative ? -value : value;
        }
        
        outError = "Invalid expression syntax";
        return 0.0f;
    };

    // 解析乘除
    std::function<float()> parseTerm = [&]() -> float {
        float left = parsePrimary();
        
        while (pos < expr.length()) {
            skipSpaces();
            if (pos >= expr.length()) break;
            
            char op = expr[pos];
            if (op == '*' || op == '/') {
                pos++;
                float right = parsePrimary();
                if (op == '*') {
                    left *= right;
                } else {
                    if (right != 0.0f) {
                        left /= right;
                    } else {
                        outError = "Division by zero";
                        return 0.0f;
                    }
                }
            } else {
                break;
            }
        }
        
        return left;
    };

    // 解析加减
    parseExpression = [&]() -> float {
        float left = parseTerm();
        
        while (pos < expr.length()) {
            skipSpaces();
            if (pos >= expr.length()) break;
            
            char op = expr[pos];
            if (op == '+' || op == '-') {
                pos++;
                float right = parseTerm();
                if (op == '+') {
                    left += right;
                } else {
                    left -= right;
                }
            } else {
                break;
            }
        }
        
        return left;
    };

    return parseExpression();
}

void CSGBuilder::ResolveTransform(const TransformTRS& transformExpr, ParameterScope& scope,
                                   Vector3& outPosition, Quaternion& outRotation, Vector3& outScale,
                                   std::string& outError) {
    // Position
    float px = ResolveValue(transformExpr.positionX, scope, outError);
    float py = ResolveValue(transformExpr.positionY, scope, outError);
    float pz = ResolveValue(transformExpr.positionZ, scope, outError);
    outPosition = Vector3(px, py, pz);

    // Rotation (Euler angles in degrees -> Quaternion)
    float rx = ResolveValue(transformExpr.rotationX, scope, outError);
    float ry = ResolveValue(transformExpr.rotationY, scope, outError);
    float rz = ResolveValue(transformExpr.rotationZ, scope, outError);
    outRotation = Quaternion::Euler(Vector3(rx, ry, rz));

    // Scale
    float sx = ResolveValue(transformExpr.scaleX, scope, outError);
    float sy = ResolveValue(transformExpr.scaleY, scope, outError);
    float sz = ResolveValue(transformExpr.scaleZ, scope, outError);
    outScale = Vector3(sx, sy, sz);
}

} // namespace CSG
} // namespace Moon
