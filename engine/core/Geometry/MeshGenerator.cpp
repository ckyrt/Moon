#include "MeshGenerator.h"
#include <cmath>

namespace Moon {

// 数学常量
constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 2.0f * PI;

// 注意：引擎使用左手坐标系（+Y上，+Z前，+X右），顺时针三角形是正面

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
    
    // 6个面的法线（朝外）
    Vector3 normals[6] = {
        Vector3(0, 0, 1),   // 前 +Z
        Vector3(0, 0, -1),  // 后 -Z
        Vector3(-1, 0, 0),  // 左 -X
        Vector3(1, 0, 0),   // 右 +X
        Vector3(0, 1, 0),   // 上 +Y
        Vector3(0, -1, 0)   // 下 -Y
    };
    
    // 每个面的UV坐标（标准映射：左下(0,1)、右下(1,1)、右上(1,0)、左上(0,0)）
    Vector2 uvs[4] = {
        Vector2(0.0f, 1.0f),  // 左下
        Vector2(1.0f, 1.0f),  // 右下
        Vector2(1.0f, 0.0f),  // 右上
        Vector2(0.0f, 0.0f)   // 左上
    };
    
    // 24个顶点（每个面4个顶点，共6个面）
    std::vector<Vertex> vertices = {
        // 前面 (+Z) - 顶点索引：4, 5, 6, 7
        Vertex(positions[4], normals[0], color, 1.0f, uvs[0]), 
        Vertex(positions[5], normals[0], color, 1.0f, uvs[1]), 
        Vertex(positions[6], normals[0], color, 1.0f, uvs[2]), 
        Vertex(positions[7], normals[0], color, 1.0f, uvs[3]),
        
        // 后面 (-Z) - 顶点索引：1, 0, 3, 2
        Vertex(positions[1], normals[1], color, 1.0f, uvs[0]), 
        Vertex(positions[0], normals[1], color, 1.0f, uvs[1]), 
        Vertex(positions[3], normals[1], color, 1.0f, uvs[2]), 
        Vertex(positions[2], normals[1], color, 1.0f, uvs[3]),
        
        // 左面 (-X) - 顶点索引：0, 4, 7, 3
        Vertex(positions[0], normals[2], color, 1.0f, uvs[0]), 
        Vertex(positions[4], normals[2], color, 1.0f, uvs[1]), 
        Vertex(positions[7], normals[2], color, 1.0f, uvs[2]), 
        Vertex(positions[3], normals[2], color, 1.0f, uvs[3]),
        
        // 右面 (+X) - 顶点索引：5, 1, 2, 6
        Vertex(positions[5], normals[3], color, 1.0f, uvs[0]), 
        Vertex(positions[1], normals[3], color, 1.0f, uvs[1]), 
        Vertex(positions[2], normals[3], color, 1.0f, uvs[2]), 
        Vertex(positions[6], normals[3], color, 1.0f, uvs[3]),
        
        // 上面 (+Y) - 顶点索引：7, 6, 2, 3
        Vertex(positions[7], normals[4], color, 1.0f, uvs[0]), 
        Vertex(positions[6], normals[4], color, 1.0f, uvs[1]), 
        Vertex(positions[2], normals[4], color, 1.0f, uvs[2]), 
        Vertex(positions[3], normals[4], color, 1.0f, uvs[3]),
        
        // 下面 (-Y) - 顶点索引：4, 0, 1, 5
        Vertex(positions[4], normals[5], color, 1.0f, uvs[0]), 
        Vertex(positions[0], normals[5], color, 1.0f, uvs[1]), 
        Vertex(positions[1], normals[5], color, 1.0f, uvs[2]), 
        Vertex(positions[5], normals[5], color, 1.0f, uvs[3])
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
            
            // 球体法线 = 归一化的径向向量（从球心指向表面）
            Vector3 normal = pos;
            float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (length > 0.0001f) {
                normal.x /= length;
                normal.y /= length;
                normal.z /= length;
            }
            
            // UV 坐标：球面映射
            // u: 经度 [0, 1]
            // v: 纬度 [0, 1]
            float u = float(seg) / float(segments);
            float v = float(ring) / float(rings);
            Vector2 uv(u, v);
            
            vertices.push_back(Vertex(pos, normal, color, 1.0f, uv));
        }
    }
    
    // 索引生成（四边形网格转三角形，顺时针）
    // 注意：左手坐标系中，顺时针是正面（与Cube一致）
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            int current = ring * (segments + 1) + seg;
            int next = current + segments + 1;
            
            // 第一个三角形（顺时针）
            indices.push_back(current);
            indices.push_back(current + 1);
            indices.push_back(next);
            
            // 第二个三角形（顺时针）
            indices.push_back(current + 1);
            indices.push_back(next + 1);
            indices.push_back(next);
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
    
    // 平面法线向上（+Y）
    Vector3 normal(0, 1, 0);
    
    // 顶点生成
    for (int z = 0; z < vertsZ; ++z) {
        for (int x = 0; x < vertsX; ++x) {
            float px = -halfW + (width * x) / subdivisionsX;
            float pz = -halfD + (depth * z) / subdivisionsZ;
            
            // UV 坐标：从 (0,0) 到 (1,1)，注意 Z 轴对应 V 坐标
            float u = static_cast<float>(x) / subdivisionsX;
            float v = static_cast<float>(z) / subdivisionsZ;
            
            vertices.push_back(Vertex(Vector3(px, 0, pz), normal, color, 1.0f, Vector2(u, v)));
        }
    }
    
    // 索引生成：生成双面 Plane（正面+背面）
    // 左手坐标系：从上往下看，逆时针为正面
    for (int z = 0; z < subdivisionsZ; ++z) {
        for (int x = 0; x < subdivisionsX; ++x) {
            int topLeft = z * vertsX + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * vertsX + x;
            int bottomRight = bottomLeft + 1;
            
            // 正面（从上往下看，逆时针）
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
            
            // 背面（从下往上看，逆时针）
            indices.push_back(topLeft);
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            
            indices.push_back(topRight);
            indices.push_back(bottomRight);
            indices.push_back(bottomLeft);
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
    
    // 顶部圆心（法线向上）
    uint32_t topCenterIdx = static_cast<uint32_t>(vertices.size());
    vertices.push_back(Vertex(Vector3(0, halfH, 0), Vector3(0, 1, 0), color));
    
    // 顶部圆周顶点（法线向上）
    uint32_t topStartIdx = static_cast<uint32_t>(vertices.size());
    for (int i = 0; i < segments; ++i) {
        float angle = TWO_PI * float(i) / float(segments);
        float x = radiusTop * cosf(angle);
        float z = radiusTop * sinf(angle);
        vertices.push_back(Vertex(Vector3(x, halfH, z), Vector3(0, 1, 0), color));
    }
    
    // 底部圆心（法线向下）
    uint32_t bottomCenterIdx = static_cast<uint32_t>(vertices.size());
    vertices.push_back(Vertex(Vector3(0, -halfH, 0), Vector3(0, -1, 0), color));
    
    // 底部圆周顶点（法线向下）
    uint32_t bottomStartIdx = static_cast<uint32_t>(vertices.size());
    for (int i = 0; i < segments; ++i) {
        float angle = TWO_PI * float(i) / float(segments);
        float x = radiusBottom * cosf(angle);
        float z = radiusBottom * sinf(angle);
        vertices.push_back(Vertex(Vector3(x, -halfH, z), Vector3(0, -1, 0), color));
    }
    
    // 侧面顶点（需要重复顶点以支持径向法线）
    uint32_t sideTopStartIdx = static_cast<uint32_t>(vertices.size());
    for (int i = 0; i < segments; ++i) {
        float angle = TWO_PI * float(i) / float(segments);
        float x = radiusTop * cosf(angle);
        float z = radiusTop * sinf(angle);
        
        // 侧面法线：径向向外（归一化的 XZ 分量）
        Vector3 normal(x, 0, z);
        float length = sqrtf(normal.x * normal.x + normal.z * normal.z);
        if (length > 0.0001f) {
            normal.x /= length;
            normal.z /= length;
        }
        
        vertices.push_back(Vertex(Vector3(x, halfH, z), normal, color));
    }
    
    uint32_t sideBottomStartIdx = static_cast<uint32_t>(vertices.size());
    for (int i = 0; i < segments; ++i) {
        float angle = TWO_PI * float(i) / float(segments);
        float x = radiusBottom * cosf(angle);
        float z = radiusBottom * sinf(angle);
        
        // 侧面法线：径向向外
        Vector3 normal(x, 0, z);
        float length = sqrtf(normal.x * normal.x + normal.z * normal.z);
        if (length > 0.0001f) {
            normal.x /= length;
            normal.z /= length;
        }
        
        vertices.push_back(Vertex(Vector3(x, -halfH, z), normal, color));
    }
    
    // 顶面三角形（从上往下看，顺时针 = 正面朝上）
    // 注意：左手坐标系中，顺时针是正面
    for (int i = 0; i < segments; ++i) {
        int next = (i + 1) % segments;
        indices.push_back(topCenterIdx);
        indices.push_back(topStartIdx + next);
        indices.push_back(topStartIdx + i);
    }
    
    // 底面三角形（从下往上看，顺时针 = 正面朝下）
    for (int i = 0; i < segments; ++i) {
        int next = (i + 1) % segments;
        indices.push_back(bottomCenterIdx);
        indices.push_back(bottomStartIdx + i);
        indices.push_back(bottomStartIdx + next);
    }
    
    // 侧面三角形（从外部看，顺时针）
    for (int i = 0; i < segments; ++i) {
        int next = (i + 1) % segments;
        
        int t0 = sideTopStartIdx + i;
        int t1 = sideTopStartIdx + next;
        int b0 = sideBottomStartIdx + i;
        int b1 = sideBottomStartIdx + next;
        
        // 第一个三角形：t0 -> t1 -> b0
        indices.push_back(t0);
        indices.push_back(t1);
        indices.push_back(b0);
        
        // 第二个三角形：t1 -> b1 -> b0
        indices.push_back(t1);
        indices.push_back(b1);
        indices.push_back(b0);
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
            
            // 圆环法线：从管道中心指向表面
            // 管道中心在 (majorRadius * cosu, 0, majorRadius * sinu)
            Vector3 tubeCenter(majorRadius * cosu, 0, majorRadius * sinu);
            Vector3 position(x, y, z);
            Vector3 normal(position.x - tubeCenter.x, position.y - tubeCenter.y, position.z - tubeCenter.z);
            
            float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (length > 0.0001f) {
                normal.x /= length;
                normal.y /= length;
                normal.z /= length;
            }
            
            vertices.push_back(Vertex(position, normal, color));
        }
    }
    
    // 索引生成（顺时针）
    // 注意：左手坐标系中，顺时针是正面
    for (int i = 0; i < majorSegments; ++i) {
        for (int j = 0; j < minorSegments; ++j) {
            int current = i * (minorSegments + 1) + j;
            int next = current + minorSegments + 1;
            
            indices.push_back(current);
            indices.push_back(current + 1);
            indices.push_back(next);
            
            indices.push_back(current + 1);
            indices.push_back(next + 1);
            indices.push_back(next);
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
            
            // 半球法线：从半球中心指向表面
            Vector3 sphereCenter(0, halfCylinder, 0);
            Vector3 position(x, y, z);
            Vector3 normal(position.x - sphereCenter.x, position.y - sphereCenter.y, position.z - sphereCenter.z);
            
            float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (length > 0.0001f) {
                normal.x /= length;
                normal.y /= length;
                normal.z /= length;
            }
            
            vertices.push_back(Vertex(position, normal, color));
        }
    }
    
    // 圆柱体中段
    for (int seg = 0; seg <= segments; ++seg) {
        float theta = TWO_PI * float(seg) / float(segments);
        float x = radius * cosf(theta);
        float z = radius * sinf(theta);
        
        // 圆柱侧面法线：径向向外
        Vector3 normal(x, 0, z);
        float length = sqrtf(normal.x * normal.x + normal.z * normal.z);
        if (length > 0.0001f) {
            normal.x /= length;
            normal.z /= length;
        }
        
        vertices.push_back(Vertex(Vector3(x, halfCylinder, z), normal, color));
        vertices.push_back(Vertex(Vector3(x, -halfCylinder, z), normal, color));
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
            
            // 半球法线：从半球中心指向表面
            Vector3 sphereCenter(0, -halfCylinder, 0);
            Vector3 position(x, y, z);
            Vector3 normal(position.x - sphereCenter.x, position.y - sphereCenter.y, position.z - sphereCenter.z);
            
            float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (length > 0.0001f) {
                normal.x /= length;
                normal.y /= length;
                normal.z /= length;
            }
            
            vertices.push_back(Vertex(position, normal, color));
        }
    }
    
    // 上半球索引（顺时针）
    // 注意：左手坐标系中，顺时针是正面
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            int current = ring * (segments + 1) + seg;
            int next = current + segments + 1;
            
            indices.push_back(current);
            indices.push_back(current + 1);
            indices.push_back(next);
            
            indices.push_back(current + 1);
            indices.push_back(next + 1);
            indices.push_back(next);
        }
    }
    
    // 圆柱体索引（顺时针）
    for (int seg = 0; seg < segments; ++seg) {
        int t0 = cylinderStartIdx + seg * 2;
        int t1 = cylinderStartIdx + (seg + 1) * 2;
        int b0 = t0 + 1;
        int b1 = t1 + 1;
        
        indices.push_back(t0);
        indices.push_back(t1);
        indices.push_back(b0);
        
        indices.push_back(t1);
        indices.push_back(b1);
        indices.push_back(b0);
    }
    
    // 下半球索引（顺时针）
    for (int ring = 0; ring < rings; ++ring) {
        for (int seg = 0; seg < segments; ++seg) {
            int current = bottomStartIdx + ring * (segments + 1) + seg;
            int next = current + segments + 1;
            
            indices.push_back(current);
            indices.push_back(current + 1);
            indices.push_back(next);
            
            indices.push_back(current + 1);
            indices.push_back(next + 1);
            indices.push_back(next);
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
    
    // Quad 法线朝向 +Z
    Vector3 normal(0, 0, 1);
    
    std::vector<Vertex> vertices = {
        Vertex(Vector3(-halfW, -halfH, 0), normal, color),  // 左下
        Vertex(Vector3( halfW, -halfH, 0), normal, color),  // 右下
        Vertex(Vector3( halfW,  halfH, 0), normal, color),  // 右上
        Vertex(Vector3(-halfW,  halfH, 0), normal, color)   // 左上
    };
    
    std::vector<uint32_t> indices = {
        0, 1, 2,   // 第一个三角形
        0, 2, 3    // 第二个三角形
    };
    
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    
    return mesh;
}

static Moon::Vector3 NormalizeSafe(const Moon::Vector3& v)
{
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-6f) return { 0, 1, 0 };
    return { v.x / len, v.y / len, v.z / len };
}

