#include "CSGOperations.h"
#include "../Logging/Logger.h"

// Manifold 库头文件
#include "manifold/manifold.h"

#include <vector>

namespace Moon {
namespace CSG {

static manifold::Manifold MeshToManifold(const Mesh* mesh) {
    if (!mesh || !mesh->IsValid()) {
        MOON_LOG_ERROR("CSG", "Invalid input mesh");
        return manifold::Manifold();
    }

    const std::vector<Vertex>& vertices = mesh->GetVertices();
    const std::vector<uint32_t>& indices = mesh->GetIndices();

    manifold::MeshGL meshGL;
    meshGL.numProp = 3;
    meshGL.vertProperties.reserve(vertices.size() * 3);
    // 反向坐标转换：Engine (Y-up) → Manifold (Z-up)
    // Engine (x, y, z) → Manifold (x, -z, y)
    for (const auto& v : vertices) {
        meshGL.vertProperties.push_back(v.position.x);    // Engine X → Manifold X
        meshGL.vertProperties.push_back(-v.position.z);   // Engine -Z → Manifold Y  
        meshGL.vertProperties.push_back(v.position.y);    // Engine Y → Manifold Z
    }

    meshGL.triVerts.reserve(indices.size());
    for (size_t i = 0; i < indices.size(); i += 3) {
        meshGL.triVerts.push_back(indices[i + 0]);
        meshGL.triVerts.push_back(indices[i + 1]);
        meshGL.triVerts.push_back(indices[i + 2]);
    }

    manifold::Manifold result(meshGL);
    if (result.IsEmpty()) {
        MOON_LOG_ERROR("CSG", "Failed to create manifold: %zu verts, %zu tris", 
                      vertices.size(), indices.size() / 3);
    }
    return result;
}

// 内部函数：保留拓扑结构的转换（用于布尔运算中间步骤）
static std::shared_ptr<Mesh> ManifoldToMesh_PreserveTopology(const manifold::Manifold& manifold) {
    if (manifold.IsEmpty()) return nullptr;

    manifold::MeshGL meshGL = manifold.GetMeshGL();
    std::vector<Vertex> vertices;
    size_t numVerts = meshGL.vertProperties.size() / meshGL.numProp;
    vertices.reserve(numVerts);

    // 保留顶点共享结构（用于布尔运算）
    // ⚠️ Manifold使用Z-up坐标系，引擎使用Y-up，在这里统一转换：
    // Manifold (x, y, z) → Engine (x, z, -y)  即：Z→Y, Y→-Z
    for (size_t i = 0; i < numVerts; ++i) {
        size_t offset = i * meshGL.numProp;
        float mx = meshGL.vertProperties[offset + 0];
        float my = meshGL.vertProperties[offset + 1];
        float mz = meshGL.vertProperties[offset + 2];
        
        Vertex v;
        // 坐标系转换：Manifold的Z轴 → 引擎的Y轴
        v.position.x = mx;
        v.position.y = mz;  // Manifold的Z → 引擎的Y
        v.position.z = -my; // Manifold的Y → 引擎的-Z
        v.normal = Vector3(0, 0, 0);  // 稍后计算
        
        v.uv = Vector2(0, 0);
        vertices.push_back(v);
        
        // DEBUG: 输出第一个顶点的转换
        if (i == 0) {
            MOON_LOG_INFO("CSG", "  ManifoldToMesh coord transform: Manifold(%.4f, %.4f, %.4f) → Engine(%.4f, %.4f, %.4f)",
                mx, my, mz, v.position.x, v.position.y, v.position.z);
        }
    }

    std::vector<uint32_t> indices(meshGL.triVerts.begin(), meshGL.triVerts.end());

    // 计算平滑法线（顶点共享，法线平均）
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i + 0];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        
        Vector3 v0 = vertices[i0].position;
        Vector3 v1 = vertices[i1].position;
        Vector3 v2 = vertices[i2].position;
        
