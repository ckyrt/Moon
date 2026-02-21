#pragma once

#include "../Mesh/Mesh.h"
#include "../Math/Vector3.h"
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
 * @brief 将Mesh转换为FlatShading（硬边效果）
 * 
 * @param mesh 输入Mesh
 * @return std::shared_ptr<Mesh> 转换后的Mesh，失败返回 nullptr
 */
std::shared_ptr<Mesh> ConvertToFlatShading(const Mesh* mesh);

/**
 * @brief 创建 CSG 立方体（带世界坐标Transform）
 * 
 * @param width 宽度（X 轴）
 * @param height 高度（Y 轴）
 * @param depth 深度（Z 轴）
 * @param position 世界坐标位置（默认原点）
 * @param rotation 旋转（欧拉角，度，默认无旋转）
 * @param scale 缩放（默认1）
 * @return std::shared_ptr<Mesh> 生成的 Mesh（已应用Transform）
 */
std::shared_ptr<Mesh> CreateCSGBox(float width, float height, float depth,
                                   const Vector3& position = Vector3(0, 0, 0),
                                   const Vector3& rotation = Vector3(0, 0, 0),
                                   const Vector3& scale = Vector3(1, 1, 1),
                                   bool flatShading = true);

/**
 * @brief 创建 CSG 球体（带世界坐标Transform）
 * 
 * @param radius 半径
 * @param segments 细分数（默认 32，越大越平滑）
 * @param position 世界坐标位置（默认原点）
 * @param rotation 旋转（欧拉角，度，默认无旋转）
 * @param scale 缩放（默认1）
 * @return std::shared_ptr<Mesh> 生成的 Mesh（已应用Transform）
 */
std::shared_ptr<Mesh> CreateCSGSphere(float radius, int segments = 32,
                                      const Vector3& position = Vector3(0, 0, 0),
                                      const Vector3& rotation = Vector3(0, 0, 0),
                                      const Vector3& scale = Vector3(1, 1, 1),
                                      bool flatShading = true);

/**
 * @brief 创建 CSG 圆柱体（带世界坐标Transform）
 * 
 * @param radius 半径
 * @param height 高度
 * @param segments 圆周细分数（默认 32）
 * @param position 世界坐标位置（默认原点）
 * @param rotation 旋转（欧拉角，度，默认无旋转）
 * @param scale 缩放（默认1）
 * @return std::shared_ptr<Mesh> 生成的 Mesh（已应用Transform）
 */
std::shared_ptr<Mesh> CreateCSGCylinder(float radius, float height, int segments = 32,
                                        const Vector3& position = Vector3(0, 0, 0),
                                        const Vector3& rotation = Vector3(0, 0, 0),
                                        const Vector3& scale = Vector3(1, 1, 1),
                                        bool flatShading = true);

/**
 * @brief 创建 CSG 圆锥体（带世界坐标Transform）
 * 
 * @param radius 底面半径
 * @param height 高度
 * @param segments 圆周细分数（默认 32）
 * @param position 世界坐标位置（默认原点）
 * @param rotation 旋转（欧拉角，度，默认无旋转）
 * @param scale 缩放（默认1）
 * @return std::shared_ptr<Mesh> 生成的 Mesh（已应用Transform）
 */
std::shared_ptr<Mesh> CreateCSGCone(float radius, float height, int segments = 32,
                                    const Vector3& position = Vector3(0, 0, 0),
                                    const Vector3& rotation = Vector3(0, 0, 0),
                                    const Vector3& scale = Vector3(1, 1, 1),
                                    bool flatShading = true);

} // namespace CSG
} // namespace Moon
