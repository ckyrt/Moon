/**
 * @file CSGTest.cpp
 * @brief CSG 布尔运算测试
 * 
 * 测试 Manifold CSG 库的集成和基本功能
 */

#include "CSGTest.h"
#include "../core/CSG/CSGOperations.h"
#include "../core/Geometry/MeshGenerator.h"
#include "../core/Logging/Logger.h"

using namespace Moon;
using namespace Moon::CSG;

namespace Moon {
namespace CSGTest {

bool RunTests() {
    MOON_LOG_INFO("CSGTest", "=== CSG Boolean Operations Test ===");
    
    // 测试 1: 创建 CSG 基础图元
    MOON_LOG_INFO("CSGTest", "\n[Test 1] Creating CSG primitives...");
    
    auto box = CreateCSGBox(2.0f, 2.0f, 2.0f);
    if (box && box->IsValid()) {
        MOON_LOG_INFO("CSGTest", "✓ Box created: %zu vertices, %zu triangles", 
            box->GetVertexCount(), box->GetTriangleCount());
    } else {
        MOON_LOG_ERROR("CSGTest", "✗ Failed to create box");
        return false;
    }
    
    auto sphere = CreateCSGSphere(1.5f, 32);
    if (sphere && sphere->IsValid()) {
        MOON_LOG_INFO("CSGTest", "✓ Sphere created: %zu vertices, %zu triangles", 
            sphere->GetVertexCount(), sphere->GetTriangleCount());
    } else {
        MOON_LOG_ERROR("CSGTest", "✗ Failed to create sphere");
        return false;
    }
    
    auto cylinder = CreateCSGCylinder(1.0f, 3.0f, 32);
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
    
    MOON_LOG_INFO("CSGTest", "\n=== All CSG tests passed! ===");
    
    return true;
}

} // namespace CSGTest
} // namespace Moon