template<typename T> T clamp(const T& v, const T& lo, const T& hi) { return std::max(lo, std::min(v, hi)); }

// 采样 height，自动 clamp 到边界
static float SampleH(int x, int z, int w, int h, const float* heights)
{
    x = clamp(x, 0, w - 1);
    z = clamp(z, 0, h - 1);
    return heights[z * w + x];
}

// ============================================================================
// 地形网格生成：从高度图创建地形网格
// ============================================================================
//
// ⚠️ 重要：地形坐标系与高度图存储顺序
//
// 高度图存储：heights[z * resolutionX + x]
//   - 第一索引：Z（行索引，从前到后）
//   - 第二索引：X（列索引，从左到右）
//   - 行优先存储（类似图像的行列存储）
//
// 地形坐标系：
//   - X轴：水平方向（左右），对应高度图的列
//   - Y轴：垂直方向（高度）
//   - Z轴：水平方向（前后），对应高度图的行
//   - 地形在XZ平面，Y表示高度
//
// 顶点位置计算：
//   v.position = { x * cellSizeX - halfW, h0, z * cellSizeZ - halfH }
//   其中 h0 = heights[z * resolutionX + x]，Z是第一索引
//
// 💡 这个存储顺序决定了河流坐标的转换规则（见GetPoint函数注释）
//
Mesh* MeshGenerator::CreateTerrainFromHeightmap(int resolutionX, int resolutionZ, const float* heights, float terrainWidth, float terrainDepth, float heightScale, bool generateNormals)
{
    if (resolutionX < 2 || resolutionZ < 2 || !heights) {
        return nullptr;
    }

    std::vector<Moon::Vertex> vertices;
    std::vector<uint32_t> indices;

    vertices.resize(static_cast<size_t>(resolutionX) * static_cast<size_t>(resolutionZ));

    // 1) 生成顶点：以中心为原点，这样 terrain 默认在世界中心
    //    根据实际地形尺寸和采样分辨率计算每个格子的大小
    float cellSizeX = terrainWidth / (resolutionX - 1);
    float cellSizeZ = terrainDepth / (resolutionZ - 1);
    float halfW = terrainWidth * 0.5f;
    float halfH = terrainDepth * 0.5f;

    for (int z = 0; z < resolutionZ; ++z) {
        for (int x = 0; x < resolutionX; ++x) {
            float h0 = heights[z * resolutionX + x] * heightScale;  // ⚠️ 注意：Z是第一索引（行）

            Moon::Vertex v;
            v.position = { x * cellSizeX - halfW, h0, z * cellSizeZ - halfH };

            // 先给默认法线，后面再算
            v.normal = { 0, 1, 0 };

            // terrain 先用白色，避免顶点色影响
            v.colorR = v.colorG = v.colorB = 1.0f;
            v.colorA = 1.0f;

            // UV：0~1（后续你可以在 shader 里做 tiling）
            v.uv = { (float)x / (float)(resolutionX - 1), (float)z / (float)(resolutionZ - 1) };

            vertices[z * resolutionX + x] = v;
        }
    }

    // 2) 生成索引：每个格子 2 个三角形
    indices.reserve(static_cast<size_t>(resolutionX - 1) * static_cast<size_t>(resolutionZ - 1) * 6);

    for (int z = 0; z < resolutionZ - 1; ++z) {
        for (int x = 0; x < resolutionX - 1; ++x) {
            uint32_t i0 = (uint32_t)(z * resolutionX + x);
            uint32_t i1 = (uint32_t)(z * resolutionX + x + 1);
            uint32_t i2 = (uint32_t)((z + 1) * resolutionX + x);
            uint32_t i3 = (uint32_t)((z + 1) * resolutionX + x + 1);

            // 统一绕序（假设你引擎是右手系/左手系都没关系，关键是 consistent）
            // (i0, i2, i1) + (i1, i2, i3)
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    // 3) 法线：用中心差分近似地形梯度
    if (generateNormals) {
        for (int z = 0; z < resolutionZ; ++z) {
            for (int x = 0; x < resolutionX; ++x) {

                float hl = SampleH(x - 1, z, resolutionX, resolutionZ, heights) * heightScale;
                float hr = SampleH(x + 1, z, resolutionX, resolutionZ, heights) * heightScale;
                float hd = SampleH(x, z - 1, resolutionX, resolutionZ, heights) * heightScale;
                float hu = SampleH(x, z + 1, resolutionX, resolutionZ, heights) * heightScale;

                // 地形切线方向
                // dX = (2*cellSizeX, hr - hl, 0)
                // dZ = (0, hu - hd, 2*cellSizeZ)
                Moon::Vector3 dX = { 2.0f * cellSizeX, hr - hl, 0.0f };
                Moon::Vector3 dZ = { 0.0f, hu - hd, 2.0f * cellSizeZ };

                // 法线 = normalize(cross(dZ, dX)) 或 cross(dX,dZ) 取决于绕序
                // 这里用 cross(dZ, dX) 让 y 正向更稳定
                Moon::Vector3 n = {
                    dZ.y * dX.z - dZ.z * dX.y,
                    dZ.z * dX.x - dZ.x * dX.z,
                    dZ.x * dX.y - dZ.y * dX.x
                };

                vertices[z * resolutionX + x].normal = NormalizeSafe(n);
            }
        }
    }

    Mesh* mesh = new Mesh();
    mesh->SetVertices(std::move(vertices));
    mesh->SetIndices(std::move(indices));
    return mesh;
}

// ============================================================================
// 坐标系转换函数：Web编辑器 -> Moon引擎
// ============================================================================
// 
// ⚠️ 重要：Web编辑器（Three.js）与Moon引擎的坐标系映射规则
//
// Web编辑器坐标系 (Three.js 右手系):
//   - X轴: 水平方向（左右）
//   - Y轴: 垂直方向（上下，高度）
//   - Z轴: 水平方向（前后）
//   - 地形在XZ平面，Y表示高度
//
// Moon引擎坐标系 (左手系):
//   - X轴: 水平方向
//   - Y轴: 垂直方向（上下，高度）
//   - Z轴: 水平方向
//   - 地形在XZ平面，Y表示高度
//   - 地形顶点按 heights[z * resolutionX + x] 存储（行优先，Z是行索引）
//
// 坐标转换规则 (经过实际测试验证):
//   Moon X = Web Z  (交换轴，匹配地形的行列存储顺序)
//   Moon Y = Web Y  (高度不变)
//   Moon Z = Web X  (交换轴，匹配地形的行列存储顺序)
//
// 💡 为什么要交换X和Z？
//   因为地形生成函数使用 heights[z * resX + x]，其中Z是第一索引（行），X是第二索引（列）
//   而Web编辑器导出时，坐标顺序与此不同，需要交换才能让河流贴合地形
//
inline Vector3 GetPoint(const std::vector<float>& pts, int i)
{
    float webX = pts[i * 3 + 0];
    float webY = pts[i * 3 + 1];
    float webZ = pts[i * 3 + 2];
    
    return {
        webZ,      // Moon X = Web Z
        webY,      // Moon Y = Web Y
        webX       // Moon Z = Web X
    };
}

// Catmull-Rom 样条曲线插值（类似THREE.CatmullRomCurve3）
static Vector3 CatmullRomInterpolate(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t, float tension = 0.5f)
{
    float t2 = t * t;
    float t3 = t2 * t;
    
    // Catmull-Rom 基函数
    float s = (1.0f - tension) * 0.5f;
    
    float b0 = -s * t3 + 2.0f * s * t2 - s * t;
    float b1 = (2.0f - s) * t3 + (s - 3.0f) * t2 + 1.0f;
    float b2 = (s - 2.0f) * t3 + (3.0f - 2.0f * s) * t2 + s * t;
    float b3 = s * t3 - s * t2;
    
    return p0 * b0 + p1 * b1 + p2 * b2 + p3 * b3;
}

// ============================================================================
// 河流网格生成：从样条控制点创建平滑的河流表面
// ============================================================================
// 
// 输入数据来源：Web编辑器导出的河流样条线控制点
//   - points: [x0,y0,z0, x1,y1,z1, ...] Web坐标系
//   - width: 河流宽度
//
// 处理流程：
//   1. 通过GetPoint()函数将Web坐标转换为Moon引擎坐标
//   2. 使用Catmull-Rom样条插值生成平滑曲线（每段10个插值点）
//   3. 沿曲线生成河流两侧边缘顶点
//   4. 创建三角形网格连接两侧边缘
//
// ⚠️ 注意：坐标转换由GetPoint()函数处理，见上方注释
//
Mesh* MeshGenerator::CreateRiverFromPolyline(const std::vector<float>& points, float width)
{
    const int controlPointCount = static_cast<int>(points.size() / 3);
    if (controlPointCount < 2 || width <= 0.0f)
        return new Mesh();

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const float halfWidth = width * 0.5f;
    const Vector3 up = { 0, 1, 0 };
    
    // 根据控制点数量计算插值点数量
    const int segmentsPerControlPoint = 10;  // 每个控制点段插值10个点
    const int totalSegments = (controlPointCount - 1) * segmentsPerControlPoint;
    const int interpolatedPointCount = totalSegments + 1;
    
    vertices.reserve(interpolatedPointCount * 2);
    indices.reserve(totalSegments * 6);

    // ---------- 生成插值后的顶点 ----------
    for (int i = 0; i <= totalSegments; ++i)
    {
        float globalT = static_cast<float>(i) / static_cast<float>(totalSegments);
        float scaledT = globalT * (controlPointCount - 1);
        int segment = static_cast<int>(scaledT);
        segment = std::min(segment, controlPointCount - 2);
        float localT = scaledT - segment;
        
        // 获取4个控制点用于Catmull-Rom插值
        int idx0 = std::max(0, segment - 1);
        int idx1 = segment;
        int idx2 = segment + 1;
        int idx3 = std::min(controlPointCount - 1, segment + 2);
        
        Vector3 p0 = GetPoint(points, idx0);
        Vector3 p1 = GetPoint(points, idx1);
        Vector3 p2 = GetPoint(points, idx2);
        Vector3 p3 = GetPoint(points, idx3);
        
        // Catmull-Rom 样条插值
        Vector3 p = CatmullRomInterpolate(p0, p1, p2, p3, localT, 0.5f);

        // 计算切线方向
        Vector3 dir;
        if (i == 0) {
            Vector3 p_next = CatmullRomInterpolate(p0, p1, p2, p3, localT + 0.01f, 0.5f);
            dir = p_next - p;
        }
        else if (i == totalSegments) {
            Vector3 p_prev = CatmullRomInterpolate(
                GetPoint(points, std::max(0, controlPointCount - 3)),
                GetPoint(points, controlPointCount - 2),
                GetPoint(points, controlPointCount - 1),
                GetPoint(points, controlPointCount - 1),
                0.99f, 0.5f
            );
            dir = p - p_prev;
        }
        else {
            float nextGlobalT = static_cast<float>(i + 1) / static_cast<float>(totalSegments);
            float nextScaledT = nextGlobalT * (controlPointCount - 1);
            int nextSegment = std::min(static_cast<int>(nextScaledT), controlPointCount - 2);
            float nextLocalT = nextScaledT - nextSegment;
            
            Vector3 p_next = CatmullRomInterpolate(
                GetPoint(points, std::max(0, nextSegment - 1)),
                GetPoint(points, nextSegment),
                GetPoint(points, nextSegment + 1),
                GetPoint(points, std::min(controlPointCount - 1, nextSegment + 2)),
                nextLocalT, 0.5f
            );
            dir = p_next - p;
        }

        dir.y = 0.0f;
        dir = NormalizeSafe(dir);

        // 右方向 = up × dir
        Vector3 right = NormalizeSafe(Vector3::Cross(up, dir));

        Vector3 leftPos = p - right * halfWidth;
        Vector3 rightPos = p + right * halfWidth;

        // 左顶点
        Vertex vl{};
        vl.position = leftPos;
        vl.normal = up;
        vl.uv = { 0.0f, globalT };
        vl.colorR = vl.colorG = vl.colorB = 1.0f;
        vl.colorA = 1.0f;

        // 右顶点
        Vertex vr = vl;
        vr.position = rightPos;
        vr.uv.x = 1.0f;

        vertices.push_back(vl);
        vertices.push_back(vr);
    }

    // ---------- 生成索引 ----------
    for (int i = 0; i < totalSegments; ++i)
    {
        uint32_t i0 = i * 2;
        uint32_t i1 = i * 2 + 1;
        uint32_t i2 = (i + 1) * 2;
        uint32_t i3 = (i + 1) * 2 + 1;

        // 两个三角形（注意绕序）
        indices.push_back(i0);
        indices.push_back(i2);
        indices.push_back(i1);

        indices.push_back(i1);
        indices.push_back(i2);
        indices.push_back(i3);
    }

    Mesh* mesh = new Mesh();
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

} // namespace Moon
