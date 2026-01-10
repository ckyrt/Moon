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

// 内部函数：保留拓扑结构的转换（用于布尔运算中间步骤）
static std::shared_ptr<Mesh> ManifoldToMesh_PreserveTopology(const manifold::Manifold& manifold) {
    if (manifold.IsEmpty()) return nullptr;

    manifold::MeshGL meshGL = manifold.GetMeshGL();
    std::vector<Vertex> vertices;
    size_t numVerts = meshGL.vertProperties.size() / meshGL.numProp;
    vertices.reserve(numVerts);

    // 保留顶点共享结构（用于布尔运算）
    for (size_t i = 0; i < numVerts; ++i) {
        size_t offset = i * meshGL.numProp;
        Vertex v;
        v.position.x = meshGL.vertProperties[offset + 0];
        v.position.y = meshGL.vertProperties[offset + 1];
        v.position.z = meshGL.vertProperties[offset + 2];
        v.normal = Vector3(0, 0, 0);
        v.uv = Vector2(0, 0);
        vertices.push_back(v);
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
    for (auto& v : vertices) {
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
        
        Vector3 p0(
            meshGL.vertProperties[origIdx0 * meshGL.numProp + 0],
            meshGL.vertProperties[origIdx0 * meshGL.numProp + 1],
            meshGL.vertProperties[origIdx0 * meshGL.numProp + 2]
        );
        Vector3 p1(
            meshGL.vertProperties[origIdx1 * meshGL.numProp + 0],
            meshGL.vertProperties[origIdx1 * meshGL.numProp + 1],
            meshGL.vertProperties[origIdx1 * meshGL.numProp + 2]
        );
        Vector3 p2(
            meshGL.vertProperties[origIdx2 * meshGL.numProp + 0],
            meshGL.vertProperties[origIdx2 * meshGL.numProp + 1],
            meshGL.vertProperties[origIdx2 * meshGL.numProp + 2]
        );
        
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

    // 布尔运算结果使用扁平着色（硬边效果）
    return ManifoldToMesh_FlatShading(result);
}

std::shared_ptr<Mesh> CreateCSGBox(float width, float height, float depth,
                                   const Vector3& position,
                                   const Vector3& rotation,
                                   const Vector3& scale,
                                   bool flatShading) {
    MOON_LOG_INFO("CSG", "Creating box (%.2f x %.2f x %.2f) at pos(%.2f, %.2f, %.2f)", 
                 width, height, depth, position.x, position.y, position.z);
    
    manifold::Manifold box = manifold::Manifold::Cube({width, height, depth}, true);
    
    if (scale != Vector3(1, 1, 1)) {
        box = box.Scale({scale.x, scale.y, scale.z});
    }
    if (rotation != Vector3(0, 0, 0)) {
        box = box.Rotate(rotation.x, rotation.y, rotation.z);
    }
    if (position != Vector3(0, 0, 0)) {
        box = box.Translate({position.x, position.y, position.z});
    }
    
    // 根据flatShading参数选择转换方式
    return flatShading ? ManifoldToMesh_FlatShading(box) : ManifoldToMesh_PreserveTopology(box);
}

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
        sphere = sphere.Translate({position.x, position.y, position.z});
    }
    
    return flatShading ? ManifoldToMesh_FlatShading(sphere) : ManifoldToMesh_PreserveTopology(sphere);
}

std::shared_ptr<Mesh> CreateCSGCylinder(float radius, float height, int segments,
                                        const Vector3& position,
                                        const Vector3& rotation,
                                        const Vector3& scale,
                                        bool flatShading) {
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
    
    return flatShading ? ManifoldToMesh_FlatShading(cylinder) : ManifoldToMesh_PreserveTopology(cylinder);
}

std::shared_ptr<Mesh> CreateCSGCone(float radius, float height, int segments,
                                    const Vector3& position,
                                    const Vector3& rotation,
                                    const Vector3& scale,
                                    bool flatShading) {
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
    
    return flatShading ? ManifoldToMesh_FlatShading(cone) : ManifoldToMesh_PreserveTopology(cone);
}

} // namespace CSG
} // namespace Moon
