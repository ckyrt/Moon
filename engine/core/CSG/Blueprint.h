#pragma once

#include "BlueprintTypes.h"
#include <string>
#include <array>
#include <memory>
#include <unordered_map>

namespace Moon {
namespace CSG {

/**
 * @brief Blueprint - 可复用的"物体配方"
 * 
 * 包含参数定义和 Node 树结构
 */
class Blueprint {
public:
    Blueprint();
    ~Blueprint();

    // 元数据访问
    const std::string& GetId() const { return m_metadata.id; }
    const std::string& GetCategory() const { return m_metadata.category; }
    const std::vector<std::string>& GetTags() const { return m_metadata.tags; }
    int GetSchemaVersion() const { return m_metadata.schemaVersion; }

    // 设置元数据
    void SetId(const std::string& id) { m_metadata.id = id; }
    void SetCategory(const std::string& category) { m_metadata.category = category; }
    void SetTags(const std::vector<std::string>& tags) { m_metadata.tags = tags; }
    void SetSchemaVersion(int version) { m_metadata.schemaVersion = version; }

    // 参数系统
    void AddParameter(const std::string& name, const ParameterDef& def);
    bool HasParameter(const std::string& name) const;
    const ParameterDef* GetParameter(const std::string& name) const;
    const std::unordered_map<std::string, ParameterDef>& GetParameters() const { return m_parameters; }

    // Anchor 系统
    // anchors 存储为表达式字符串三元组 [x, y, z]，在 Build 时用 ParameterScope 求值
    using AnchorExpr = std::array<std::string, 3>;  // [x_expr, y_expr, z_expr]
    void AddAnchor(const std::string& name, const AnchorExpr& expr) { m_anchors[name] = expr; }
    bool HasAnchor(const std::string& name) const { return m_anchors.count(name) > 0; }
    const std::unordered_map<std::string, AnchorExpr>& GetAnchors() const { return m_anchors; }

    // Node 树
    void SetRootNode(std::unique_ptr<Node> root);
    const Node* GetRootNode() const { return m_rootNode.get(); }
    Node* GetRootNode() { return m_rootNode.get(); }

    // 验证
    bool Validate(std::string& outError) const;

private:
    BlueprintMetadata m_metadata;
    std::unordered_map<std::string, ParameterDef> m_parameters;
    std::unordered_map<std::string, std::array<std::string, 3>> m_anchors;  // name -> [x,y,z] expr
    std::unique_ptr<Node> m_rootNode;
};

/**
 * @brief Blueprint Database - 管理所有加载的 Blueprint
 */
class BlueprintDatabase {
public:
    BlueprintDatabase();
    ~BlueprintDatabase();

    /**
     * @brief 加载单个 Blueprint JSON 文件
     */
    bool LoadBlueprint(const std::string& filepath, std::string& outError);

    /**
     * @brief 加载索引文件 index.json
     */
    bool LoadIndex(const std::string& indexPath, std::string& outError);

    /**
     * @brief 根据 ID 获取 Blueprint
     */
    const Blueprint* GetBlueprint(const std::string& id) const;

    /**
     * @brief 检查 Blueprint 是否存在
     */
    bool HasBlueprint(const std::string& id) const;

    /**
     * @brief 获取所有 Blueprint ID
     */
    std::vector<std::string> GetAllIds() const;

    /**
     * @brief 清空数据库
     */
    void Clear();

private:
    std::unordered_map<std::string, std::unique_ptr<Blueprint>> m_blueprints;
    std::unordered_map<std::string, std::string> m_idToPath; // id -> filepath 映射
};

} // namespace CSG
} // namespace Moon
