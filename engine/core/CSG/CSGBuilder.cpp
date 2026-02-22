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

    m_buildDepth++;
    BuildResult result = BuildNode(rootNode, rootScope, outError);
    m_buildDepth--;

    // 最终输出：仅在最外层 Build 将所有 mesh 转换为 FlatShading
    // 内层递归 Build（来自 reference）已经做过 FlatShading，不可重复
    if (m_buildDepth == 0) {
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
            // 解析参数：支持size（正方体）或size_x/size_y/size_z（长方体）
            float size_x = 1.0f;
            float size_y = 1.0f;
            float size_z = 1.0f;
            
            auto itSize = prim->params.find("size");
            if (itSize != prim->params.end()) {
                float size = ResolveValue(itSize->second, scope, outError);
                size_x = size_y = size_z = size;
            }
            
            auto itSizeX = prim->params.find("size_x");
            if (itSizeX != prim->params.end()) {
                size_x = ResolveValue(itSizeX->second, scope, outError);
            }
            
            auto itSizeY = prim->params.find("size_y");
            if (itSizeY != prim->params.end()) {
                size_y = ResolveValue(itSizeY->second, scope, outError);
            }
            
            auto itSizeZ = prim->params.find("size_z");
            if (itSizeZ != prim->params.end()) {
                size_z = ResolveValue(itSizeZ->second, scope, outError);
            }

            // JSON 单位为 cm，转换为引擎单位（米）
            static constexpr float CM_TO_M = 0.01f;
            mesh = CreateCSGBox(size_x * CM_TO_M, size_y * CM_TO_M, size_z * CM_TO_M,
                Vector3(0, 0, 0),  // Keep mesh at origin, position applied via worldTransform
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

            static constexpr float CM_TO_M = 0.01f;
            mesh = CreateCSGSphere(radius * CM_TO_M, 32,
                Vector3(0, 0, 0),  // Keep mesh at origin, position applied via worldTransform
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

            static constexpr float CM_TO_M = 0.01f;
            mesh = CreateCSGCylinder(radius * CM_TO_M, height * CM_TO_M, 32,
                Vector3(0, 0, 0),  // Keep mesh at origin, position applied via worldTransform
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

            static constexpr float CM_TO_M = 0.01f;
            mesh = CreateCSGCone(radius * CM_TO_M, height * CM_TO_M, 32,
                Vector3(0, 0, 0),  // Keep mesh at origin, position applied via worldTransform
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

    // BuildPrimitive 故意把 mesh 保留在原点，position 存在 worldTransform 里（"applied later"）
    // 在 CSG 布尔运算之前必须把 position bake 进顶点，否则所有 primitive 都在原点做运算
    auto bakePosition = [](const Mesh* src, const Vector3& pos) -> std::shared_ptr<Mesh> {
        if (pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f) return nullptr;
        std::vector<Vertex> verts = src->GetVertices();
        for (auto& v : verts) {
            v.position.x += pos.x;
            v.position.y += pos.y;
            v.position.z += pos.z;
        }
        auto m = std::make_shared<Mesh>();
        m->SetVertices(std::move(verts));
        auto idx = src->GetIndices();
        m->SetIndices(std::move(idx));
        return m;
    };

    const Vector3& leftPos  = leftResult.meshes[0].worldTransform.position;
    const Vector3& rightPos = rightResult.meshes[0].worldTransform.position;
    std::shared_ptr<Mesh> leftBaked  = bakePosition(leftResult.meshes[0].mesh.get(),  leftPos);
    std::shared_ptr<Mesh> rightBaked = bakePosition(rightResult.meshes[0].mesh.get(), rightPos);
    const Mesh* leftMesh  = leftBaked  ? leftBaked.get()  : leftResult.meshes[0].mesh.get();
    const Mesh* rightMesh = rightBaked ? rightBaked.get() : rightResult.meshes[0].mesh.get();

    std::shared_ptr<Mesh> resultMesh = PerformBoolean(leftMesh, rightMesh, op);

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

    // =========================================================================
    // Phase A：构建所有子节点，同时收集命名节点的 anchor 信息
    // =========================================================================
    struct BuiltChild {
        BuildResult result;
        Vector3 basePosition;       // transform 解析出的基础位置（不含 attach）
        std::unordered_map<std::string, Vector3> anchors; // 求值后的 anchor（local space, cm→m 已转换）
    };

    std::vector<BuiltChild> builtChildren;
    builtChildren.reserve(group->children.size());

    // name -> index 映射（用于 Phase B 的 target_path 解析）
    std::unordered_map<std::string, size_t> nameToIndex;

    for (size_t i = 0; i < group->children.size(); ++i) {
        const Node* childNode = group->children[i].get();
        const std::string& childName = (i < group->childNames.size()) ? group->childNames[i] : "";

        BuiltChild bc;
        bc.result = BuildNode(childNode, scope, outError);
        bc.basePosition = Vector3(0, 0, 0);
        
        // 如果是 reference 节点，获取其 transform position 和 anchors
        if (childNode->type == NodeType::Reference) {
            const RefNode* ref = childNode->data.ref;
            Vector3 refPos, refScale;
            Quaternion refRot;
            ResolveTransform(ref->localTransform, scope, refPos, refRot, refScale, outError);
            bc.basePosition = refPos;

            // 获取被引用 blueprint 的 anchors（用子参数 scope 求值）
            if (m_database) {
                const Blueprint* refBp = m_database->GetBlueprint(ref->refId);
                if (refBp) {
                    // 构建子 scope（含 overrides）
                    ParameterScope childScope = scope.CreateChild();
                    for (const auto& param : refBp->GetParameters()) {
                        childScope.SetValue(param.first, param.second.defaultValue);
                    }
                    for (const auto& ov : ref->overrides) {
                        float val = ResolveValue(ov.second, scope, outError);
                        childScope.SetValue(ov.first, val);
                    }
                    bc.anchors = EvaluateAnchors(refBp, childScope, outError);
                }
            }
        }

        if (!childName.empty()) {
            nameToIndex[childName] = i;
        }

        builtChildren.push_back(std::move(bc));
    }

    // =========================================================================
    // Phase B：处理 attach 约束，修正每个 child 的最终位置
    // =========================================================================
    for (size_t i = 0; i < group->children.size(); ++i) {
        const Node* childNode = group->children[i].get();
        if (childNode->type != NodeType::Reference) continue;

        const RefNode* ref = childNode->data.ref;
        if (!ref->attach.hasAttach) continue;

        const AttachDef& att = ref->attach;

        // 1. 找到 self_anchor（本节点自己的 anchor，local space）
        auto& selfAnchors = builtChildren[i].anchors;
        auto itSelf = selfAnchors.find(att.selfAnchor);
        if (itSelf == selfAnchors.end()) {
            MOON_LOG_ERROR("CSGBuilder", "Attach error: self_anchor '%s' not found in '%s'",
                           att.selfAnchor.c_str(), ref->refId.c_str());
            outError = "Missing self_anchor: " + att.selfAnchor + " in " + ref->refId;
            return BuildResult();
        }
        Vector3 selfAnchorLocal = itSelf->second;

        // 2. 解析 target_path（v1 只支持 "../<name>"）
        std::string targetName = att.targetPath;
        if (targetName.substr(0, 3) == "../") {
            targetName = targetName.substr(3);
        }
        auto itTarget = nameToIndex.find(targetName);
        if (itTarget == nameToIndex.end()) {
            MOON_LOG_ERROR("CSGBuilder", "Attach error: target '%s' not found in group", targetName.c_str());
            outError = "Missing attach target: " + att.targetPath;
            return BuildResult();
        }
        size_t targetIdx = itTarget->second;
        if (targetIdx >= i) {
            MOON_LOG_ERROR("CSGBuilder", "Attach error: target '%s' must appear before self in children list", targetName.c_str());
            outError = "Attach target must be defined before self in children list";
            return BuildResult();
        }

        // 3. 找到 target_anchor（目标节点的 anchor，local space）
        auto& targetAnchors = builtChildren[targetIdx].anchors;
        auto itTarget2 = targetAnchors.find(att.targetAnchor);
        if (itTarget2 == targetAnchors.end()) {
            MOON_LOG_ERROR("CSGBuilder", "Attach error: target_anchor '%s' not found in target '%s'",
                           att.targetAnchor.c_str(), targetName.c_str());
            outError = "Missing target_anchor: " + att.targetAnchor + " in " + targetName;
            return BuildResult();
        }
        Vector3 targetAnchorLocal = itTarget2->second;

        // 4. 计算对齐位移（全部在 parent local space 内，因为是同一 group 的 sibling）
        // 目标 anchor 的 parent-space 位置 = target.basePosition + targetAnchorLocal
        // 本节点 self anchor 的 parent-space 位置（未修正前）= self.basePosition + selfAnchorLocal
        // 需要: (self.basePosition + delta) + selfAnchorLocal == target.basePosition + targetAnchorLocal
        // → delta = (target.basePosition + targetAnchorLocal) - (self.basePosition + selfAnchorLocal)
        Vector3 targetWorld = builtChildren[targetIdx].basePosition + targetAnchorLocal;
        Vector3 selfWorld0  = builtChildren[i].basePosition + selfAnchorLocal;
        // v1: 只修正 Y（垂直对齐），X/Z 由 self 的 transform.position 提供
        // 全局 delta 会把腿的 XZ 手抗掉（因为 anchor.x/z 在中心 but 腿在发角）
        float deltaY = (targetWorld.y - selfWorld0.y);
        Vector3 delta(0.0f, deltaY, 0.0f);

        MOON_LOG_INFO("CSGBuilder",
            "Attach '%s'.%s → '%s'.%s: delta=(%.4f, %.4f, %.4f)m",
            ref->refId.c_str(), att.selfAnchor.c_str(),
            targetName.c_str(), att.targetAnchor.c_str(),
            delta.x, delta.y, delta.z);

        // 5. 把 delta 应用到该 child 的所有 mesh worldTransform.position
        for (auto& meshItem : builtChildren[i].result.meshes) {
            meshItem.worldTransform.position = meshItem.worldTransform.position + delta;
        }
        // 同时更新 basePosition（万一后续其他节点 attach 到本节点）
        builtChildren[i].basePosition = builtChildren[i].basePosition + delta;
    }

    // =========================================================================
    // 合并所有子节点结果
    // =========================================================================
    BuildResult result;
    for (auto& bc : builtChildren) {
        result.Merge(std::move(bc.result));
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
    std::unordered_map<std::string, float> overrideValues;
    for (const auto& override : ref->overrides) {
        float value = ResolveValue(override.second, scope, outError);
        overrideValues[override.first] = value;
    }

    // 构建被引用的 Blueprint（传入已求値的 overrides）
    BuildResult childResult = Build(refBlueprint, overrideValues, outError);

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

std::unordered_map<std::string, Vector3> CSGBuilder::EvaluateAnchors(
    const Blueprint* blueprint, ParameterScope& scope, std::string& outError)
{
    static constexpr float CM_TO_M = 0.01f;
    std::unordered_map<std::string, Vector3> result;

    for (const auto& kv : blueprint->GetAnchors()) {
        const std::string& name = kv.first;
        const Blueprint::AnchorExpr& expr = kv.second;

        float x = EvaluateStringExpr(expr[0], scope, outError) * CM_TO_M;
        float y = EvaluateStringExpr(expr[1], scope, outError) * CM_TO_M;
        float z = EvaluateStringExpr(expr[2], scope, outError) * CM_TO_M;

        result[name] = Vector3(x, y, z);
        MOON_LOG_INFO("CSGBuilder", "  Anchor '%s' = (%.4f, %.4f, %.4f) m", name.c_str(), x, y, z);
    }

    return result;
}

float CSGBuilder::EvaluateStringExpr(const std::string& exprStr, ParameterScope& scope, std::string& outError) {
    // 尝试直接解析成数字（例如 "0" 或 "3.14"）
    try {
        size_t idx = 0;
        float val = std::stof(exprStr, &idx);
        if (idx == exprStr.size()) return val;
    } catch (...) {}

    // 否则走表达式求值器（与 EvaluateExpression 共用）
    return EvaluateExpression(exprStr, scope, outError);
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
    // Position - JSON 单位为 cm，乘以 0.01f 转换为引擎单位（米）
    static constexpr float CM_TO_M = 0.01f;
    float px = ResolveValue(transformExpr.positionX, scope, outError) * CM_TO_M;
    float py = ResolveValue(transformExpr.positionY, scope, outError) * CM_TO_M;
    float pz = ResolveValue(transformExpr.positionZ, scope, outError) * CM_TO_M;
    outPosition = Vector3(px, py, pz);
    
    if (px != 0.0f || py != 0.0f || pz != 0.0f) {
        MOON_LOG_INFO("CSGBuilder", "Resolved position: (%.4f, %.4f, %.4f) m", px, py, pz);
    }

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
