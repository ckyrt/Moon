#include "Mesh.h"

namespace Moon {

Mesh* CreateCubeMesh(float size) {
    Mesh* mesh = new Mesh();
    
    float halfSize = size * 0.5f;
    
    // 定义 8 个顶点位置
    Vector3 positions[8] = {
        { -halfSize, -halfSize, -halfSize },  // 0: 左下后
        {  halfSize, -halfSize, -halfSize },  // 1: 右下后
        {  halfSize,  halfSize, -halfSize },  // 2: 右上后
        { -halfSize,  halfSize, -halfSize },  // 3: 左上后
        { -halfSize, -halfSize,  halfSize },  // 4: 左下前
        {  halfSize, -halfSize,  halfSize },  // 5: 右下前
        {  halfSize,  halfSize,  halfSize },  // 6: 右上前
        { -halfSize,  halfSize,  halfSize }   // 7: 左上前
    };
    
    // 6 个面的法线（朝外）
    Vector3 normals[6] = {
        { 0.0f, 0.0f, 1.0f },   // 前面 +Z
        { 0.0f, 0.0f, -1.0f },  // 后面 -Z
        { 0.0f, 1.0f, 0.0f },   // 上面 +Y
        { 0.0f, -1.0f, 0.0f },  // 下面 -Y
        { 1.0f, 0.0f, 0.0f },   // 右面 +X
        { -1.0f, 0.0f, 0.0f }   // 左面 -X
    };
    
    // 6 个面的颜色（RGB）
    Vector3 faceColors[6] = {
        { 1.0f, 0.0f, 0.0f },  // 前面 - 红色
        { 0.0f, 1.0f, 0.0f },  // 后面 - 绿色
        { 0.0f, 0.0f, 1.0f },  // 上面 - 蓝色
        { 1.0f, 1.0f, 0.0f },  // 下面 - 黄色
        { 1.0f, 0.0f, 1.0f },  // 右面 - 品红
        { 0.0f, 1.0f, 1.0f }   // 左面 - 青色
    };
    
    // 构建 24 个顶点（每个面 4 个顶点，共 6 个面）
    std::vector<Vertex> vertices;
    vertices.reserve(24);
    
    // UV坐标（标准纹理映射：每个面独立映射到[0,1]x[0,1]）
    Vector2 uvs[4] = {
        { 0.0f, 1.0f },  // 左下
        { 1.0f, 1.0f },  // 右下
        { 1.0f, 0.0f },  // 右上
        { 0.0f, 0.0f }   // 左上
    };
    
    // 前面 (Z+)
    vertices.emplace_back(positions[4], normals[0], faceColors[0], 1.0f, uvs[0]);
    vertices.emplace_back(positions[5], normals[0], faceColors[0], 1.0f, uvs[1]);
    vertices.emplace_back(positions[6], normals[0], faceColors[0], 1.0f, uvs[2]);
    vertices.emplace_back(positions[7], normals[0], faceColors[0], 1.0f, uvs[3]);
    
    // 后面 (Z-)
    vertices.emplace_back(positions[1], normals[1], faceColors[1], 1.0f, uvs[0]);
    vertices.emplace_back(positions[0], normals[1], faceColors[1], 1.0f, uvs[1]);
    vertices.emplace_back(positions[3], normals[1], faceColors[1], 1.0f, uvs[2]);
    vertices.emplace_back(positions[2], normals[1], faceColors[1], 1.0f, uvs[3]);
    
    // 上面 (Y+)
    vertices.emplace_back(positions[7], normals[2], faceColors[2], 1.0f, uvs[0]);
    vertices.emplace_back(positions[6], normals[2], faceColors[2], 1.0f, uvs[1]);
    vertices.emplace_back(positions[2], normals[2], faceColors[2], 1.0f, uvs[2]);
    vertices.emplace_back(positions[3], normals[2], faceColors[2], 1.0f, uvs[3]);
    
    // 下面 (Y-)
    vertices.emplace_back(positions[4], normals[3], faceColors[3], 1.0f, uvs[0]);
    vertices.emplace_back(positions[0], normals[3], faceColors[3], 1.0f, uvs[1]);
    vertices.emplace_back(positions[1], normals[3], faceColors[3], 1.0f, uvs[2]);
    vertices.emplace_back(positions[5], normals[3], faceColors[3], 1.0f, uvs[3]);
    
    // 右面 (X+)
    vertices.emplace_back(positions[5], normals[4], faceColors[4], 1.0f, uvs[0]);
    vertices.emplace_back(positions[1], normals[4], faceColors[4], 1.0f, uvs[1]);
    vertices.emplace_back(positions[2], normals[4], faceColors[4], 1.0f, uvs[2]);
    vertices.emplace_back(positions[6], normals[4], faceColors[4], 1.0f, uvs[3]);
    
    // 左面 (X-)
    vertices.emplace_back(positions[0], normals[5], faceColors[5], 1.0f, uvs[0]);
    vertices.emplace_back(positions[4], normals[5], faceColors[5], 1.0f, uvs[1]);
    vertices.emplace_back(positions[7], normals[5], faceColors[5], 1.0f, uvs[2]);
    vertices.emplace_back(positions[3], normals[5], faceColors[5], 1.0f, uvs[3]);
    
    // 构建索引（每个面 2 个三角形，共 36 个索引）
    std::vector<uint32_t> indices;
    indices.reserve(36);
    
    for (uint32_t face = 0; face < 6; ++face) {
        uint32_t baseIndex = face * 4;
        // 第一个三角形
        indices.push_back(baseIndex + 0);
        indices.push_back(baseIndex + 1);
        indices.push_back(baseIndex + 2);
        // 第二个三角形
        indices.push_back(baseIndex + 0);
        indices.push_back(baseIndex + 2);
        indices.push_back(baseIndex + 3);
    }
    
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    
    return mesh;
}

} // namespace Moon
