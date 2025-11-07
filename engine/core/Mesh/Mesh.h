#pragma once
#include <vector>
#include "../Camera/Camera.h"  // For Vector3

namespace Moon {

/**
 * @brief 顶点数据结构（简化版）
 * 
 * 目前只包含位置和颜色，不包含纹理坐标、法线等
 */
struct Vertex {
    Vector3 position;  ///< 顶点位置 (3 floats, 12 bytes)
    float colorR;      ///< 红色分量 (0-1)
    float colorG;      ///< 绿色分量 (0-1)
    float colorB;      ///< 蓝色分量 (0-1)
    float colorA;      ///< Alpha 通道 (0-1, 1=不透明)
    
    Vertex() 
        : position(0, 0, 0)
        , colorR(1.0f), colorG(1.0f), colorB(1.0f), colorA(1.0f) 
    {}
    
    Vertex(const Vector3& pos, const Vector3& col, float alpha = 1.0f) 
        : position(pos)
        , colorR(col.x), colorG(col.y), colorB(col.z), colorA(alpha)
    {}
};

/**
 * @brief Mesh 类 - 存储几何网格数据
 * 
 * 简化版设计：
 * - 只支持三角形网格（索引必须是3的倍数）
 * - 顶点包含：位置 + 颜色
 * - 未来可扩展：法线、UV、切线等
 */
class Mesh {
public:
    Mesh() = default;
    ~Mesh() = default;
    
    // 禁止拷贝，允许移动
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) = default;
    Mesh& operator=(Mesh&&) = default;
    
    /**
     * @brief 设置顶点数据
     */
    void SetVertices(const std::vector<Vertex>& vertices) {
        m_vertices = vertices;
    }
    
    void SetVertices(std::vector<Vertex>&& vertices) {
        m_vertices = std::move(vertices);
    }
    
    /**
     * @brief 设置索引数据（三角形列表）
     */
    void SetIndices(const std::vector<uint32_t>& indices) {
        m_indices = indices;
    }
    
    void SetIndices(std::vector<uint32_t>&& indices) {
        m_indices = std::move(indices);
    }
    
    /**
     * @brief 获取顶点数据（只读）
     */
    const std::vector<Vertex>& GetVertices() const { return m_vertices; }
    
    /**
     * @brief 获取索引数据（只读）
     */
    const std::vector<uint32_t>& GetIndices() const { return m_indices; }
    
    /**
     * @brief 获取顶点数量
     */
    size_t GetVertexCount() const { return m_vertices.size(); }
    
    /**
     * @brief 获取索引数量
     */
    size_t GetIndexCount() const { return m_indices.size(); }
    
    /**
     * @brief 获取三角形数量
     */
    size_t GetTriangleCount() const { return m_indices.size() / 3; }
    
    /**
     * @brief 检查 Mesh 是否有效
     */
    bool IsValid() const {
        return !m_vertices.empty() && 
               !m_indices.empty() && 
               (m_indices.size() % 3 == 0);
    }
    
    /**
     * @brief 清空数据
     */
    void Clear() {
        m_vertices.clear();
        m_indices.clear();
    }

private:
    std::vector<Vertex> m_vertices;    ///< 顶点数组
    std::vector<uint32_t> m_indices;   ///< 索引数组（三角形列表）
};

/**
 * @brief 创建立方体 Mesh 的辅助函数
 * @param size 立方体边长（默认1.0）
 * @return 生成的立方体 Mesh
 */
Mesh* CreateCubeMesh(float size = 1.0f);

} // namespace Moon
