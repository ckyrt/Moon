#include "CSGComponent.h"
#include "CSGOperations.h"
#include "../Scene/SceneNode.h"
#include "../Logging/Logger.h"
#include "../Math/Matrix4x4.h"
#include <sstream>

namespace Moon {

// ============================================================================
// CSGNode 实现
// ============================================================================

std::shared_ptr<CSGNode> CSGNode::CreateBox(float width, float height, float depth) {
    auto node = std::make_shared<CSGNode>();
    node->operation = CSGOperationType::Primitive;
    node->primitiveType = CSGPrimitiveType::Box;
    node->width = width;
    node->height = height;
    node->depth = depth;
    return node;
}

std::shared_ptr<CSGNode> CSGNode::CreateSphere(float radius, int segments) {
    auto node = std::make_shared<CSGNode>();
    node->operation = CSGOperationType::Primitive;
    node->primitiveType = CSGPrimitiveType::Sphere;
    node->depth = radius;  // 使用 depth 字段存储半径
    node->segments = segments;
    return node;
}

std::shared_ptr<CSGNode> CSGNode::CreateCylinder(float radius, float height, int segments) {
    auto node = std::make_shared<CSGNode>();
    node->operation = CSGOperationType::Primitive;
    node->primitiveType = CSGPrimitiveType::Cylinder;
    node->width = radius;  // 使用 width 字段存储半径
    node->height = height;
    node->segments = segments;
    return node;
}

std::shared_ptr<CSGNode> CSGNode::CreateCone(float radius, float height, int segments) {
    auto node = std::make_shared<CSGNode>();
    node->operation = CSGOperationType::Primitive;
    node->primitiveType = CSGPrimitiveType::Cone;
    node->width = radius;  // 使用 width 字段存储半径
    node->height = height;
    node->segments = segments;
    return node;
}

std::shared_ptr<CSGNode> CSGNode::CreateUnion(
    std::shared_ptr<CSGNode> left, 
    std::shared_ptr<CSGNode> right) {
    auto node = std::make_shared<CSGNode>();
    node->operation = CSGOperationType::Union;
    node->left = left;
    node->right = right;
    
    // 设置子节点的父节点引用
    if (left) left->SetParent(node);
    if (right) right->SetParent(node);
    
    return node;
}

std::shared_ptr<CSGNode> CSGNode::CreateSubtract(
    std::shared_ptr<CSGNode> left, 
    std::shared_ptr<CSGNode> right) {
    auto node = std::make_shared<CSGNode>();
    node->operation = CSGOperationType::Subtract;
    node->left = left;
    node->right = right;
    
    // 设置子节点的父节点引用
    if (left) left->SetParent(node);
    if (right) right->SetParent(node);
    
    return node;
}

std::shared_ptr<CSGNode> CSGNode::CreateIntersect(
    std::shared_ptr<CSGNode> left, 
    std::shared_ptr<CSGNode> right) {
    auto node = std::make_shared<CSGNode>();
    node->operation = CSGOperationType::Intersect;
    node->left = left;
    node->right = right;
    
    // 设置子节点的父节点引用
    if (left) left->SetParent(node);
    if (right) right->SetParent(node);
    
    return node;
}

// ============================================================================
// 批量布尔运算（平衡二叉树构建）
// ============================================================================

/**
 * @brief 通用批量布尔运算辅助函数（递归构建平衡二叉树）
 */
static std::shared_ptr<CSGNode> BuildBalancedTree(
    const std::vector<std::shared_ptr<CSGNode>>& nodes,
    CSGOperationType operation,
    size_t start,
    size_t end) {
    
    if (start >= end) return nullptr;
    if (start + 1 == end) return nodes[start];
    
    // 分治策略：一分为二
    size_t mid = start + (end - start) / 2;
    
    auto left = BuildBalancedTree(nodes, operation, start, mid);
    auto right = BuildBalancedTree(nodes, operation, mid, end);
    
    // 根据操作类型创建节点
    switch (operation) {
        case CSGOperationType::Union:
            return CSGNode::CreateUnion(left, right);
        case CSGOperationType::Subtract:
            return CSGNode::CreateSubtract(left, right);
        case CSGOperationType::Intersect:
            return CSGNode::CreateIntersect(left, right);
        default:
            return nullptr;
    }
}

std::shared_ptr<CSGNode> CSGNode::CreateMultiUnion(
    const std::vector<std::shared_ptr<CSGNode>>& nodes) {
    
    if (nodes.empty()) {
        MOON_LOG_WARN("CSGNode", "CreateMultiUnion: empty node list");
        return nullptr;
    }
    
    if (nodes.size() == 1) {
        return nodes[0];
    }
    
    return BuildBalancedTree(nodes, CSGOperationType::Union, 0, nodes.size());
}

std::shared_ptr<CSGNode> CSGNode::CreateMultiSubtract(
    const std::vector<std::shared_ptr<CSGNode>>& nodes) {
    
    if (nodes.empty()) {
        MOON_LOG_WARN("CSGNode", "CreateMultiSubtract: empty node list");
        return nullptr;
    }
    
    if (nodes.size() == 1) {
        return nodes[0];
    }
    
    // Subtract 操作：nodes[0] - nodes[1] - nodes[2] - ...
    // 策略：先把所有被减数做Union，然后用nodes[0]减去它
    if (nodes.size() == 2) {
        return CreateSubtract(nodes[0], nodes[1]);
    }
    
    // nodes[0] - Union(nodes[1], nodes[2], ..., nodes[n])
    std::vector<std::shared_ptr<CSGNode>> subtrahends(nodes.begin() + 1, nodes.end());
    auto subtrahendUnion = CreateMultiUnion(subtrahends);
    return CreateSubtract(nodes[0], subtrahendUnion);
}

std::shared_ptr<CSGNode> CSGNode::CreateMultiIntersect(
    const std::vector<std::shared_ptr<CSGNode>>& nodes) {
    
    if (nodes.empty()) {
        MOON_LOG_WARN("CSGNode", "CreateMultiIntersect: empty node list");
        return nullptr;
    }
    
    if (nodes.size() == 1) {
        return nodes[0];
    }
    
    return BuildBalancedTree(nodes, CSGOperationType::Intersect, 0, nodes.size());
}

std::shared_ptr<Mesh> CSGNode::GetMesh() const {
    // 如果不需要重新生成，直接返回缓存
    if (!dirty && cachedMesh) {
        return cachedMesh;
    }
    
    // 重新生成
    cachedMesh = GenerateMesh();
    dirty = false;
    
    return cachedMesh;
}

void CSGNode::MarkDirty() {
    dirty = true;
    
    // 向上传播：递归标记父节点为 dirty
    if (auto parentNode = parent.lock()) {
        parentNode->MarkDirty();
    }
}

std::shared_ptr<Mesh> CSGNode::GenerateMesh() const {
    // 基础几何体
    if (operation == CSGOperationType::Primitive) {
        std::shared_ptr<Mesh> mesh;
        
        switch (primitiveType) {
            case CSGPrimitiveType::Box:
                // 直接传递Transform参数给CreateCSGBox，在Manifold层面应用Transform
                // TODO: 旋转参数需要从Quaternion转为欧拉角
                mesh = Moon::CSG::CreateCSGBox(width, height, depth, 
                                               localPosition, Vector3(0,0,0), localScale);
                break;
            case CSGPrimitiveType::Sphere:
                mesh = Moon::CSG::CreateCSGSphere(depth, segments,  // depth = radius
                                                  localPosition, Vector3(0,0,0), localScale);
                break;
            case CSGPrimitiveType::Cylinder:
                mesh = Moon::CSG::CreateCSGCylinder(width, height, segments,  // width = radius
                                                    localPosition, Vector3(0,0,0), localScale);
                break;
            case CSGPrimitiveType::Cone:
                mesh = Moon::CSG::CreateCSGCone(width, height, segments,  // width = radius
                                                localPosition, Vector3(0,0,0), localScale);
                break;
            default:
                MOON_LOG_ERROR("CSGNode", "Unknown primitive type");
                return nullptr;
        }
        
        // 不再需要TransformMeshVertices，Transform已经在Manifold层面应用了
        return mesh;
    }
    
    // 布尔操作
    if (!left || !right) {
        MOON_LOG_ERROR("CSGNode", "Boolean operation missing children");
        return nullptr;
    }
    
    // 使用 GetMesh() 而不是 GenerateMesh()，以利用缓存
    auto leftMesh = left->GetMesh();
    auto rightMesh = right->GetMesh();
    
    if (!leftMesh || !rightMesh) {
        MOON_LOG_ERROR("CSGNode", "Failed to generate child meshes");
        return nullptr;
    }
    
    Moon::CSG::Operation op;
    switch (operation) {
        case CSGOperationType::Union:
            op = Moon::CSG::Operation::Union;
            break;
        case CSGOperationType::Subtract:
            op = Moon::CSG::Operation::Subtract;
            break;
        case CSGOperationType::Intersect:
            op = Moon::CSG::Operation::Intersect;
            break;
        default:
            MOON_LOG_ERROR("CSGNode", "Unknown operation type");
            return nullptr;
    }
    
    return Moon::CSG::PerformBoolean(leftMesh.get(), rightMesh.get(), op);
}

std::string CSGNode::ToString() const {
    std::ostringstream oss;
    
    if (operation == CSGOperationType::Primitive) {
        switch (primitiveType) {
            case CSGPrimitiveType::Box:
                oss << "Box(" << width << "," << height << "," << depth;
                break;
            case CSGPrimitiveType::Sphere:
                oss << "Sphere(" << depth;  // depth = radius
                break;
            case CSGPrimitiveType::Cylinder:
                oss << "Cylinder(" << width << "," << height;  // width = radius
                break;
            case CSGPrimitiveType::Cone:
                oss << "Cone(" << width << "," << height;
                break;
        }
        
        // 添加 Transform 信息（如果不是默认值）
        if (localPosition.x != 0 || localPosition.y != 0 || localPosition.z != 0) {
            oss << " @(" << localPosition.x << "," << localPosition.y << "," << localPosition.z << ")";
        }
        oss << ")";
    } else {
        const char* opName = "Unknown";
        switch (operation) {
            case CSGOperationType::Union: opName = "Union"; break;
            case CSGOperationType::Subtract: opName = "Subtract"; break;
            case CSGOperationType::Intersect: opName = "Intersect"; break;
            default: break;
        }
        
        oss << opName << "(";
        if (left) oss << left->ToString();
        oss << ", ";
        if (right) oss << right->ToString();
        oss << ")";
    }
    
    return oss.str();
}

// ============================================================================
// CSGComponent 实现
// ============================================================================

CSGComponent::CSGComponent(SceneNode* owner)
    : Component(owner) {
}

void CSGComponent::SetCSGTree(std::shared_ptr<CSGNode> root) {
    m_csgRoot = root;
    
    if (root) {
        root->MarkDirty();  // 标记为需要生成
        MOON_LOG_INFO("CSGComponent", "CSG tree set: %s", root->ToString().c_str());
    } else {
        MOON_LOG_WARN("CSGComponent", "CSG tree cleared");
    }
}

std::shared_ptr<Mesh> CSGComponent::GetMesh() const {
    if (!m_csgRoot) {
        return nullptr;
    }
    
    return m_csgRoot->GetMesh();  // 使用 CSGNode 的缓存机制
}

void CSGComponent::MarkDirty() {
    if (m_csgRoot) {
        m_csgRoot->MarkDirty();
        MOON_LOG_INFO("CSGComponent", "Marked dirty for regeneration");
    }
}

std::string CSGComponent::GetCSGDefinition() const {
    if (!m_csgRoot) {
        return "(empty)";
    }
    return m_csgRoot->ToString();
}

bool CSGComponent::IsSimplePrimitive() const {
    return m_csgRoot && m_csgRoot->operation == CSGOperationType::Primitive;
}

bool CSGComponent::GetRootParameters(float& width, float& height, float& depth) const {
    if (!IsSimplePrimitive()) {
        return false;
    }
    
    width = m_csgRoot->width;
    height = m_csgRoot->height;
    depth = m_csgRoot->depth;
    return true;
}

bool CSGComponent::SetRootParameters(float w, float h, float d) {
    if (!IsSimplePrimitive()) {
        return false;
    }
    
    m_csgRoot->width = w;
    m_csgRoot->height = h;
    m_csgRoot->depth = d;
    m_csgRoot->MarkDirty();  // 标记为需要重新生成
    
    MOON_LOG_INFO("CSGComponent", "Parameters updated: %.2f x %.2f x %.2f (marked dirty)", w, h, d);
    
    return true;
}

} // namespace Moon
