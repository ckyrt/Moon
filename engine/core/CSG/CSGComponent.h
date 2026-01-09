#pragma once
#include "../Scene/Component.h"
#include "../Math/Vector3.h"
#include "../Math/Quaternion.h"
#include <memory>
#include <vector>
#include <string>

namespace Moon {

// Forward declarations
class Mesh;

/**
 * @brief CSG 操作类型
 */
enum class CSGOperationType {
    Primitive,   // 基础几何体（叶子节点）
    Union,       // 并集 (A ∪ B)
    Subtract,    // 差集 (A - B)
    Intersect    // 交集 (A ∩ B)
};

/**
 * @brief 基础几何体类型
 */
enum class CSGPrimitiveType {
    Box,
    Sphere,
    Cylinder,
    Cone
};

/**
 * @brief CSG 节点 - 参数化几何定义
 * 
 * 这是一个树形结构：
 * - 叶子节点：基础几何体（Box/Sphere/Cylinder）+ 参数
 * - 内部节点：布尔操作（Union/Subtract/Intersect）+ 子节点
 * 
 * 例如：窗户 = Box(2,3,0.1) - Box(1.5,2.5,0.2)
 *             (外框)         (玻璃洞)
 * 
 * 性能优化：
 * - Dirty Flag：参数变化时标记为 dirty
 * - 延迟生成：只在需要时生成 Mesh
 * - 缓存：避免重复生成
 */
struct CSGNode {
    CSGOperationType operation;
    
    // === 基础几何体参数（仅当 operation == Primitive 时有效）===
    CSGPrimitiveType primitiveType;
    float width = 1.0f;   // Box: 宽度, Cylinder: 半径
    float height = 1.0f;  // Box/Cylinder/Cone: 高度
    float depth = 1.0f;   // Box: 深度, Sphere: 半径
    int segments = 32;    // Sphere/Cylinder/Cone: 分段数
    
    // === Transform 参数（相对于父 CSG 节点）===
    Vector3 localPosition = Vector3(0, 0, 0);        // 本地位置
    Quaternion localRotation = Quaternion();         // 本地旋转
    Vector3 localScale = Vector3(1, 1, 1);           // 本地缩放
    
    // === 布尔操作子节点（仅当 operation != Primitive 时有效）===
    std::shared_ptr<CSGNode> left;   // 左子树
    std::shared_ptr<CSGNode> right;  // 右子树
    
    // === 父节点引用（用于向上传播 Dirty 标记）===
    std::weak_ptr<CSGNode> parent;   // 父节点弱引用（避免循环引用）
    
    // === Dirty Flag 和缓存 ===
    mutable bool dirty = true;                       // 是否需要重新生成
    mutable std::shared_ptr<Mesh> cachedMesh;        // 缓存的 Mesh
    
    // === 构造函数 ===
    
    /**
     * @brief 创建基础几何体节点
     */
    static std::shared_ptr<CSGNode> CreateBox(float width, float height, float depth);
    static std::shared_ptr<CSGNode> CreateSphere(float radius, int segments = 32);
    static std::shared_ptr<CSGNode> CreateCylinder(float radius, float height, int segments = 32);
    static std::shared_ptr<CSGNode> CreateCone(float radius, float height, int segments = 32);
    
    /**
     * @brief 创建布尔操作节点（二元操作）
     */
    static std::shared_ptr<CSGNode> CreateUnion(
        std::shared_ptr<CSGNode> left, 
        std::shared_ptr<CSGNode> right);
    
    static std::shared_ptr<CSGNode> CreateSubtract(
        std::shared_ptr<CSGNode> left, 
        std::shared_ptr<CSGNode> right);
    
    static std::shared_ptr<CSGNode> CreateIntersect(
        std::shared_ptr<CSGNode> left, 
        std::shared_ptr<CSGNode> right);
    
