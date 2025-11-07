#include "MeshGenerator.h"
#include <cmath>

namespace Moon {

// 数学常量
constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;

// ============================================================================
// Cube - 立方体
// ============================================================================
Mesh* MeshGenerator::CreateCube(float size, const Vector3& color) {
    Mesh* mesh = new Mesh();
    
    float half = size * 0.5f;
    
    // 8个顶点坐标
    Vector3 positions[8] = {
        Vector3(-half, -half, -half),  // 0: 左下后
        Vector3( half, -half, -half),  // 1: 右下后
        Vector3( half,  half, -half),  // 2: 右上后
        Vector3(-half,  half, -half),  // 3: 左上后
        Vector3(-half, -half,  half),  // 4: 左下前
        Vector3( half, -half,  half),  // 5: 右下前
        Vector3( half,  half,  half),  // 6: 右上前
        Vector3(-half,  half,  half)   // 7: 左上前
    };
    
    // 24个顶点（每个面4个顶点，共6个面）
    std::vector<Vertex> vertices = {
        // 前面 (+Z)
        Vertex(positions[4], color), Vertex(positions[5], color), 
        Vertex(positions[6], color), Vertex(positions[7], color),
        
        // 后面 (-Z)
        Vertex(positions[1], color), Vertex(positions[0], color), 
        Vertex(positions[3], color), Vertex(positions[2], color),
        
        // 左面 (-X)
        Vertex(positions[0], color), Vertex(positions[4], color), 
        Vertex(positions[7], color), Vertex(positions[3], color),
        
        // 右面 (+X)
        Vertex(positions[5], color), Vertex(positions[1], color), 
        Vertex(positions[2], color), Vertex(positions[6], color),
        
        // 上面 (+Y)
        Vertex(positions[7], color), Vertex(positions[6], color), 
        Vertex(positions[2], color), Vertex(positions[3], color),
        
        // 下面 (-Y)
        Vertex(positions[4], color), Vertex(positions[0], color), 
        Vertex(positions[1], color), Vertex(positions[5], color)
    };
    
    // 36个索引（6个面 * 2个三角形 * 3个顶点）
    std::vector<uint32_t> indices = {
        0,  1,  2,   0,  2,  3,   // 前
        4,  5,  6,   4,  6,  7,   // 后
        8,  9,  10,  8,  10, 11,  // 左
        12, 13, 14,  12, 14, 15,  // 右
        16, 17, 18,  16, 18, 19,  // 上
        20, 21, 22,  20, 22, 23   // 下
    };
    
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    
    return mesh;
}

// ============================================================================
// Sphere - UV球体（经纬度）
// ============================================================================
Mesh* MeshGenerator::CreateSphere(float radius, int segments, int rings, const Vector3& color) {
    if (segments < 3) segments = 3;
    if (rings < 2) rings = 2;
    
    Mesh* mesh = new Mesh();
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // 顶点生成
    for (int ring = 0; ring <= rings; ++ring) {
        float phi = PI * float(ring) / float(rings);  // 纬度角 [0, π]
        
        for (int seg = 0; seg <= segments; ++seg) {
            float theta = TWO_PI * float(seg) / float(segments);  // 经度角 [0, 2π]
            
            Vector3 pos = SphericalToCartesian(radius, theta, phi);
            vertices.push_back(Vertex(pos, color));
        }
    }
    
    // 索引生成（四边形网格转三角形）
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            int current = ring * (segments + 1) + seg;
            int next = current + segments + 1;
            
            // 第一个三角形
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);
            
            // 第二个三角形
            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }
    
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    
    return mesh;
}

