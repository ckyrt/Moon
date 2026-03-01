#pragma once

#include "Blueprint.h"
#include <memory>
#include <string>

namespace Moon {
namespace CSG {

/**
 * @brief 关系解析器 (Phase 2 扩展)
 * 
 * 职责：将高层语义关系（Host/Embed/Constraint）展开为纯CSG结构
 * 
 * 输入：Blueprint（可能包含 AttachMode::Host 等语义）
 * 输出：Blueprint（纯 primitive/csg/group/reference，无语义）
 * 
 * 设计原则：
 * - CSGBuilder 永远不知道 Host/Embed 存在
 * - 所有语义关系在此阶段展开为标准CSG操作
 * - 可单独测试和扩展
 * 
 * 示例转换：
 *   Input:  wall + window (attach.mode=Host, cut_opening=true)
 *   Output: (wall - window_hole) + window_frame
 */
class RelationshipResolver {
public:
    RelationshipResolver();
    ~RelationshipResolver();

    /**
     * @brief 解析 Blueprint 中的关系语义，展开为纯CSG结构
     * @param input 原始 Blueprint（可能含语义）
     * @param outError 错误信息
     * @return 展开后的纯CSG Blueprint，失败返回 nullptr
     */
    std::unique_ptr<Blueprint> Resolve(const Blueprint* input, std::string& outError);

private:
    // Phase A: 扫描所有节点，收集 Host 关系
    struct HostRelation {
        std::string hostPath;       // 宿主节点路径（如 "../wall"）
        std::string guestPath;      // 客体节点路径（如 "../window1"）
        std::string guestRefId;     // 客体的 Blueprint ID（用于推算bounds）
        Vector3 guestPosition;      // 客体的世界位置
        Vector3 guestSize;          // 客体的尺寸（用于生成洞）
    };
    
    void CollectHostRelations(const Node* node, 
                             const std::string& currentPath,
                             std::vector<HostRelation>& relations,
                             std::string& outError);

    // Phase B: 为每个宿主生成 CSG subtract 节点
    std::unique_ptr<Node> InsertOpenings(const Node* hostNode,
                                         const std::vector<HostRelation>& openings,
                                         std::string& outError);

    // Phase C: 移除客体节点的 attach.cut_opening 标记（已处理）
    void CleanupAttachFlags(Node* node);

    // 辅助：从 Blueprint 和 overrides 推算 bounding box
    Vector3 EstimateSize(const std::string& blueprintId,
                        const std::unordered_map<std::string, ValueExpr>& overrides);
};

} // namespace CSG
} // namespace Moon
