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
    for (const auto& v : vertices) {
        meshGL.vertProperties.push_back(v.position.x);
        meshGL.vertProperties.push_back(v.position.y);
        meshGL.vertProperties.push_back(v.position.z);
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

static std::shared_ptr<Mesh> ManifoldToMesh_NoConversion(const manifold::Manifold& manifold) {
    if (manifold.IsEmpty()) return nullptr;

    manifold::MeshGL meshGL = manifold.GetMeshGL();
    std::vector<Vertex> vertices;
    size_t numVerts = meshGL.vertProperties.size() / meshGL.numProp;
    vertices.reserve(numVerts);

    for (size_t i = 0; i < numVerts; ++i) {
        size_t offset = i * meshGL.numProp;
        Vertex v;
        v.position.x = meshGL.vertProperties[offset + 0];
        v.position.y = meshGL.vertProperties[offset + 1];
        v.position.z = meshGL.vertProperties[offset + 2];
        v.normal = Vector3(0, 1, 0);
        v.uv = Vector2(0, 0);
        vertices.push_back(v);
    }

    std::vector<uint32_t> indices(meshGL.triVerts.begin(), meshGL.triVerts.end());

    auto mesh = std::make_shared<Mesh>();
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    return mesh;
}

std::shared_ptr<Mesh> PerformBoolean(const Mesh* meshA, const Mesh* meshB, Operation op) {
    if (!meshA || !meshB) {
        MOON_LOG_ERROR("CSG", "Null mesh input");
        return nullptr;
    }

    MOON_LOG_INFO("CSG", "Performing boolean operation...");

    // 转换为 Manifold
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

    return ManifoldToMesh_NoConversion(result);
}

std::shared_ptr<Mesh> CreateCSGBox(float width, float height, float depth,
                                   const Vector3& position,
                                   const Vector3& rotation,
                                   const Vector3& scale) {
    MOON_LOG_INFO("CSG", "Creating box (%.2f x %.2f x %.2f) at pos(%.2f, %.2f, %.2f)", 
                 width, height, depth, position.x, position.y, position.z);
    
    // 使用 Manifold 的立方体构造函数
    manifold::Manifold box = manifold::Manifold::Cube({width, height, depth}, true);
    
    // 应用Transform（在Manifold空间）
    // Transform顺序：缩放 → 旋转 → 平移（标准TRS顺序）
    // 注意：所有操作在Manifold右手坐标系下进行
    
    // 1. 缩放
    if (scale != Vector3(1, 1, 1)) {
        box = box.Scale({scale.x, scale.y, scale.z});
    }
    
    // 2. 旋转（TODO）
    // TODO: 实现旋转功能
    // 需要将欧拉角(rotation参数)转换为旋转矩阵
    // Manifold API: manifold.Rotate(x, y, z, degrees)
    // 或使用 manifold.Transform(mat3x4) 传入完整旋转矩阵
    if (rotation != Vector3(0, 0, 0)) {
        MOON_LOG_WARN("CSG", "Rotation not yet implemented (rotation=%.2f,%.2f,%.2f ignored)", 
                     rotation.x, rotation.y, rotation.z);
        // 未来实现示例：
        // Matrix3x4 rotMat = EulerToMatrix(rotation);
        // box = box.Transform(rotMat);
    }
    
    // 3. 平移
    
    if (position != Vector3(0, 0, 0)) {
        box = box.Translate({position.x, position.y, position.z});
    }
    
    // 返回无坐标转换的Mesh（保持Manifold坐标系）
    return ManifoldToMesh_NoConversion(box);
}

std::shared_ptr<Mesh> CreateCSGSphere(float radius, int segments,
                                      const Vector3& position,
                                      const Vector3& rotation,
                                      const Vector3& scale) {
    manifold::Manifold sphere = manifold::Manifold::Sphere(radius, segments);
    
    if (scale != Vector3(1, 1, 1)) {
        sphere = sphere.Scale({scale.x, scale.y, scale.z});
    }
    if (rotation != Vector3(0, 0, 0)) {
        sphere = sphere.Rotate(rotation.x, rotation.y, rotation.z);
    }
    if (position != Vector3(0, 0, 0)) {
        sphere = sphere.Translate({position.x, position.y, position.z});
    }
    
    return ManifoldToMesh_NoConversion(sphere);
}

std::shared_ptr<Mesh> CreateCSGCylinder(float radius, float height, int segments,
                                        const Vector3& position,
                                        const Vector3& rotation,
                                        const Vector3& scale) {
    manifold::Manifold cylinder = manifold::Manifold::Cylinder(height, radius, radius, segments);
    
    if (scale != Vector3(1, 1, 1)) {
        cylinder = cylinder.Scale({scale.x, scale.y, scale.z});
    }
    if (rotation != Vector3(0, 0, 0)) {
        cylinder = cylinder.Rotate(rotation.x, rotation.y, rotation.z);
    }
    if (position != Vector3(0, 0, 0)) {
        cylinder = cylinder.Translate({position.x, position.y, position.z});
    }
    
    return ManifoldToMesh_NoConversion(cylinder);
}

std::shared_ptr<Mesh> CreateCSGCone(float radius, float height, int segments,
                                    const Vector3& position,
                                    const Vector3& rotation,
                                    const Vector3& scale) {
    // Manifold::Cylinder(height, radiusLow, radiusHigh)
    // 在XY平面创建圆，沿Z轴挤出
    // radiusHigh=0创建圆锥
    manifold::Manifold cone = manifold::Manifold::Cylinder(height, radius, 0.0f, segments);
    
    if (cone.Status() != manifold::Manifold::Error::NoError) {
        MOON_LOG_ERROR("CSG", "Failed to create cone");
        return nullptr;
    }
    
    if (scale != Vector3(1, 1, 1)) {
        cone = cone.Scale({scale.x, scale.y, scale.z});
    }
    if (rotation != Vector3(0, 0, 0)) {
        cone = cone.Rotate(rotation.x, rotation.y, rotation.z);
    }
    if (position != Vector3(0, 0, 0)) {
        cone = cone.Translate({position.x, position.y, position.z});
    }
    
    return ManifoldToMesh_NoConversion(cone);
}

} // namespace CSG
} // namespace Moon