// ============================================================================
// Plane - 平面网格
// ============================================================================
Mesh* MeshGenerator::CreatePlane(float width, float depth, int subdivisionsX, int subdivisionsZ, const Vector3& color) {
    if (subdivisionsX < 1) subdivisionsX = 1;
    if (subdivisionsZ < 1) subdivisionsZ = 1;
    
    Mesh* mesh = new Mesh();
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float halfW = width * 0.5f;
    float halfD = depth * 0.5f;
    
    int vertsX = subdivisionsX + 1;
    int vertsZ = subdivisionsZ + 1;
    
    // 顶点生成
    for (int z = 0; z < vertsZ; ++z) {
        for (int x = 0; x < vertsX; ++x) {
            float px = -halfW + (width * x) / subdivisionsX;
            float pz = -halfD + (depth * z) / subdivisionsZ;
            
            vertices.push_back(Vertex(Vector3(px, 0, pz), color));
        }
    }
    
    // 索引生成
    for (int z = 0; z < subdivisionsZ; ++z) {
        for (int x = 0; x < subdivisionsX; ++x) {
            int topLeft = z * vertsX + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * vertsX + x;
            int bottomRight = bottomLeft + 1;
            
            // 第一个三角形
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            // 第二个三角形
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
    
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    
    return mesh;
}

// ============================================================================
// Cylinder - 圆柱体
// ============================================================================
Mesh* MeshGenerator::CreateCylinder(float radiusTop, float radiusBottom, float height, int segments, const Vector3& color) {
    if (segments < 3) segments = 3;
    
    Mesh* mesh = new Mesh();
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float halfH = height * 0.5f;
    
    // 顶部圆心
    int topCenterIdx = vertices.size();
    vertices.push_back(Vertex(Vector3(0, halfH, 0), color));
    
    // 顶部圆周顶点
    int topStartIdx = vertices.size();
    GenerateCircleVertices(vertices, radiusTop, halfH, segments, color);
    
    // 底部圆心
    int bottomCenterIdx = vertices.size();
    vertices.push_back(Vertex(Vector3(0, -halfH, 0), color));
    
    // 底部圆周顶点
    int bottomStartIdx = vertices.size();
    GenerateCircleVertices(vertices, radiusBottom, -halfH, segments, color);
    
    // 侧面顶点（需要重复顶点以支持不同法线）
    int sideTopStartIdx = vertices.size();
    GenerateCircleVertices(vertices, radiusTop, halfH, segments, color);
    
    int sideBottomStartIdx = vertices.size();
    GenerateCircleVertices(vertices, radiusBottom, -halfH, segments, color);
    
    // 顶面三角形（从圆心发散）
    for (int i = 0; i < segments; ++i) {
        int next = (i + 1) % segments;
        indices.push_back(topCenterIdx);
        indices.push_back(topStartIdx + i);
        indices.push_back(topStartIdx + next);
    }
    
    // 底面三角形（逆序，法线向下）
    for (int i = 0; i < segments; ++i) {
        int next = (i + 1) % segments;
        indices.push_back(bottomCenterIdx);
        indices.push_back(bottomStartIdx + next);
        indices.push_back(bottomStartIdx + i);
    }
    
    // 侧面三角形
    for (int i = 0; i < segments; ++i) {
        int next = (i + 1) % segments;
        
        int t0 = sideTopStartIdx + i;
        int t1 = sideTopStartIdx + next;
        int b0 = sideBottomStartIdx + i;
        int b1 = sideBottomStartIdx + next;
        
        indices.push_back(t0);
        indices.push_back(b0);
        indices.push_back(t1);
        
        indices.push_back(t1);
        indices.push_back(b0);
        indices.push_back(b1);
    }
    
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    
    return mesh;
}

// ============================================================================
// Cone - 圆锥体
// ============================================================================
Mesh* MeshGenerator::CreateCone(float radius, float height, int segments, const Vector3& color) {
    // 圆锥 = 顶部半径为0的圆柱
    return CreateCylinder(0.0f, radius, height, segments, color);
}

// ============================================================================
// Torus - 圆环体
// ============================================================================
Mesh* MeshGenerator::CreateTorus(float majorRadius, float minorRadius, int majorSegments, int minorSegments, const Vector3& color) {
    if (majorSegments < 3) majorSegments = 3;
    if (minorSegments < 3) minorSegments = 3;
    
    Mesh* mesh = new Mesh();
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    // 顶点生成
    for (int i = 0; i <= majorSegments; ++i) {
        float u = TWO_PI * float(i) / float(majorSegments);
        float cosu = cosf(u);
        float sinu = sinf(u);
        
        for (int j = 0; j <= minorSegments; ++j) {
            float v = TWO_PI * float(j) / float(minorSegments);
            float cosv = cosf(v);
            float sinv = sinf(v);
            
            // 圆环参数方程
            float x = (majorRadius + minorRadius * cosv) * cosu;
            float y = minorRadius * sinv;
            float z = (majorRadius + minorRadius * cosv) * sinu;
            
            vertices.push_back(Vertex(Vector3(x, y, z), color));
        }
    }
    
    // 索引生成
    for (int i = 0; i < majorSegments; ++i) {
        for (int j = 0; j < minorSegments; ++j) {
            int current = i * (minorSegments + 1) + j;
            int next = current + minorSegments + 1;
            
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);
            
            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }
    
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    
    return mesh;
}

// ============================================================================
// Capsule - 胶囊体
// ============================================================================
Mesh* MeshGenerator::CreateCapsule(float radius, float height, int segments, int rings, const Vector3& color) {
    if (segments < 3) segments = 3;
    if (rings < 1) rings = 1;
    
    Mesh* mesh = new Mesh();
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    
    float cylinderHeight = height - 2.0f * radius;
    if (cylinderHeight < 0) cylinderHeight = 0;
    
    float halfCylinder = cylinderHeight * 0.5f;
    
    // 上半球
    for (int ring = 0; ring <= rings; ++ring) {
        float phi = (PI * 0.5f) * float(ring) / float(rings);  // [0, π/2]
        float y = radius * cosf(phi) + halfCylinder;
        float ringRadius = radius * sinf(phi);
        
        for (int seg = 0; seg <= segments; ++seg) {
            float theta = TWO_PI * float(seg) / float(segments);
            float x = ringRadius * cosf(theta);
            float z = ringRadius * sinf(theta);
            
            vertices.push_back(Vertex(Vector3(x, y, z), color));
        }
    }
    
    // 圆柱体中段
    for (int seg = 0; seg <= segments; ++seg) {
        float theta = TWO_PI * float(seg) / float(segments);
        float x = radius * cosf(theta);
        float z = radius * sinf(theta);
        
        vertices.push_back(Vertex(Vector3(x, halfCylinder, z), color));
        vertices.push_back(Vertex(Vector3(x, -halfCylinder, z), color));
    }
    
    int cylinderStartIdx = (rings + 1) * (segments + 1);
    
    // 下半球
    int bottomStartIdx = cylinderStartIdx + 2 * (segments + 1);
    for (int ring = 0; ring <= rings; ++ring) {
        float phi = PI * 0.5f + (PI * 0.5f) * float(ring) / float(rings);  // [π/2, π]
        float y = radius * cosf(phi) - halfCylinder;
        float ringRadius = radius * sinf(phi);
        
        for (int seg = 0; seg <= segments; ++seg) {
            float theta = TWO_PI * float(seg) / float(segments);
            float x = ringRadius * cosf(theta);
            float z = ringRadius * sinf(theta);
            
            vertices.push_back(Vertex(Vector3(x, y, z), color));
        }
    }
    
    // 上半球索引
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            int current = ring * (segments + 1) + seg;
            int next = current + segments + 1;
            
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);
            
            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }
    
    // 圆柱体索引
    for (int seg = 0; seg < segments; ++seg) {
        int t0 = cylinderStartIdx + seg * 2;
        int t1 = cylinderStartIdx + (seg + 1) * 2;
        int b0 = t0 + 1;
        int b1 = t1 + 1;
        
        indices.push_back(t0);
        indices.push_back(b0);
        indices.push_back(t1);
        
        indices.push_back(t1);
        indices.push_back(b0);
        indices.push_back(b1);
    }
    
    // 下半球索引
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            int current = bottomStartIdx + ring * (segments + 1) + seg;
            int next = current + segments + 1;
            
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);
            
            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }
    
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    
    return mesh;
}

