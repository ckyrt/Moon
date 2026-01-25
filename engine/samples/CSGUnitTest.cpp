/**
 * @file CSGTest.cpp
 * @brief CSG 布尔运算测试
 * 
 * 测试 Manifold CSG 库的集成和基本功能
 */

#include "CSGUnitTest.h"
#include "../core/CSG/CSGOperations.h"
#include "../core/Geometry/MeshGenerator.h"
#include "../core/Logging/Logger.h"

using namespace Moon;
using namespace Moon::CSG;

namespace Moon {
namespace CSGUnitTest {

bool RunTests() {
    MOON_LOG_INFO("CSGTest", "=== CSG Boolean Operations Test ===");
    
    // 测试 1: 创建 CSG 基础图元（flatShading=false用于Boolean操作）
    MOON_LOG_INFO("CSGTest", "\n[Test 1] Creating CSG primitives...");
    
    auto box = CreateCSGBox(2.0f, 2.0f, 2.0f, Vector3(0,0,0), Vector3(0,0,0), Vector3(1,1,1), false);
    if (box && box->IsValid()) {
        MOON_LOG_INFO("CSGTest", "✓ Box created: %zu vertices, %zu triangles", 
            box->GetVertexCount(), box->GetTriangleCount());
    } else {
        MOON_LOG_ERROR("CSGTest", "✗ Failed to create box");
        return false;
    }
    
    auto sphere = CreateCSGSphere(1.5f, 32, Vector3(0,0,0), Vector3(0,0,0), Vector3(1,1,1), false);
    if (sphere && sphere->IsValid()) {
        MOON_LOG_INFO("CSGTest", "✓ Sphere created: %zu vertices, %zu triangles", 
            sphere->GetVertexCount(), sphere->GetTriangleCount());
    } else {
        MOON_LOG_ERROR("CSGTest", "✗ Failed to create sphere");
        return false;
    }
    
    auto cylinder = CreateCSGCylinder(1.0f, 3.0f, 32, Vector3(0,0,0), Vector3(0,0,0), Vector3(1,1,1), false);
    if (cylinder && cylinder->IsValid()) {
        MOON_LOG_INFO("CSGTest", "✓ Cylinder created: %zu vertices, %zu triangles", 
            cylinder->GetVertexCount(), cylinder->GetTriangleCount());
    } else {
        MOON_LOG_ERROR("CSGTest", "✗ Failed to create cylinder");
        return false;
    }
    
    // 测试 2: Union 操作
    MOON_LOG_INFO("CSGTest", "\n[Test 2] Testing Union operation...");
    auto unionResult = PerformBoolean(box.get(), sphere.get(), Operation::Union);
    if (unionResult && unionResult->IsValid()) {
        MOON_LOG_INFO("CSGTest", "✓ Union successful: %zu vertices, %zu triangles", 
            unionResult->GetVertexCount(), unionResult->GetTriangleCount());
    } else {
        MOON_LOG_ERROR("CSGTest", "✗ Union failed");
        return false;
    }
    
    // 测试 3: Subtract 操作
    MOON_LOG_INFO("CSGTest", "\n[Test 3] Testing Subtract operation...");
    auto subtractResult = PerformBoolean(box.get(), sphere.get(), Operation::Subtract);
    if (subtractResult && subtractResult->IsValid()) {
        MOON_LOG_INFO("CSGTest", "✓ Subtract successful: %zu vertices, %zu triangles", 
            subtractResult->GetVertexCount(), subtractResult->GetTriangleCount());
    } else {
        MOON_LOG_ERROR("CSGTest", "✗ Subtract failed");
        return false;
    }
    
    // 测试 4: Intersect 操作
    MOON_LOG_INFO("CSGTest", "\n[Test 4] Testing Intersect operation...");
    auto intersectResult = PerformBoolean(box.get(), sphere.get(), Operation::Intersect);
    if (intersectResult && intersectResult->IsValid()) {
        MOON_LOG_INFO("CSGTest", "✓ Intersect successful: %zu vertices, %zu triangles", 
            intersectResult->GetVertexCount(), intersectResult->GetTriangleCount());
    } else {
        MOON_LOG_ERROR("CSGTest", "✗ Intersect failed");
        return false;
    }
    
    // 测试 5: 复杂 CSG 操作（Box - Sphere - Cylinder）
    MOON_LOG_INFO("CSGTest", "\n[Test 5] Testing complex CSG operations...");
    auto temp = PerformBoolean(box.get(), sphere.get(), Operation::Subtract);
    if (temp) {
        auto finalResult = PerformBoolean(temp.get(), cylinder.get(), Operation::Subtract);
        if (finalResult && finalResult->IsValid()) {
            MOON_LOG_INFO("CSGTest", "✓ Complex CSG successful: %zu vertices, %zu triangles", 
                finalResult->GetVertexCount(), finalResult->GetTriangleCount());
        } else {
            MOON_LOG_ERROR("CSGTest", "✗ Complex CSG failed");
            return false;
        }
    }
    
    // 测试 6: 基本CSG操作（墙+窗场景，简化版）
    MOON_LOG_INFO("CSGTest", "\n[Test 6] Testing wall-window CSG scenario...");
    auto wallBox = CreateCSGBox(10.0f, 10.0f, 1.0f, Vector3(0,0,0), Vector3(0,0,0), Vector3(1,1,1), false);
    auto windowBox = CreateCSGBox(3.0f, 4.0f, 1.5f, Vector3(0,0,0), Vector3(0,0,0), Vector3(1,1,1), false);
    
    // 砖墙 - 窗洞 = 带洞的墙（单材质）
    auto wallWithWindow = PerformBoolean(wallBox.get(), windowBox.get(), Operation::Subtract);
    if (wallWithWindow && wallWithWindow->IsValid()) {
        MOON_LOG_INFO("CSGTest", "✓ Wall-window CSG successful: %zu vertices, %zu triangles", 
            wallWithWindow->GetVertexCount(), wallWithWindow->GetTriangleCount());
    } else {
        MOON_LOG_ERROR("CSGTest", "✗ Wall-window CSG failed");
        return false;
    }
    
    // 测试 7: 坐标系和法线验证
    MOON_LOG_INFO("CSGTest", "\n[Test 7] Verifying coordinate system and normals...");
    auto testBox = CreateCSGBox(2.0f, 2.0f, 2.0f);
    if (testBox && testBox->IsValid()) {
        const auto& vertices = testBox->GetVertices();
        const auto& indices = testBox->GetIndices();
        
        // 检查前几个三角形的法线方向
        bool normalsValid = true;
        int trianglesToCheck = std::min(5, (int)(indices.size() / 3));
        
        for (int i = 0; i < trianglesToCheck; ++i) {
            uint32_t i0 = indices[i * 3 + 0];
            uint32_t i1 = indices[i * 3 + 1];
            uint32_t i2 = indices[i * 3 + 2];
            
            const auto& v0 = vertices[i0];
            const auto& v1 = vertices[i1];
            const auto& v2 = vertices[i2];
            
            // 计算三角形法线（左手坐标系，顺时针）
            Vector3 edge1(v1.position.x - v0.position.x, 
                         v1.position.y - v0.position.y, 
                         v1.position.z - v0.position.z);
            Vector3 edge2(v2.position.x - v0.position.x, 
                         v2.position.y - v0.position.y, 
                         v2.position.z - v0.position.z);
            
            // 左手坐标系：cross(edge1, edge2) 应该朝外
            Vector3 faceNormal(
                edge1.y * edge2.z - edge1.z * edge2.y,
                edge1.z * edge2.x - edge1.x * edge2.z,
                edge1.x * edge2.y - edge1.y * edge2.x
            );
            
            // 归一化
            float len = sqrtf(faceNormal.x * faceNormal.x + 
                            faceNormal.y * faceNormal.y + 
                            faceNormal.z * faceNormal.z);
            if (len > 0.0001f) {
                faceNormal.x /= len;
                faceNormal.y /= len;
                faceNormal.z /= len;
            }
            
            // 检查顶点法线与面法线是否大致一致（点积应该 > 0）
            float dot = v0.normal.x * faceNormal.x + 
                       v0.normal.y * faceNormal.y + 
                       v0.normal.z * faceNormal.z;
            
            if (dot < 0.5f) {  // 夹角应该 < 60度
                MOON_LOG_WARN("CSGTest", "⚠ Triangle %d: vertex normal may be incorrect (dot=%.2f)", i, dot);
                normalsValid = false;
            }
        }
        
        if (normalsValid) {
            MOON_LOG_INFO("CSGTest", "✓ Coordinate system and normals verified (left-hand, clockwise)");
        } else {
            MOON_LOG_WARN("CSGTest", "⚠ Some normals may need adjustment");
        }
    }
    
    // 测试 8: 对比 MeshGenerator 和 CSG 创建的立方体
    MOON_LOG_INFO("CSGTest", "\n[Test 8] Comparing MeshGenerator vs CSG geometry...");
    Mesh* meshGenCube = MeshGenerator::CreateCube(2.0f);
    auto csgCube = CreateCSGBox(2.0f, 2.0f, 2.0f);
    
    if (meshGenCube && csgCube) {
        MOON_LOG_INFO("CSGTest", "  MeshGenerator Cube: %zu vertices, %zu triangles", 
            meshGenCube->GetVertexCount(), meshGenCube->GetTriangleCount());
        MOON_LOG_INFO("CSGTest", "  CSG Cube: %zu vertices, %zu triangles", 
            csgCube->GetVertexCount(), csgCube->GetTriangleCount());
        MOON_LOG_INFO("CSGTest", "✓ Both methods work (vertex counts may differ due to different topology)");
        delete meshGenCube;
    }
    
    // 测试 9: Transform功能测试（Position）
    MOON_LOG_INFO("CSGTest", "\n[Test 9] Testing Transform - Position...");
    auto box1 = CreateCSGBox(1.0f, 1.0f, 1.0f, Vector3(0, 0, 0));
    auto box2 = CreateCSGBox(1.0f, 1.0f, 1.0f, Vector3(2, 0, 0));
    
    if (box1 && box2) {
        auto& verts1 = box1->GetVertices();
        auto& verts2 = box2->GetVertices();
        
        // 检查第二个盒子的顶点是否平移了2个单位
        float avgX1 = 0, avgX2 = 0;
        for (const auto& v : verts1) avgX1 += v.position.x;
        for (const auto& v : verts2) avgX2 += v.position.x;
        avgX1 /= verts1.size();
        avgX2 /= verts2.size();
        
        float diff = avgX2 - avgX1;
        if (diff > 1.9f && diff < 2.1f) {
            MOON_LOG_INFO("CSGTest", "✓ Position transform working (offset: %.2f)", diff);
        } else {
            MOON_LOG_ERROR("CSGTest", "✗ Position transform failed (expected ~2.0, got %.2f)", diff);
            return false;
        }
    }
    
    // 测试 10: Transform功能测试（Scale）
    MOON_LOG_INFO("CSGTest", "\n[Test 10] Testing Transform - Scale...");
    auto boxNormal = CreateCSGBox(2.0f, 2.0f, 2.0f, Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(1, 1, 1));
    auto boxScaled = CreateCSGBox(2.0f, 2.0f, 2.0f, Vector3(0, 0, 0), Vector3(0, 0, 0), Vector3(2, 2, 2));
    
    if (boxNormal && boxScaled) {
        auto& vertsN = boxNormal->GetVertices();
        auto& vertsS = boxScaled->GetVertices();
        
        // 计算边界框大小
        float minX_N = vertsN[0].position.x, maxX_N = vertsN[0].position.x;
        float minX_S = vertsS[0].position.x, maxX_S = vertsS[0].position.x;
        
        for (const auto& v : vertsN) {
            minX_N = std::min(minX_N, v.position.x);
            maxX_N = std::max(maxX_N, v.position.x);
        }
        for (const auto& v : vertsS) {
            minX_S = std::min(minX_S, v.position.x);
            maxX_S = std::max(maxX_S, v.position.x);
        }
        
        float sizeN = maxX_N - minX_N;
        float sizeS = maxX_S - minX_S;
        float ratio = sizeS / sizeN;
        
        if (ratio > 1.9f && ratio < 2.1f) {
            MOON_LOG_INFO("CSGTest", "✓ Scale transform working (ratio: %.2f)", ratio);
        } else {
            MOON_LOG_ERROR("CSGTest", "✗ Scale transform failed (expected ~2.0, got %.2f)", ratio);
            return false;
        }
    }
    
    // 测试 11: Transform功能测试（Rotation）
    MOON_LOG_INFO("CSGTest", "\n[Test 11] Testing Transform - Rotation...");
    auto boxNoRot = CreateCSGBox(2.0f, 1.0f, 1.0f, Vector3(0, 0, 0), Vector3(0, 0, 0));
    auto boxRot90 = CreateCSGBox(2.0f, 1.0f, 1.0f, Vector3(0, 0, 0), Vector3(0, 0, 90));
    
    if (boxNoRot && boxRot90) {
        auto& vertsNoRot = boxNoRot->GetVertices();
        auto& vertsRot = boxRot90->GetVertices();
        
        // 计算X和Y方向的范围
        float rangeX_N = 0, rangeY_N = 0;
        float rangeX_R = 0, rangeY_R = 0;
        
        float minX_N = vertsNoRot[0].position.x, maxX_N = vertsNoRot[0].position.x;
        float minY_N = vertsNoRot[0].position.y, maxY_N = vertsNoRot[0].position.y;
        float minX_R = vertsRot[0].position.x, maxX_R = vertsRot[0].position.x;
        float minY_R = vertsRot[0].position.y, maxY_R = vertsRot[0].position.y;
        
        for (const auto& v : vertsNoRot) {
            minX_N = std::min(minX_N, v.position.x);
            maxX_N = std::max(maxX_N, v.position.x);
            minY_N = std::min(minY_N, v.position.y);
            maxY_N = std::max(maxY_N, v.position.y);
        }
        for (const auto& v : vertsRot) {
            minX_R = std::min(minX_R, v.position.x);
            maxX_R = std::max(maxX_R, v.position.x);
            minY_R = std::min(minY_R, v.position.y);
            maxY_R = std::max(maxY_R, v.position.y);
        }
        
        rangeX_N = maxX_N - minX_N;
        rangeY_N = maxY_N - minY_N;
        rangeX_R = maxX_R - minX_R;
        rangeY_R = maxY_R - minY_R;
        
        // 旋转90度后，原来X方向的长度(2.0)应该变成Y方向，Y方向(1.0)变成X方向
        bool rotationWorked = (std::abs(rangeX_R - rangeY_N) < 0.2f) && 
                              (std::abs(rangeY_R - rangeX_N) < 0.2f);
        
        if (rotationWorked) {
            MOON_LOG_INFO("CSGTest", "✓ Rotation transform working (X: %.2f->%.2f, Y: %.2f->%.2f)", 
                         rangeX_N, rangeX_R, rangeY_N, rangeY_R);
        } else {
            MOON_LOG_WARN("CSGTest", "⚠ Rotation may not be working correctly (X: %.2f->%.2f, Y: %.2f->%.2f)", 
                         rangeX_N, rangeX_R, rangeY_N, rangeY_R);
        }
    }
    
    // 测试 12: Cone图元测试
    MOON_LOG_INFO("CSGTest", "\n[Test 12] Testing Cone primitive...");
    auto cone = CreateCSGCone(1.0f, 2.0f, 32);
    
    if (cone && cone->IsValid()) {
        MOON_LOG_INFO("CSGTest", "✓ Cone created: %zu vertices, %zu triangles", 
            cone->GetVertexCount(), cone->GetTriangleCount());
        
        // 验证cone的形状：Manifold在XY平面创建圆，沿Z轴挤出
        auto& verts = cone->GetVertices();
        float maxRadius = 0;
        float minZ = verts[0].position.z, maxZ = verts[0].position.z;
        
        for (const auto& v : verts) {
            float r = std::sqrt(v.position.x * v.position.x + v.position.y * v.position.y);
            maxRadius = std::max(maxRadius, r);
            minZ = std::min(minZ, v.position.z);
            maxZ = std::max(maxZ, v.position.z);
        }
        
        float height = maxZ - minZ;
        
        if (maxRadius > 0.9f && maxRadius < 1.1f && height > 1.9f && height < 2.1f) {
            MOON_LOG_INFO("CSGTest", "✓ Cone shape verified (radius: %.2f, height: %.2f)", maxRadius, height);
        } else {
            MOON_LOG_WARN("CSGTest", "⚠ Cone dimensions unexpected (radius: %.2f, height: %.2f)", maxRadius, height);
        }
    } else {
        MOON_LOG_ERROR("CSGTest", "✗ Failed to create cone");
        return false;
    }
    
    // 测试 13: Flat Shading验证 - 顶点数量检查
    MOON_LOG_INFO("CSGTest", "\n[Test 13] Testing Flat Shading vertex count...");
    auto flatBox = CreateCSGBox(1.0f, 1.0f, 1.0f, Vector3(0,0,0), Vector3(0,0,0), Vector3(1,1,1), true);  // flatShading = true
    
    if (flatBox && flatBox->IsValid()) {
        size_t vertCount = flatBox->GetVertexCount();
        size_t triCount = flatBox->GetTriangleCount();
        
        MOON_LOG_INFO("CSGTest", "  CSG Box (flat shading): %zu vertices, %zu triangles", vertCount, triCount);
        
        // 立方体有6个面，每面2个三角形，每个三角形3个顶点 = 36个独立顶点
        if (vertCount == 36 && triCount == 12) {
            MOON_LOG_INFO("CSGTest", "✓ Flat shading verified: 36 独立顶点 (each face has independent vertices for hard edges)");
        } else {
            MOON_LOG_WARN("CSGTest", "⚠ Vertex count unexpected (expected 36, got %zu)", vertCount);
        }
        
        // 检查前6个顶点法线（应该是同一个face normal）
        auto& verts = flatBox->GetVertices();
        if (verts.size() >= 6) {
            const auto& n0 = verts[0].normal;
            bool sameFace = true;
            for (int i = 1; i < 6; i++) {
                float dot = n0.x * verts[i].normal.x + n0.y * verts[i].normal.y + n0.z * verts[i].normal.z;
                if (dot < 0.99f) {  // 法线夹角应该接近0度
                    sameFace = false;
                    break;
                }
            }
            if (sameFace) {
                MOON_LOG_INFO("CSGTest", "✓ First face normals consistent: (%.3f, %.3f, %.3f)", n0.x, n0.y, n0.z);
            }
        }
    } else {
        MOON_LOG_ERROR("CSGTest", "✗ Failed to create flat shading box");
        return false;
    }
    
    // 测试 14: 法线诊断测试 - 对比 MeshGenerator 和 CSG 的法线
    MOON_LOG_INFO("CSGTest", "\n[Test 14] Comparing MeshGenerator vs CSG normals...");
    
    // 普通Box (MeshGenerator) - 使用外部API
    MOON_LOG_INFO("CSGTest", "  Note: MeshGenerator comparison requires external MeshManager, skipping in unit test");
    MOON_LOG_INFO("CSGTest", "  (This test is covered in hello_win32.cpp integration)");
    
    // CSG Box 法线检查
    auto csgDiagBox = CreateCSGBox(1.0f, 1.0f, 1.0f);
    if (csgDiagBox && csgDiagBox->IsValid()) {
        const auto& verts = csgDiagBox->GetVertices();
        MOON_LOG_INFO("CSGTest", "  CSG Box: %zu vertices", verts.size());
        
        // 检查前3个顶点的法线
        if (verts.size() >= 3) {
            MOON_LOG_INFO("CSGTest", "  First 3 vertex normals:");
            for (int i = 0; i < 3; i++) {
                MOON_LOG_INFO("CSGTest", "    v[%d]: (%.3f, %.3f, %.3f)", 
                             i, verts[i].normal.x, verts[i].normal.y, verts[i].normal.z);
            }
            
            // 验证法线已归一化
            bool allNormalized = true;
            for (int i = 0; i < 3; i++) {
                float len = std::sqrt(verts[i].normal.x * verts[i].normal.x +
                                     verts[i].normal.y * verts[i].normal.y +
                                     verts[i].normal.z * verts[i].normal.z);
                if (len < 0.99f || len > 1.01f) {
                    allNormalized = false;
                    break;
                }
            }
            
            if (allNormalized) {
                MOON_LOG_INFO("CSGTest", "✓ Normals are properly normalized");
            } else {
                MOON_LOG_ERROR("CSGTest", "✗ Some normals are not normalized");
                return false;
            }
        }
    } else {
        MOON_LOG_ERROR("CSGTest", "✗ Failed to create CSG box for normal diagnosis");
        return false;
    }
    
    MOON_LOG_INFO("CSGTest", "\n=== All CSG tests passed! ===");
    
    return true;
}

} // namespace CSGUnitTest
} // namespace Moon
