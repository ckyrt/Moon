#pragma once

#include <cstdint>
#include <vector>

#include "../Camera/Camera.h"
#include "../Math/Vector2.h"

namespace Moon {

struct VertexAttributeDesc {
    const char* semanticName;
    int numComponents;
    int offsetInBytes;
};

struct Vertex {
    Vector3 position;
    Vector3 normal;
    float colorR;
    float colorG;
    float colorB;
    float colorA;
    Vector2 uv;

    Vertex()
        : position(0, 0, 0)
        , normal(0, 1, 0)
        , colorR(1.0f)
        , colorG(1.0f)
        , colorB(1.0f)
        , colorA(1.0f)
        , uv(0.0f, 0.0f) {
    }

    Vertex(
        const Vector3& pos,
        const Vector3& norm,
        const Vector3& col,
        float alpha = 1.0f,
        const Vector2& texCoord = Vector2(0.0f, 0.0f))
        : position(pos)
        , normal(norm)
        , colorR(col.x)
        , colorG(col.y)
        , colorB(col.z)
        , colorA(alpha)
        , uv(texCoord) {
    }

    static const VertexAttributeDesc* GetLayoutDesc(int& outCount) {
        static const VertexAttributeDesc layout[] = {
            {"POSITION", 3, offsetof(Vertex, position)},
            {"NORMAL", 3, offsetof(Vertex, normal)},
            {"COLOR", 4, offsetof(Vertex, colorR)},
            {"TEXCOORD", 2, offsetof(Vertex, uv)},
        };
        outCount = 4;
        return layout;
    }

    static constexpr int GetStride() { return sizeof(Vertex); }
};

static_assert(sizeof(Vertex) == 48, "Vertex size must be 48 bytes");
static_assert(offsetof(Vertex, position) == 0, "Position must be at offset 0");
static_assert(offsetof(Vertex, normal) == 12, "Normal must be at offset 12");
static_assert(offsetof(Vertex, colorR) == 24, "Color must be at offset 24");
static_assert(offsetof(Vertex, uv) == 40, "UV must be at offset 40");

class Mesh {
public:
    Mesh();
    ~Mesh() = default;

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) = default;
    Mesh& operator=(Mesh&&) = default;

    void SetVertices(const std::vector<Vertex>& vertices) {
        m_vertices = vertices;
    }

    void SetVertices(std::vector<Vertex>&& vertices) {
        m_vertices = std::move(vertices);
    }

    void SetIndices(const std::vector<uint32_t>& indices) {
        m_indices = indices;
    }

    void SetIndices(std::vector<uint32_t>&& indices) {
        m_indices = std::move(indices);
    }

    const std::vector<Vertex>& GetVertices() const { return m_vertices; }
    const std::vector<uint32_t>& GetIndices() const { return m_indices; }

    size_t GetVertexCount() const { return m_vertices.size(); }
    size_t GetIndexCount() const { return m_indices.size(); }
    size_t GetTriangleCount() const { return m_indices.size() / 3; }

    uint64_t GetRuntimeId() const { return m_runtimeId; }

    bool IsValid() const {
        return !m_vertices.empty() && !m_indices.empty() && (m_indices.size() % 3 == 0);
    }

    void Clear() {
        m_vertices.clear();
        m_indices.clear();
    }

private:
    static uint64_t AllocateRuntimeId();

    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    uint64_t m_runtimeId = 0;
};

Mesh* CreateCubeMesh(float size = 1.0f);

} // namespace Moon
