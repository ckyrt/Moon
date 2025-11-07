#pragma once
#include "../Mesh/Mesh.h"
#include "../Camera/Camera.h"  // For Vector3

namespace Moon {

/**
 * @brief Mesh 几何生成器 - 创建基础几何体
 * 
 * 支持的几何体类型（类似 Unity Primitives）：
 * - Cube       - 立方体
 * - Sphere     - UV球体（经纬度细分）
 * - Plane      - 平面网格
 * - Cylinder   - 圆柱体
 * - Cone       - 圆锥体
 * - Torus      - 圆环体
 * - Capsule    - 胶囊体
 * - Quad       - 四边形面片
 */
class MeshGenerator {
public:
    /**
     * @brief 创建立方体 Mesh
     * @param size 立方体边长（从中心到面的距离为 size/2）
     * @param color 顶点颜色（默认白色）
     * @return 生成的 Mesh 对象（调用者负责释放）
     */
    static Mesh* CreateCube(float size = 1.0f, const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建 UV 球体（经纬度细分）
     * @param radius 球体半径
     * @param segments 经度细分数（最小3，推荐16-32）
     * @param rings 纬度细分数（最小2，推荐8-16）
     * @param color 顶点颜色
     * @return 生成的 Mesh 对象
     * 
     * 原理：
     * - 使用球面坐标系 (θ, φ) 生成顶点
     * - θ: 经度角 [0, 2π], φ: 纬度角 [0, π]
     * - x = r·sin(φ)·cos(θ), y = r·cos(φ), z = r·sin(φ)·sin(θ)
     */
    static Mesh* CreateSphere(float radius = 0.5f, int segments = 24, int rings = 16, 
                             const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建平面网格（XZ 平面，法线向上）
     * @param width X轴宽度
     * @param depth Z轴深度
     * @param subdivisionsX X方向细分数（最小1）
     * @param subdivisionsZ Z方向细分数（最小1）
     * @param color 顶点颜色
     * @return 生成的 Mesh 对象
     * 
     * 平面中心在原点，Y=0，法线朝向 +Y
     */
    static Mesh* CreatePlane(float width = 1.0f, float depth = 1.0f, 
                            int subdivisionsX = 1, int subdivisionsZ = 1, 
                            const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建圆柱体（Y轴向上）
     * @param radiusTop 顶部半径
     * @param radiusBottom 底部半径
     * @param height 高度（沿Y轴）
     * @param segments 圆周细分数（最小3）
     * @param color 顶点颜色
     * @return 生成的 Mesh 对象
     * 
     * 圆柱体中心在原点，底面 y = -height/2, 顶面 y = height/2
     */
    static Mesh* CreateCylinder(float radiusTop = 0.5f, float radiusBottom = 0.5f, 
                               float height = 1.0f, int segments = 24, 
                               const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建圆锥体（底部封闭）
     * @param radius 底面半径
     * @param height 高度
     * @param segments 圆周细分数
     * @param color 顶点颜色
     * @return 生成的 Mesh 对象
     * 
     * 圆锥体底面中心在原点下方，顶点在 y = height
     */
    static Mesh* CreateCone(float radius = 0.5f, float height = 1.0f, int segments = 24, 
                           const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建圆环体（甜甜圈）
     * @param majorRadius 大圆半径（圆环中心到管子中心的距离）
     * @param minorRadius 小圆半径（管子的半径）
     * @param majorSegments 大圆细分数（最小3，推荐16-32）
     * @param minorSegments 小圆细分数（最小3，推荐8-16）
     * @param color 顶点颜色
     * @return 生成的 Mesh 对象
     * 
     * 圆环位于 XZ 平面，管子沿圆周分布
     */
    static Mesh* CreateTorus(float majorRadius = 0.5f, float minorRadius = 0.2f, 
                            int majorSegments = 24, int minorSegments = 12, 
                            const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建胶囊体（两端半球 + 中间圆柱）
     * @param radius 半径
     * @param height 总高度（包含两端半球）
     * @param segments 圆周细分数
     * @param rings 半球纬度细分数
     * @param color 顶点颜色
     * @return 生成的 Mesh 对象
     * 
     * 胶囊体中心在原点，Y轴向上
     */
    static Mesh* CreateCapsule(float radius = 0.5f, float height = 2.0f, 
                              int segments = 16, int rings = 8, 
                              const Vector3& color = Vector3(1, 1, 1));
    
    /**
     * @brief 创建四边形面片（单个平面，两个三角形）
     * @param width X轴宽度
     * @param height Y轴高度
     * @param color 顶点颜色
     * @return 生成的 Mesh 对象
     * 
     * Quad 位于 XY 平面，中心在原点，法线朝向 +Z
     */
    static Mesh* CreateQuad(float width = 1.0f, float height = 1.0f, 
                           const Vector3& color = Vector3(1, 1, 1));

private:
    // 辅助函数：计算球面坐标
    static Vector3 SphericalToCartesian(float radius, float theta, float phi);
    
    // 辅助函数：生成圆周顶点
    static void GenerateCircleVertices(std::vector<Vertex>& vertices, 
                                      float radius, float y, int segments, 
                                      const Vector3& color);
};

} // namespace Moon
