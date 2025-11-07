#pragma once
#include "../Mesh/Mesh.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Moon {

// Forward declaration
struct Vector3;

/**
 * @brief Mesh 资源管理器
 * 
 * 职责：
 * - 统一管理所有 Mesh 的生命周期
 * - 提供便捷的 Mesh 创建接口
 * - 缓存和复用 Mesh 资源（可选）
 * - 自动引用计数管理（通过 shared_ptr）
 * 
 * 使用示例：
 * ```cpp
 * MeshManager* manager = engine->GetMeshManager();
 * 
 * // 创建 Mesh（自动管理生命周期）
 * auto cubeMesh = manager->CreateCube(1.0f, Vector3(1,0,0));
 * auto sphereMesh = manager->CreateSphere(0.5f);
 * 
 * // 多个对象可以共享同一个 Mesh
 * meshRenderer1->SetMesh(cubeMesh);
 * meshRenderer2->SetMesh(cubeMesh);  // 复用
 * ```
 */
class MeshManager {
public:
    MeshManager() = default;
    ~MeshManager();
    
    // 禁止拷贝
    MeshManager(const MeshManager&) = delete;
    MeshManager& operator=(const MeshManager&) = delete;
    
    // === 基础几何体创建接口 ===
    
    /**
     * @brief 创建立方体 Mesh
     * @param size 立方体边长
     * @param color 顶点颜色
     * @return shared_ptr<Mesh> 智能指针（自动管理生命周期）
     */
    std::shared_ptr<Mesh> CreateCube(float size = 1.0f, const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建球体 Mesh
     * @param radius 球体半径
     * @param segments 经度细分数（默认24）
     * @param rings 纬度细分数（默认16）
     * @param color 顶点颜色
     * @return shared_ptr<Mesh>
     */
    std::shared_ptr<Mesh> CreateSphere(float radius = 0.5f, int segments = 24, int rings = 16, 
                                       const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建平面网格 Mesh
     * @param width X轴宽度
     * @param depth Z轴深度
     * @param subdivisionsX X方向细分数
     * @param subdivisionsZ Z方向细分数
     * @param color 顶点颜色
     * @return shared_ptr<Mesh>
     */
    std::shared_ptr<Mesh> CreatePlane(float width = 1.0f, float depth = 1.0f, 
                                      int subdivisionsX = 1, int subdivisionsZ = 1,
                                      const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建圆柱体 Mesh
     * @param radiusTop 顶部半径
     * @param radiusBottom 底部半径
     * @param height 高度
     * @param segments 圆周细分数
     * @param color 顶点颜色
     * @return shared_ptr<Mesh>
     */
    std::shared_ptr<Mesh> CreateCylinder(float radiusTop = 0.5f, float radiusBottom = 0.5f, 
                                         float height = 1.0f, int segments = 24,
                                         const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建圆锥体 Mesh
     * @param radius 底面半径
     * @param height 高度
     * @param segments 圆周细分数
     * @param color 顶点颜色
     * @return shared_ptr<Mesh>
     */
    std::shared_ptr<Mesh> CreateCone(float radius = 0.5f, float height = 1.0f, int segments = 24,
                                     const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建圆环体 Mesh
     * @param majorRadius 大圆半径
     * @param minorRadius 小圆半径
     * @param majorSegments 大圆细分数
     * @param minorSegments 小圆细分数
     * @param color 顶点颜色
     * @return shared_ptr<Mesh>
     */
    std::shared_ptr<Mesh> CreateTorus(float majorRadius = 0.5f, float minorRadius = 0.2f, 
                                      int majorSegments = 24, int minorSegments = 12,
                                      const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建胶囊体 Mesh
     * @param radius 半径
     * @param height 总高度
     * @param segments 圆周细分数
     * @param rings 半球纬度细分数
     * @param color 顶点颜色
     * @return shared_ptr<Mesh>
     */
    std::shared_ptr<Mesh> CreateCapsule(float radius = 0.5f, float height = 2.0f, 
                                        int segments = 16, int rings = 8,
                                        const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建四边形面片 Mesh
     * @param width X轴宽度
     * @param height Y轴高度
     * @param color 顶点颜色
     * @return shared_ptr<Mesh>
     */
    std::shared_ptr<Mesh> CreateQuad(float width = 1.0f, float height = 1.0f,
                                     const Vector3& color = Vector3(1, 1, 1));
    
    // === 资源管理接口 ===
    
    /**
     * @brief 清理所有 Mesh 资源
     * 
     * 注意：如果外部仍持有 shared_ptr，资源不会立即释放
     */
    void Clear();
    
    /**
     * @brief 获取当前管理的 Mesh 数量
     */
    size_t GetMeshCount() const { return m_meshes.size(); }
    
    /**
     * @brief 通过名称查找 Mesh（如果之前通过 RegisterMesh 注册过）
     * @param name Mesh 名称
     * @return shared_ptr<Mesh> 如果未找到返回 nullptr
     */
    std::shared_ptr<Mesh> GetMesh(const std::string& name) const;
    
    /**
     * @brief 注册一个命名的 Mesh（用于缓存和查找）
     * @param name Mesh 名称
     * @param mesh Mesh 智能指针
     */
    void RegisterMesh(const std::string& name, std::shared_ptr<Mesh> mesh);

private:
    // 所有创建的 Mesh 列表（用于统计和清理）
    std::vector<std::shared_ptr<Mesh>> m_meshes;
    
    // 命名的 Mesh 映射（用于缓存和查找）
    std::unordered_map<std::string, std::shared_ptr<Mesh>> m_namedMeshes;
};

} // namespace Moon
