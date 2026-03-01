#include "RelationshipResolver.h"
#include "Blueprint.h"
#include "../Logging/Logger.h"

namespace Moon {
namespace CSG {

RelationshipResolver::RelationshipResolver() {
}

RelationshipResolver::~RelationshipResolver() {
}

std::unique_ptr<Blueprint> RelationshipResolver::Resolve(const Blueprint* input, std::string& outError) {
    if (!input) {
        outError = "Input blueprint is null";
        return nullptr;
    }

    // Phase A: 扫描所有 Host 关系
    std::vector<HostRelation> hostRelations;
    // TODO: 实现收集逻辑
    
    // Phase B: 克隆原 Blueprint 并修改结构
    auto output = std::make_unique<Blueprint>(*input);
    
    // TODO: 对每个 Host 关系，修改树结构：
    // 1. 找到宿主节点（如 wall）
    // 2. 替换为 CSG subtract 节点：
    //    subtract {
    //      left: 原宿主节点
    //      right: union { 所有开洞 }
    //    }
    // 3. 保持客体节点（如 window），但移除 cut_opening 标记
    
    // Phase C: 清理 attach 标记
    // TODO: 遍历所有节点，移除已处理的 cut_opening 标记
    
    return output;
}

void RelationshipResolver::CollectHostRelations(
    const Node* node,
    const std::string& currentPath,
    std::vector<HostRelation>& relations,
    std::string& outError) {
    
    if (!node) return;
    
    // 如果是 Reference 节点且有 Host attach
    if (node->type == NodeType::Reference) {
        const RefNode* ref = node->data.ref;
        if (ref->attach.hasAttach && 
            ref->attach.mode == AttachMode::Host &&
            ref->attach.cutOpening) {
            
            HostRelation rel;
            rel.hostPath = ref->attach.targetPath;
            rel.guestPath = currentPath;
            rel.guestRefId = ref->refId;
            
            // 解析位置（需要参数求值）
            // TODO: 需要访问 ParameterScope
            rel.guestPosition = Vector3(0, 0, 0);
            
            // 推算尺寸
            rel.guestSize = EstimateSize(ref->refId, ref->overrides);
            
            relations.push_back(rel);
            
            MOON_LOG_INFO("RelationshipResolver", 
                "Found Host relation: %s hosts %s (size: %.2f x %.2f x %.2f)",
                rel.hostPath.c_str(), rel.guestPath.c_str(),
                rel.guestSize.x, rel.guestSize.y, rel.guestSize.z);
        }
    }
    
    // 递归处理子节点
    if (node->type == NodeType::Group) {
        const GroupNode* group = node->data.group;
        for (size_t i = 0; i < group->children.size(); i++) {
            std::string childPath = currentPath;
            if (i < group->childNames.size() && !group->childNames[i].empty()) {
                childPath += "/" + group->childNames[i];
            } else {
                childPath += "/[" + std::to_string(i) + "]";
            }
            CollectHostRelations(group->children[i].get(), childPath, relations, outError);
        }
    } else if (node->type == NodeType::Csg) {
        const CsgNode* csg = node->data.csg;
        CollectHostRelations(csg->left.get(), currentPath + "/left", relations, outError);
        CollectHostRelations(csg->right.get(), currentPath + "/right", relations, outError);
    }
}

std::unique_ptr<Node> RelationshipResolver::InsertOpenings(
    const Node* hostNode,
    const std::vector<HostRelation>& openings,
    std::string& outError) {
    
    if (openings.empty()) {
        // 无开洞，直接克隆原节点
        // TODO: 实现 Node 深拷贝
        return nullptr;
    }
    
    // 创建 CSG subtract 节点
    auto csg = std::make_unique<Node>(NodeType::Csg);
    CsgNode* csgData = csg->AsCsg();
    csgData->operation = CsgOp::Subtract;
    
    // Left: 克隆原宿主节点
    // TODO: csgData->left = CloneNode(hostNode);
    
    // Right: 创建所有开洞的并集
    if (openings.size() == 1) {
        // 单个洞：直接创建 cube
        const auto& opening = openings[0];
        auto hole = std::make_unique<Node>(NodeType::Primitive);
        PrimitiveNode* primData = hole->AsPrimitive();
        primData->primitive = PrimitiveType::Cube;
        primData->params["size_x"] = ValueExpr::Constant(opening.guestSize.x);
        primData->params["size_y"] = ValueExpr::Constant(opening.guestSize.y);
        primData->params["size_z"] = ValueExpr::Constant(opening.guestSize.z + 2.0f); // 避免z-fighting
        primData->localTransform.position = opening.guestPosition;
        
        csgData->right = std::move(hole);
        
    } else {
        // 多个洞：创建 Group，然后外面再 Union
        // TODO: 实现多洞逻辑（需要 Multiple CSG support）
        outError = "Multiple openings not yet supported in Phase 2";
        return nullptr;
    }
    
    return csg;
}

Vector3 RelationshipResolver::EstimateSize(
    const std::string& blueprintId,
    const std::unordered_map<std::string, ValueExpr>& overrides) {
    
    // 简化版：假设从 overrides 中读取 w/h/t 或 size_x/size_y/size_z
    Vector3 size(100, 100, 10); // 默认值
    
    auto it_w = overrides.find("w");
    auto it_h = overrides.find("h");
    auto it_t = overrides.find("t");
    
    if (it_w != overrides.end() && it_w->second.kind == ValueExpr::Kind::Constant) {
        size.x = it_w->second.constantValue;
    }
    if (it_h != overrides.end() && it_h->second.kind == ValueExpr::Kind::Constant) {
        size.y = it_h->second.constantValue;
    }
    if (it_t != overrides.end() && it_t->second.kind == ValueExpr::Kind::Constant) {
        size.z = it_t->second.constantValue;
    }
    
    return size;
}

void RelationshipResolver::CleanupAttachFlags(Node* node) {
    if (!node) return;
    
    if (node->type == NodeType::Reference) {
        RefNode* ref = node->data.ref;
        if (ref->attach.cutOpening) {
            ref->attach.cutOpening = false; // 标记已处理
        }
    }
    
    // 递归清理子节点
    if (node->type == NodeType::Group) {
        GroupNode* group = node->data.group;
        for (auto& child : group->children) {
            CleanupAttachFlags(child.get());
        }
    } else if (node->type == NodeType::Csg) {
        CsgNode* csg = node->data.csg;
        CleanupAttachFlags(csg->left.get());
        CleanupAttachFlags(csg->right.get());
    }
}

} // namespace CSG
} // namespace Moon