    /**
     * @brief 创建批量布尔操作节点（自动构建平衡二叉树）
     * 
     * 主要用于：
     * - 文件加载/反序列化时重建CSG树（深度优化：O(n) → O(log n)）
     * - 批量导入多个物体
     * 
     * 例如：100个物体
     * - CreateUnion逐个调用 → 深度100层
     * - CreateMultiUnion批量调用 → 深度7层（平衡二叉树）
     * 
     * @param nodes 节点列表
     * @return 合并后的根节点
     */
    static std::shared_ptr<CSGNode> CreateMultiUnion(
        const std::vector<std::shared_ptr<CSGNode>>& nodes);
    
    static std::shared_ptr<CSGNode> CreateMultiSubtract(
        const std::vector<std::shared_ptr<CSGNode>>& nodes);
    
    static std::shared_ptr<CSGNode> CreateMultiIntersect(
        const std::vector<std::shared_ptr<CSGNode>>& nodes);
    
    /**
     * @brief 获取 Mesh（按需生成，使用缓存）
     * @return Mesh 指针
     */
    std::shared_ptr<Mesh> GetMesh() const;
    
    /**
     * @brief 标记为需要重新生成（并向上传播到父节点）
     */
    void MarkDirty();
    
    /**
     * @brief 设置父节点（内部使用）
     */
    void SetParent(std::weak_ptr<CSGNode> parentNode) { parent = parentNode; }
    
    /**
     * @brief 直接生成 Mesh（不使用缓存）
     * @return 生成的 Mesh
     */
    std::shared_ptr<Mesh> GenerateMesh() const;
    
    /**
     * @brief 转为可读字符串（调试用）
     * 例如：Union(Box(2,3,1), Subtract(Sphere(1.5), Cylinder(0.5,2)))
     */
    std::string ToString() const;
};

/**
 * @brief CSG 组件 - 参数化几何组件
 * 
 * 用途：
 * - L2 用户（Asset Builder）创建参数化构件
 * - 存储 CSG 操作树（而不是最终几何体）
 * - 参数修改后可重新生成 Mesh
 * 
 * 工作流程：
 * 1. 用户创建 CSG 操作树（Box - Sphere）
 * 2. CSGComponent 存储这个树
 * 3. 调用 RegenerateMesh() 生成最终 Mesh
 * 4. Mesh 传递给 MeshComponent 用于渲染
 * 5. 用户修改参数（如 Box 宽度）→ 重新生成 Mesh
 * 
 * 对 L1 用户（Builder）完全隐藏：
 * - 他们看到的是"门"、"窗"等语义构件
 * - 他们只能调整暴露的参数（宽度/高度）
 * - 底层的 CSG 操作树对他们不可见
 */
class CSGComponent : public Component {
public:
    /**
     * @brief 构造函数
     * @param owner 拥有此组件的场景节点
     */
    explicit CSGComponent(SceneNode* owner);
    
    /**
     * @brief 析构函数
     */
    ~CSGComponent() override = default;

    // === CSG 树管理 ===
    
    /**
     * @brief 设置 CSG 操作树
     * @param root CSG 树的根节点
     */
    void SetCSGTree(std::shared_ptr<CSGNode> root);
    
    /**
     * @brief 获取 CSG 操作树
     */
    std::shared_ptr<CSGNode> GetCSGTree() const { return m_csgRoot; }
    
    /**
     * @brief 获取 Mesh（按需生成）
     * 
     * 使用 dirty flag 机制，只在需要时重新生成
     * @return Mesh 指针
     */
    std::shared_ptr<Mesh> GetMesh() const;
    
    /**
     * @brief 标记为需要重新生成（参数修改后调用）
     */
    void MarkDirty();
    
    /**
     * @brief 获取 CSG 定义字符串（调试用）
     */
    std::string GetCSGDefinition() const;

    // === 参数快捷访问（用于 Inspector UI）===
    
    /**
     * @brief 是否是简单基础几何体（单节点）
     */
    bool IsSimplePrimitive() const;
    
    /**
     * @brief 获取根节点参数（仅当是简单几何体时有效）
     */
    bool GetRootParameters(float& width, float& height, float& depth) const;
    
    /**
     * @brief 设置根节点参数（仅当是简单几何体时有效）
     * @return 是否设置成功
     */
    bool SetRootParameters(float width, float height, float depth);

private:
    std::shared_ptr<CSGNode> m_csgRoot;           // CSG 操作树根节点
};

} // namespace Moon
