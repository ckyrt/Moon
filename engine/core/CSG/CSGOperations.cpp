#include "CSGOperations.h"
#include "../Logging/Logger.h"

// Manifold 库头文件
#include "manifold/manifold.h"

#include <vector>

namespace Moon {
namespace CSG {

// ============================================================================
// 辅助函数：Moon::Mesh → manifold::Manifold
// ============================================================================
static manifold::Manifold MeshToManifold(const Mesh* mesh) {
    if (!mesh || !mesh->IsValid()) {
        MOON_LOG_ERROR("CSG", "Invalid input mesh");
        return manifold::Manifold();
    }

    const std::vector<Vertex>& vertices = mesh->GetVertices();
    const std::vector<uint32_t>& indices = mesh->GetIndices();

    // 转换为 Manifold 的 MeshGL 格式
    manifold::MeshGL meshGL;
    
    // 顶点位置（只需要位置，Manifold 会处理法线）
    meshGL.numProp = 3; // x, y, z
    meshGL.vertProperties.reserve(vertices.size() * 3);
    for (const auto& v : vertices) {
        meshGL.vertProperties.push_back(v.position.x);
        meshGL.vertProperties.push_back(v.position.y);
        meshGL.vertProperties.push_back(v.position.z);
    }

    // 三角形索引
    meshGL.triVerts.reserve(indices.size());
    for (uint32_t idx : indices) {
        meshGL.triVerts.push_back(idx);
    }

    // 从 MeshGL 创建 Manifold
    return manifold::Manifold(meshGL);
}

// ============================================================================
// 辅助函数：manifold::Manifold → Moon::Mesh
// ============================================================================
static std::shared_ptr<Mesh> ManifoldToMesh(const manifold::Manifold& manifold) {
    if (manifold.IsEmpty()) {
        MOON_LOG_ERROR("CSG", "Result manifold is empty");
        return nullptr;
    }

    // 导出为 MeshGL
    manifold::MeshGL meshGL = manifold.GetMeshGL();

    // 转换顶点
    std::vector<Vertex> vertices;
    size_t numVerts = meshGL.vertProperties.size() / meshGL.numProp;
    vertices.reserve(numVerts);

    for (size_t i = 0; i < numVerts; ++i) {
        size_t offset = i * meshGL.numProp;
        Vertex v;
        v.position.x = meshGL.vertProperties[offset + 0];
        v.position.y = meshGL.vertProperties[offset + 1];
        v.position.z = meshGL.vertProperties[offset + 2];
        
        // 默认法线（向上）
        v.normal = Vector3(0, 1, 0);
        
        // 默认颜色
        v.colorR = 1.0f;
        v.colorG = 1.0f;
        v.colorB = 1.0f;
        v.colorA = 1.0f;
        
        // UV 默认值
        v.uv.x = 0.0f;
        v.uv.y = 0.0f;
        
        vertices.push_back(v);
    }

    // 转换索引
    std::vector<uint32_t> indices;
    indices.reserve(meshGL.triVerts.size());
    for (uint32_t idx : meshGL.triVerts) {
        indices.push_back(idx);
    }

    // 创建 Mesh
    auto mesh = std::make_shared<Mesh>();
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    
    if (!mesh->IsValid()) {
        MOON_LOG_ERROR("CSG", "Generated mesh is invalid");
        return nullptr;
    }

    return mesh;
}

// ============================================================================
// CSG 布尔运算
// ============================================================================
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

    // 转换回 Mesh
    return ManifoldToMesh(result);
}

// ============================================================================
// CSG 基础图元
// ============================================================================

std::shared_ptr<Mesh> CreateCSGBox(float width, float height, float depth) {
    MOON_LOG_INFO("CSG", "Creating box (%.2f x %.2f x %.2f)", width, height, depth);
    
    // 使用 Manifold 的立方体构造函数
    manifold::Manifold box = manifold::Manifold::Cube({width, height, depth}, true);
    
    return ManifoldToMesh(box);
}

std::shared_ptr<Mesh> CreateCSGSphere(float radius, int segments) {
    MOON_LOG_INFO("CSG", "Creating sphere (radius=%.2f, segments=%d)", radius, segments);
    
    // 使用 Manifold 的球体构造函数
    manifold::Manifold sphere = manifold::Manifold::Sphere(radius, segments);
    
    return ManifoldToMesh(sphere);
}

std::shared_ptr<Mesh> CreateCSGCylinder(float radius, float height, int segments) {
    MOON_LOG_INFO("CSG", "Creating cylinder (radius=%.2f, height=%.2f, segments=%d)", 
                 radius, height, segments);
    
    // 使用 Manifold 的圆柱体构造函数
    manifold::Manifold cylinder = manifold::Manifold::Cylinder(height, radius, radius, segments);
    
    return ManifoldToMesh(cylinder);
}

} // namespace CSG
} // namespace Moon
