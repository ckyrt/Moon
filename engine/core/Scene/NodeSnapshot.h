#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "../Math/Vector3.h"
#include "../Math/Quaternion.h"

namespace Moon {

/**
 * @brief 组件快照数据（通用基类）
 * 
 * 每种组件类型继承此类，添加自己的数据
 */
struct ComponentSnapshot {
    std::string type;           // 组件类型 (例如 "MeshRenderer", "RigidBody")
    bool enabled = true;        // 是否启用
    
    virtual ~ComponentSnapshot() = default;
};

/**
 * @brief MeshRenderer 组件快照
 */
struct MeshRendererSnapshot : public ComponentSnapshot {
    std::string meshType;       // "cube", "sphere", "cylinder", "plane", "custom"
    bool visible = true;        // 是否可见
    
    // TODO: 材质数据（暂时预留）
    // MaterialData material;
    
    MeshRendererSnapshot() {
        type = "MeshRenderer";
    }
};

/**
 * @brief RigidBody 组件快照
 */
struct RigidBodySnapshot : public ComponentSnapshot {
    float mass = 1.0f;              // 质量
    std::string shapeType;          // "Box", "Sphere", "Capsule", "Cylinder"
    Vector3 size;                   // 形状尺寸
    
    // 运行时状态（可选，根据需求决定是否保存）
    // Vector3 linearVelocity;
    // Vector3 angularVelocity;
    
    RigidBodySnapshot() {
        type = "RigidBody";
    }
};

/**
 * @brief Transform 快照
 */
struct TransformSnapshot {
    Vector3 position;       // 本地位置
    Quaternion rotation;    // 本地旋转
    Vector3 scale;          // 本地缩放
    
    TransformSnapshot()
        : position(0.0f, 0.0f, 0.0f)
        , rotation(0.0f, 0.0f, 0.0f, 1.0f)
        , scale(1.0f, 1.0f, 1.0f)
    {}
};

/**
 * @brief 节点快照（完整的节点状态）
 * 
 * 用途：
 * - Delete Undo（保存被删除节点及子树）
 * - Scene Save/Load（序列化整个场景）
 * - Prefab 系统（保存节点模板）
 * 
 * 设计原则：
 * - 纯数据结构，不包含逻辑
 * - 支持递归（children 保存完整子树）
 * - 与渲染、物理系统解耦
 */
struct NodeSnapshot {
    // === 基础信息 ===
    uint32_t nodeId;                    // 节点 ID（Undo 时保持不变）
    std::string name;                   // 节点名称
    uint32_t parentId;                  // 父节点 ID（0 表示根节点）
    bool active = true;                 // 是否激活
    
    // === Transform ===
    TransformSnapshot transform;        // 变换数据
    
    // === 节点类型 ===
    std::string nodeType;               // "Empty", "Mesh", "Light", "Camera" 等
    
    // === 组件列表 ===
    std::vector<std::shared_ptr<ComponentSnapshot>> components;
    
    // === 子节点（递归结构）===
    std::vector<NodeSnapshot> children;
    
    NodeSnapshot()
        : nodeId(0)
        , parentId(0)
        , active(true)
        , nodeType("Empty")
    {}
};

} // namespace Moon
