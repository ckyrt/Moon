#pragma once

#include "BlueprintTypes.h"
#include "Blueprint.h"
#include "../Mesh/Mesh.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace Moon {
namespace CSG {

/**
 * @brief 参数作用域 - 管理参数值的层级查找
 */
class ParameterScope {
public:
    ParameterScope() : m_parent(nullptr) {}
    explicit ParameterScope(ParameterScope* parent) : m_parent(parent) {}

    /**
     * @brief 设置参数值
     */
    void SetValue(const std::string& name, float value) {
        m_values[name] = value;
    }

    /**
     * @brief 查找参数值（支持向父作用域查找）
     * @return 找到返回 true，值存入 outValue
     */
    bool GetValue(const std::string& name, float& outValue) const {
        auto it = m_values.find(name);
        if (it != m_values.end()) {
            outValue = it->second;
            return true;
        }
        
        // 向父作用域查找
        if (m_parent) {
            return m_parent->GetValue(name, outValue);
        }
        
        return false;
    }

    /**
     * @brief 创建子作用域
     */
    ParameterScope CreateChild() {
        return ParameterScope(this);
    }

private:
    ParameterScope* m_parent;
    std::unordered_map<std::string, float> m_values;
};

/**
 * @brief 解析后的 Transform (实际的 position/rotation/scale 值)
 */
struct ResolvedTransform {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;

    ResolvedTransform()
        : position(0, 0, 0)
        , rotation(Quaternion::Identity())
        , scale(1, 1, 1)
    {}

    ResolvedTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scl)
        : position(pos), rotation(rot), scale(scl)
    {}
};

/**
 * @brief 构建结果 - 单个 Mesh 项
 */
struct MeshItem {
    std::shared_ptr<Mesh> mesh;
    std::string material;
    ResolvedTransform worldTransform;

    MeshItem() = default;
    MeshItem(std::shared_ptr<Mesh> m, const std::string& mat, const ResolvedTransform& trans)
        : mesh(m), material(mat), worldTransform(trans) {}
};

/**
 * @brief 构建结果 - 可包含多个 Mesh
 */
struct BuildResult {
    std::vector<MeshItem> meshes;

    void AddMesh(const MeshItem& item) {
        meshes.push_back(item);
    }

    void Merge(BuildResult&& other) {
        meshes.insert(meshes.end(), 
            std::make_move_iterator(other.meshes.begin()),
            std::make_move_iterator(other.meshes.end()));
    }
};

/**
 * @brief CSG Builder - 根据 Blueprint 构建 Mesh
 */
class CSGBuilder {
public:
    CSGBuilder();
    ~CSGBuilder();

    /**
     * @brief 设置 Blueprint 数据库（用于 Reference 节点）
     */
    void SetBlueprintDatabase(BlueprintDatabase* db) {
        m_database = db;
    }

    /**
     * @brief 构建 Blueprint
     * @param blueprint Blueprint 对象
     * @param parameterOverrides 参数覆盖值
     * @param outError 错误信息
     * @return 构建结果
     */
    BuildResult Build(const Blueprint* blueprint, 
                      const std::unordered_map<std::string, float>& parameterOverrides,
                      std::string& outError);

private:
    // 构建单个节点
    BuildResult BuildNode(const Node* node, ParameterScope& scope, std::string& outError);

    // 构建基础几何体
    BuildResult BuildPrimitive(const PrimitiveNode* prim, ParameterScope& scope, std::string& outError);

    // 执行 CSG 运算
    BuildResult BuildCSG(const CsgNode* csg, ParameterScope& scope, std::string& outError);

    // 构建 Group
    BuildResult BuildGroup(const GroupNode* group, ParameterScope& scope, std::string& outError);

    // 构建 Reference（内部版本，返回结果 + 求值后的 anchor 坐标）
    BuildResult BuildReference(const RefNode* ref, ParameterScope& scope, std::string& outError);

    // 对 blueprint 的 anchors 用指定 scope 求值，返回 name -> Vector3
    // CM_TO_M 会在此处应用（anchors 坐标单位为 cm）
    std::unordered_map<std::string, Vector3> EvaluateAnchors(
        const Blueprint* blueprint, ParameterScope& scope, std::string& outError);

    // 解析单个表达式字符串（用于 anchor 坐标）
    float EvaluateStringExpr(const std::string& exprStr, ParameterScope& scope, std::string& outError);

    // 解析值表达式
    float ResolveValue(const ValueExpr& expr, ParameterScope& scope, std::string& outError);

    // 计算表达式字符串（支持 +, -, *, / 和 $param 引用）
    float EvaluateExpression(const std::string& exprStr, ParameterScope& scope, std::string& outError);

    // 解析 Transform（将 ValueExpr 转换为实际值）
    void ResolveTransform(const TransformTRS& transformExpr, ParameterScope& scope,
                          Vector3& outPosition, Quaternion& outRotation, Vector3& outScale,
                          std::string& outError);

    // Blueprint 数据库（用于 Reference）
    BlueprintDatabase* m_database;

    // 递归构建深度计数（0 = 最外层，>0 = 被 reference 调用）
    // 用于避免对已经 FlatShading 的 mesh 重复转换
    int m_buildDepth = 0;
};

} // namespace CSG
} // namespace Moon