        Vector3 edge1(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
        Vector3 edge2(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
        
        Vector3 faceNormal(
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x
        );
        
        vertices[i0].normal.x += faceNormal.x;
        vertices[i0].normal.y += faceNormal.y;
        vertices[i0].normal.z += faceNormal.z;
        
        vertices[i1].normal.x += faceNormal.x;
        vertices[i1].normal.y += faceNormal.y;
        vertices[i1].normal.z += faceNormal.z;
        
        vertices[i2].normal.x += faceNormal.x;
        vertices[i2].normal.y += faceNormal.y;
        vertices[i2].normal.z += faceNormal.z;
    }
    
    // 归一化法线
    // ✅ UV不再需要CPU生成，shader端用triplanar mapping（世界空间实时计算）
    for (auto& v : vertices) {
        // 归一化法线
        float len = sqrtf(v.normal.x * v.normal.x + 
                         v.normal.y * v.normal.y + 
                         v.normal.z * v.normal.z);
        if (len > 0.0001f) {
            v.normal.x /= len;
            v.normal.y /= len;
            v.normal.z /= len;
        } else {
            v.normal = Vector3(0, 1, 0);
        }
        
        // UV保留为(0,0)，triplanar不使用UV
        v.uv = Vector2(0, 0);
    }

    auto mesh = std::make_shared<Mesh>();
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    return mesh;
}

// 最终渲染用：扁平化顶点实现硬边效果
static std::shared_ptr<Mesh> ManifoldToMesh_FlatShading(const manifold::Manifold& manifold) {
    if (manifold.IsEmpty()) return nullptr;

    manifold::MeshGL meshGL = manifold.GetMeshGL();
    
    // 按三角形展开顶点，每个面独立法线（硬边）
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    size_t numTriangles = meshGL.triVerts.size() / 3;
    vertices.reserve(numTriangles * 3);
    indices.reserve(numTriangles * 3);
    
    for (size_t triIdx = 0; triIdx < numTriangles; ++triIdx) {
        uint32_t origIdx0 = meshGL.triVerts[triIdx * 3 + 0];
        uint32_t origIdx1 = meshGL.triVerts[triIdx * 3 + 1];
        uint32_t origIdx2 = meshGL.triVerts[triIdx * 3 + 2];
        
        // 读取Manifold坐标（Z-up）
        float mx0 = meshGL.vertProperties[origIdx0 * meshGL.numProp + 0];
        float my0 = meshGL.vertProperties[origIdx0 * meshGL.numProp + 1];
        float mz0 = meshGL.vertProperties[origIdx0 * meshGL.numProp + 2];
        
        float mx1 = meshGL.vertProperties[origIdx1 * meshGL.numProp + 0];
        float my1 = meshGL.vertProperties[origIdx1 * meshGL.numProp + 1];
        float mz1 = meshGL.vertProperties[origIdx1 * meshGL.numProp + 2];
        
        float mx2 = meshGL.vertProperties[origIdx2 * meshGL.numProp + 0];
        float my2 = meshGL.vertProperties[origIdx2 * meshGL.numProp + 1];
        float mz2 = meshGL.vertProperties[origIdx2 * meshGL.numProp + 2];
        
        // 转换到引擎坐标系（Y-up）: (x, y, z) → (x, z, -y)
        Vector3 p0(mx0, mz0, -my0);
        Vector3 p1(mx1, mz1, -my1);
        Vector3 p2(mx2, mz2, -my2);
        
        Vector3 edge1(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
        Vector3 edge2(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z);
        Vector3 faceNormal(
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x
        );
        
        float len = sqrtf(faceNormal.x * faceNormal.x + 
                         faceNormal.y * faceNormal.y + 
                         faceNormal.z * faceNormal.z);
        if (len > 0.0001f) {
            faceNormal.x /= len;
            faceNormal.y /= len;
            faceNormal.z /= len;
        } else {
            faceNormal = Vector3(0, 1, 0);
        }
        
        Vertex v0, v1, v2;
        v0.position = p0;
        v0.normal = faceNormal;
        v0.uv = Vector2(0, 0);
        
        v1.position = p1;
        v1.normal = faceNormal;
        v1.uv = Vector2(0, 0);
        
        v2.position = p2;
        v2.normal = faceNormal;
        v2.uv = Vector2(0, 0);
        
        uint32_t newIdx = static_cast<uint32_t>(vertices.size());
        vertices.push_back(v0);
        vertices.push_back(v1);
        vertices.push_back(v2);
        
        indices.push_back(newIdx + 0);
        indices.push_back(newIdx + 1);
        indices.push_back(newIdx + 2);
    }

    auto mesh = std::make_shared<Mesh>();
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    return mesh;
}

// 公开接口：默认使用扁平着色（硬边效果）
static std::shared_ptr<Mesh> ManifoldToMesh_NoConversion(const manifold::Manifold& manifold) {
    return ManifoldToMesh_FlatShading(manifold);
}

std::shared_ptr<Mesh> ConvertToFlatShading(const Mesh* mesh) {
    if (!mesh || !mesh->IsValid()) {
        MOON_LOG_ERROR("CSG", "Invalid input mesh for FlatShading conversion");
        return nullptr;
    }

    // 转换为Manifold再转回来，自动应用FlatShading
    manifold::Manifold manifold = MeshToManifold(mesh);
    if (manifold.IsEmpty()) {
        MOON_LOG_ERROR("CSG", "Failed to convert mesh to manifold");
        return nullptr;
    }

    return ManifoldToMesh_FlatShading(manifold);
}

std::shared_ptr<Mesh> PerformBoolean(const Mesh* meshA, const Mesh* meshB, Operation op) {
    if (!meshA || !meshB) {
        MOON_LOG_ERROR("CSG", "Null mesh input");
        return nullptr;
    }

    MOON_LOG_INFO("CSG", "Performing boolean operation...");

    // 转换为 Manifold（需要保留拓扑结构）
    manifold::Manifold manifoldA = MeshToManifold(meshA);
    manifold::Manifold manifoldB = MeshToManifold(meshB);

    if (manifoldA.IsEmpty() || manifoldB.IsEmpty()) {
        MOON_LOG_ERROR("CSG", "Failed to convert meshes to manifold");
        return nullptr;
    }

    // 执行布尔运算
    manifold::Manifold result;
    switch (op) {
        case Operation::Union:
            result = manifoldA + manifoldB;
            MOON_LOG_INFO("CSG", "Union operation completed");
            break;
        case Operation::Subtract:
            result = manifoldA - manifoldB;
            MOON_LOG_INFO("CSG", "Subtract operation completed");
            break;
        case Operation::Intersect:
            result = manifoldA ^ manifoldB;
            MOON_LOG_INFO("CSG", "Intersect operation completed");
            break;
        default:
            MOON_LOG_ERROR("CSG", "Unknown operation type");
            return nullptr;
    }

    // 布尔运算结果保留拓扑结构，以便后续嵌套操作
    // 最终输出时会在CSGBuilder中转换为FlatShading
    return ManifoldToMesh_PreserveTopology(result);
}

/**
 * 创建CSG立方体
 * 
 * 【返回几何体说明 - 引擎坐标系(Y-up)】
 * - 原点位置：几何中心
 * - 示例：CreateCSGBox(1, 2, 3) 返回的立方体范围是
 *   + X: [-0.5, 0.5] (宽度1)
 *   + Y: [-1.0, 1.0] (高度2)
 *   + Z: [-1.5, 1.5] (深度3)
 * 
 * @param width   宽度 (X轴方向)
 * @param height  高度 (Y轴方向，向上)
 * @param depth   深度 (Z轴方向)
 * @param position 世界坐标位置（引擎坐标系），默认(0,0,0)
 * @param rotation 旋转欧拉角(度)，默认无旋转
 * @param scale   缩放系数，默认(1,1,1)
 * @param flatShading 是否使用平面着色（硬边效果），默认true
 */
std::shared_ptr<Mesh> CreateCSGBox(float width, float height, float depth,
                                   const Vector3& position,
                                   const Vector3& rotation,
                                   const Vector3& scale,
                                   bool flatShading) {
    // Manifold是Z-up，引擎是Y-up，所以需要转换参数顺序
    // 引擎(width, height, depth) → Manifold(width, depth, height)
    // 因为引擎Y轴(高度)对应ManifoldZ轴，引擎Z轴(深度)对应ManifoldY轴
    manifold::Manifold box = manifold::Manifold::Cube({width, depth, height}, true);
    
    // 应用用户变换（在Manifold坐标系中）
    if (scale != Vector3(1, 1, 1)) {
        box = box.Scale({scale.x, scale.y, scale.z});
    }
    if (rotation != Vector3(0, 0, 0)) {
        box = box.Rotate(rotation.x, rotation.y, rotation.z);
    }
    if (position != Vector3(0, 0, 0)) {
        // position是引擎坐标系(Y-up)，需要转换到Manifold坐标系(Z-up)
        box = box.Translate({position.x, -position.z, position.y});
    }
    
    return flatShading ? ManifoldToMesh_FlatShading(box) : ManifoldToMesh_PreserveTopology(box);
}

/**
 * 创建CSG球体
 * 
 * 【返回几何体说明 - 引擎坐标系(Y-up)】
 * - 原点位置：球心
 * - 示例：CreateCSGSphere(1.0) 返回的球体范围是
 *   + X: [-1.0, 1.0]
 *   + Y: [-1.0, 1.0]
 *   + Z: [-1.0, 1.0]
 * - 注意：球体旋转无视觉差异
 * 
 * @param radius  球体半径
 * @param segments 圆周细分数，越大越平滑，默认32
 * @param position 世界坐标位置（引擎坐标系），默认(0,0,0)
 * @param rotation 旋转欧拉角(度)，默认无旋转
 * @param scale   缩放系数，默认(1,1,1)
 * @param flatShading 是否使用平面着色（硬边效果），默认true
 */
std::shared_ptr<Mesh> CreateCSGSphere(float radius, int segments,
                                      const Vector3& position,
                                      const Vector3& rotation,
                                      const Vector3& scale,
                                      bool flatShading) {
    manifold::Manifold sphere = manifold::Manifold::Sphere(radius, segments);
    
    if (scale != Vector3(1, 1, 1)) {
        sphere = sphere.Scale({scale.x, scale.y, scale.z});
    }
    if (rotation != Vector3(0, 0, 0)) {
        sphere = sphere.Rotate(rotation.x, rotation.y, rotation.z);
    }
    if (position != Vector3(0, 0, 0)) {
        // position是引擎坐标系(Y-up)，需要转换到Manifold坐标系(Z-up)
        sphere = sphere.Translate({position.x, -position.z, position.y});
    }
    
    return flatShading ? ManifoldToMesh_FlatShading(sphere) : ManifoldToMesh_PreserveTopology(sphere);
}

/**
 * 创建CSG圆柱体
 * 
 * 【返回几何体说明 - 引擎坐标系(Y-up)】
 * - 原点位置：⚠️ 几何中心（已自动居中）
 * - 朝向：沿Y轴向上延伸
 * - 示例：CreateCSGCylinder(0.5, 2.0) 返回的圆柱范围是
 *   + X: [-0.5, 0.5] (直径1.0)
 *   + Y: [-1.0, 1.0] (高度2.0，几何中心在Y=0)
 *   + Z: [-0.5, 0.5] (直径1.0)
 * 
 * @param radius  圆柱半径（底面和顶面相同）
 * @param height  圆柱高度（沿Y轴方向）
 * @param segments 圆周细分数，越大越平滑，默认32
 * @param position 世界坐标位置（引擎坐标系，指定几何中心），默认(0,0,0)
 * @param rotation 旋转欧拉角(度)，默认无旋转
 * @param scale   缩放系数，默认(1,1,1)
 * @param flatShading 是否使用平面着色（硬边效果），默认true
 */
std::shared_ptr<Mesh> CreateCSGCylinder(float radius, float height, int segments,
                                        const Vector3& position,
                                        const Vector3& rotation,
                                        const Vector3& scale,
                                        bool flatShading) {
    // Manifold的Cylinder: 底部在原点(0,0,0)，沿Z轴向上延伸height
    manifold::Manifold cylinder = manifold::Manifold::Cylinder(height, radius, radius, segments);
    
    // 在Manifold坐标系(Z-up)中将圆柱中心移到原点
    // Manifold Cylinder原始范围: Z[0, height]，要让它居中到Z[-height/2, height/2]
    // 所以需要向下平移-height/2
    cylinder = cylinder.Translate({0.0f, 0.0f, -height / 2.0f});
    
    // 应用用户变换
    if (scale != Vector3(1, 1, 1)) {
        cylinder = cylinder.Scale({scale.x, scale.y, scale.z});
    }
    if (rotation != Vector3(0, 0, 0)) {
        cylinder = cylinder.Rotate(rotation.x, rotation.y, rotation.z);
    }
    if (position != Vector3(0, 0, 0)) {
        // position是引擎坐标系(Y-up)，需要转换到Manifold坐标系(Z-up)
        cylinder = cylinder.Translate({position.x, -position.z, position.y});
    }
    
    return flatShading ? ManifoldToMesh_FlatShading(cylinder) : ManifoldToMesh_PreserveTopology(cylinder);
}

/**
 * 创建CSG圆锥体
 * 
 * 【返回几何体说明 - 引擎坐标系(Y-up)】
 * - 原点位置：几何中心（已自动居中）
 * - 朝向：底面在下(-Y)，尖端在上(+Y)
 * - 示例：CreateCSGCone(0.5, 2.0) 返回的圆锥范围是
 *   + X: [-0.5, 0.5] (底面直径1.0)
 *   + Y: [-1.0, 1.0] (高度2.0，几何中心在Y=0)
 *   + Z: [-0.5, 0.5] (底面直径1.0)
 *   + 顶点在(0, 1.0, 0)
 * 
 * @param radius  底面半径（顶端为尖点，半径为0）
 * @param height  圆锥高度（沿Y轴方向）
 * @param segments 圆周细分数，越大越平滑，默认32
 * @param position 世界坐标位置（引擎坐标系，指定几何中心），默认(0,0,0)
 * @param rotation 旋转欧拉角(度)，默认无旋转
 * @param scale   缩放系数，默认(1,1,1)
 * @param flatShading 是否使用平面着色（硬边效果），默认true
 */
std::shared_ptr<Mesh> CreateCSGCone(float radius, float height, int segments,
                                    const Vector3& position,
                                    const Vector3& rotation,
                                    const Vector3& scale,
                                    bool flatShading) {
    // Manifold::Cylinder(height, bottomRadius, topRadius)
    // 圆锥是topRadius=0的特殊圆柱，底部在原点(0,0,0)，沿Z轴向上延伸
    manifold::Manifold cone = manifold::Manifold::Cylinder(height, radius, 0.0f, segments);
    
    if (cone.Status() != manifold::Manifold::Error::NoError) {
        MOON_LOG_ERROR("CSG", "Failed to create cone");
        return nullptr;
    }
    
    // 在Manifold坐标系(Z-up)中将圆锥中心移到原点
    // Manifold Cone原始范围: Z[0, height]，要让它居中到Z[-height/2, height/2]
    // 所以需要向下平移-height/2
    cone = cone.Translate({0.0f, 0.0f, -height / 2.0f});
    
    // 应用用户变换
    if (scale != Vector3(1, 1, 1)) {
        cone = cone.Scale({scale.x, scale.y, scale.z});
    }
    if (rotation != Vector3(0, 0, 0)) {
        cone = cone.Rotate(rotation.x, rotation.y, rotation.z);
    }
    if (position != Vector3(0, 0, 0)) {
        // position是引擎坐标系(Y-up)，需要转换到Manifold坐标系(Z-up)
        cone = cone.Translate({position.x, -position.z, position.y});
    }
    
    return flatShading ? ManifoldToMesh_FlatShading(cone) : ManifoldToMesh_PreserveTopology(cone);
}

} // namespace CSG
} // namespace Moon