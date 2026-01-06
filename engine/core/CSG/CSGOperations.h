#pragma once

#include "../Mesh/Mesh.h"
#include <memory>

namespace Moon {
namespace CSG {

/**
 * @brief CSG 布尔运算类型
 */
enum class Operation {
    Union,      // 并集 A ∪ B
    Subtract,   // 差集 A - B
    Intersect   // 交集 A ∩ B
};

/**
 * @brief 对两个 Mesh 执行 CSG 布尔运算
 * 
 * @param meshA 第一个 Mesh
 * @param meshB 第二个 Mesh
 * @param op 运算类型
 * @return std::shared_ptr<Mesh> 结果 Mesh，失败返回 nullptr
 */
std::shared_ptr<Mesh> PerformBoolean(
    const Mesh* meshA,
    const Mesh* meshB,
    Operation op
);

/**
 * @brief 创建 CSG 立方体
 * 
 * @param width 宽度（X 轴）
 * @param height 高度（Y 轴）
 * @param depth 深度（Z 轴）
 * @return std::shared_ptr<Mesh> 生成的 Mesh
 */
std::shared_ptr<Mesh> CreateCSGBox(float width, float height, float depth);

/**
 * @brief 创建 CSG 球体
 * 
 * @param radius 半径
 * @param segments 细分数（默认 32，越大越平滑）
 * @return std::shared_ptr<Mesh> 生成的 Mesh
 */
std::shared_ptr<Mesh> CreateCSGSphere(float radius, int segments = 32);

/**
 * @brief 创建 CSG 圆柱体
 * 
 * @param radius 半径
 * @param height 高度
 * @param segments 圆周细分数（默认 32）
 * @return std::shared_ptr<Mesh> 生成的 Mesh
 */
std::shared_ptr<Mesh> CreateCSGCylinder(float radius, float height, int segments = 32);

} // namespace CSG
} // namespace Moon