// ============================================================================
// Quad - 四边形面片
// ============================================================================
Mesh* MeshGenerator::CreateQuad(float width, float height, const Vector3& color) {
    Mesh* mesh = new Mesh();
    
    float halfW = width * 0.5f;
    float halfH = height * 0.5f;
    
    std::vector<Vertex> vertices = {
        Vertex(Vector3(-halfW, -halfH, 0), color),  // 左下
        Vertex(Vector3( halfW, -halfH, 0), color),  // 右下
        Vertex(Vector3( halfW,  halfH, 0), color),  // 右上
        Vertex(Vector3(-halfW,  halfH, 0), color)   // 左上
    };
    
    std::vector<uint32_t> indices = {
        0, 1, 2,   // 第一个三角形
        0, 2, 3    // 第二个三角形
    };
    
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    
    return mesh;
}

// ============================================================================
// 辅助函数
// ============================================================================
Vector3 MeshGenerator::SphericalToCartesian(float radius, float theta, float phi) {
    float sinPhi = sinf(phi);
    return Vector3(
        radius * sinPhi * cosf(theta),  // x
        radius * cosf(phi),              // y
        radius * sinPhi * sinf(theta)   // z
    );
}

void MeshGenerator::GenerateCircleVertices(std::vector<Vertex>& vertices, float radius, float y, int segments, const Vector3& color) {
    for (int i = 0; i < segments; ++i) {
        float angle = TWO_PI * float(i) / float(segments);
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        vertices.push_back(Vertex(Vector3(x, y, z), color));
    }
}

} // namespace Moon
