#include "MeshManager.h"
#include "../Geometry/MeshGenerator.h"
#include "../Logging/Logger.h"
#include <string>

namespace Moon {

// ============================================================================
// 构造/析构
// ============================================================================

MeshManager::~MeshManager() {
    MOON_LOG_INFO("MeshManager", "Destroying MeshManager (%zu meshes, %zu named meshes)", 
                  m_meshes.size(), m_namedMeshes.size());
    
    // 显式清理（虽然 vector 和 map 会自动清理，但这里记录日志）
    Clear();
}

// ============================================================================
// 基础几何体创建接口
// ============================================================================

std::shared_ptr<Mesh> MeshManager::CreateCube(float size, const Vector3& color) {
    Mesh* rawMesh = MeshGenerator::CreateCube(size, color);
    std::shared_ptr<Mesh> mesh(rawMesh);
    m_meshes.push_back(mesh);
    return mesh;
}

std::shared_ptr<Mesh> MeshManager::CreateSphere(float radius, int segments, int rings, const Vector3& color) {
    Mesh* rawMesh = MeshGenerator::CreateSphere(radius, segments, rings, color);
    std::shared_ptr<Mesh> mesh(rawMesh);
    m_meshes.push_back(mesh);
    return mesh;
}

std::shared_ptr<Mesh> MeshManager::CreatePlane(float width, float depth, int subdivisionsX, int subdivisionsZ, const Vector3& color) {
    Mesh* rawMesh = MeshGenerator::CreatePlane(width, depth, subdivisionsX, subdivisionsZ, color);
    std::shared_ptr<Mesh> mesh(rawMesh);
    m_meshes.push_back(mesh);
    return mesh;
}

std::shared_ptr<Mesh> MeshManager::CreateCylinder(float radiusTop, float radiusBottom, float height, int segments, const Vector3& color) {
    Mesh* rawMesh = MeshGenerator::CreateCylinder(radiusTop, radiusBottom, height, segments, color);
    std::shared_ptr<Mesh> mesh(rawMesh);
    m_meshes.push_back(mesh);
    return mesh;
}

std::shared_ptr<Mesh> MeshManager::CreateCone(float radius, float height, int segments, const Vector3& color) {
    Mesh* rawMesh = MeshGenerator::CreateCone(radius, height, segments, color);
    std::shared_ptr<Mesh> mesh(rawMesh);
    m_meshes.push_back(mesh);
    return mesh;
}

std::shared_ptr<Mesh> MeshManager::CreateTorus(float majorRadius, float minorRadius, int majorSegments, int minorSegments, const Vector3& color) {
    Mesh* rawMesh = MeshGenerator::CreateTorus(majorRadius, minorRadius, majorSegments, minorSegments, color);
    std::shared_ptr<Mesh> mesh(rawMesh);
    m_meshes.push_back(mesh);
    return mesh;
}

std::shared_ptr<Mesh> MeshManager::CreateCapsule(float radius, float height, int segments, int rings, const Vector3& color) {
    Mesh* rawMesh = MeshGenerator::CreateCapsule(radius, height, segments, rings, color);
    std::shared_ptr<Mesh> mesh(rawMesh);
    m_meshes.push_back(mesh);
    return mesh;
}

std::shared_ptr<Mesh> MeshManager::CreateQuad(float width, float height, const Vector3& color) {
    Mesh* rawMesh = MeshGenerator::CreateQuad(width, height, color);
    std::shared_ptr<Mesh> mesh(rawMesh);
    m_meshes.push_back(mesh);
    return mesh;
}

// ============================================================================
// 资源管理接口
// ============================================================================

void MeshManager::Clear() {
    m_meshes.clear();
    m_namedMeshes.clear();
    MOON_LOG_INFO("MeshManager", "Cleared all mesh resources");
}

std::shared_ptr<Mesh> MeshManager::GetMesh(const std::string& name) const {
    auto it = m_namedMeshes.find(name);
    if (it != m_namedMeshes.end()) {
        return it->second;
    }
    return nullptr;
}

void MeshManager::RegisterMesh(const std::string& name, std::shared_ptr<Mesh> mesh) {
    if (!mesh) {
        MOON_LOG_INFO("MeshManager", "Attempted to register null mesh with name: %s", name.c_str());
        return;
    }
    
    m_namedMeshes[name] = mesh;
    MOON_LOG_INFO("MeshManager", "Registered mesh: %s", name.c_str());
}

} // namespace Moon
