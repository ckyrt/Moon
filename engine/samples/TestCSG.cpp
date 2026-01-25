#include "TestScenes.h"
#include <Logging/Logger.h>
#include <CSG/CSGOperations.h>
#include "CSGUnitTest.h"
#include <EngineCore.h>
#include <Scene/MeshRenderer.h>
#include <Scene/Material.h>

namespace TestScenes {

    void TestCSG(EngineCore* engine)
    {
        Moon::Scene* scene = engine->GetScene();
        Moon::MeshManager* meshManager = engine->GetMeshManager();
        Moon::TextureManager* textureManager = engine->GetTextureManager();

        // 运行 CSG 测试
        MOON_LOG_INFO("HelloEngine", "Running CSG tests...");
        if (Moon::CSGUnitTest::RunTests()) {
            MOON_LOG_INFO("HelloEngine", "✓ CSG tests passed!");
        }
        else {
            MOON_LOG_ERROR("HelloEngine", "✗ CSG tests failed!");
        }

        // ============================================================================
        // 7. CSG 演示场景 - Box - Sphere (布尔减法)
        // ============================================================================
        MOON_LOG_INFO("Sample", "Creating CSG demonstration object...");

        // 创建 CSG Box（flatShading=false用于布尔操作，保留共享顶点）
        auto csgBox = Moon::CSG::CreateCSGBox(2.0f, 2.0f, 2.0f,
            Moon::Vector3(0, 0, 0),
            Moon::Vector3(0, 0, 0),
            Moon::Vector3(1, 1, 1),
            false);  // 保留共享顶点用于Boolean
        if (csgBox && csgBox->IsValid()) {
            MOON_LOG_INFO("Sample", "CSG Box created: %zu vertices, %zu triangles",
                csgBox->GetVertexCount(), csgBox->GetTriangleCount());
        }

        // 创建 CSG Sphere（flatShading=false用于布尔操作）
        auto csgSphere = Moon::CSG::CreateCSGSphere(1.2f, 32,
            Moon::Vector3(0, 0, 0),
            Moon::Vector3(0, 0, 0),
            Moon::Vector3(1, 1, 1),
            false);  // 保留共享顶点用于Boolean
        if (csgSphere && csgSphere->IsValid()) {
            MOON_LOG_INFO("Sample", "CSG Sphere created: %zu vertices, %zu triangles",
                csgSphere->GetVertexCount(), csgSphere->GetTriangleCount());
        }

        // 执行布尔减法：Box - Sphere（从立方体中挖出球形）
        auto csgResult = Moon::CSG::PerformBoolean(csgBox.get(), csgSphere.get(), Moon::CSG::Operation::Subtract);
        if (csgResult && csgResult->IsValid()) {
            MOON_LOG_INFO("Sample", "CSG Boolean Subtract successful: %zu vertices, %zu triangles",
                csgResult->GetVertexCount(), csgResult->GetTriangleCount());

            // 添加到场景
            Moon::SceneNode* csgNode = scene->CreateNode("CSG_Box_Minus_Sphere");
            csgNode->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, 2.0f, 5.0f));

            Moon::MeshRenderer* csgRenderer = csgNode->AddComponent<Moon::MeshRenderer>();
            csgRenderer->SetMesh(csgResult);

            Moon::Material* csgMaterial = csgNode->AddComponent<Moon::Material>();
            csgMaterial->SetMaterialPreset(Moon::MaterialPreset::Metal);
            csgMaterial->SetMappingMode(Moon::MappingMode::Triplanar);  // ✅ CSG必须用Triplanar
            csgMaterial->SetTriplanarTiling(0.5f);
            csgMaterial->SetTriplanarBlend(4.0f);

            MOON_LOG_INFO("Sample", "CSG object added to scene at position (0, 2, 5)");
        }
        else {
            MOON_LOG_ERROR("Sample", "CSG Boolean operation failed!");
        }

        // ============================================================================
        // 8. UV vs Triplanar 对比测试 - 拉伸的立方体（更明显的差异）
        // ============================================================================
        MOON_LOG_INFO("Sample", "Creating UV vs Triplanar comparison test with stretched cubes...");

        // 8a. 普通立方体 - UV模式 + 拉伸 → 纹理会变形
        Moon::SceneNode* uvCubeNode = scene->CreateNode("UV_Cube_Stretched");
        uvCubeNode->GetTransform()->SetLocalPosition(Moon::Vector3(-3.0f, 2.0f, -3.0f));
        uvCubeNode->GetTransform()->SetLocalScale(Moon::Vector3(1.0f, 3.0f, 1.0f));  // ✅ 拉高3倍

        Moon::MeshRenderer* uvCubeRenderer = uvCubeNode->AddComponent<Moon::MeshRenderer>();
        uvCubeRenderer->SetMesh(meshManager->CreateCube(1.0f, Moon::Vector3(1.0f, 1.0f, 1.0f)));

        Moon::Material* uvCubeMaterial = uvCubeNode->AddComponent<Moon::Material>();
        uvCubeMaterial->SetMaterialPreset(Moon::MaterialPreset::Rock);  // 砖块纹理更明显
        uvCubeMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 显式UV模式
        uvCubeMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ UV模式：拉伸后纹理变形

        MOON_LOG_INFO("Sample", "UV Cube (stretched 1x3x1) created at (-3, 2, -3) - texture will be stretched");

        // 8b. CSG立方体 - Triplanar模式 + 拉伸 → 纹理保持正确比例
        auto csgCube = Moon::CSG::CreateCSGBox(1.0f, 1.0f, 1.0f,
            Moon::Vector3(0, 0, 0),
            Moon::Vector3(0, 0, 0),
            Moon::Vector3(1, 1, 1),
            true);  // flatShading=true

        if (csgCube && csgCube->IsValid()) {
            Moon::SceneNode* triplanarCubeNode = scene->CreateNode("Triplanar_CSG_Cube_Stretched");
            triplanarCubeNode->GetTransform()->SetLocalPosition(Moon::Vector3(3.0f, 2.0f, -3.0f));
            triplanarCubeNode->GetTransform()->SetLocalScale(Moon::Vector3(1.0f, 3.0f, 1.0f));  // ✅ 同样拉高3倍

            Moon::MeshRenderer* triplanarCubeRenderer = triplanarCubeNode->AddComponent<Moon::MeshRenderer>();
            triplanarCubeRenderer->SetMesh(csgCube);

            Moon::Material* triplanarCubeMaterial = triplanarCubeNode->AddComponent<Moon::Material>();
            triplanarCubeMaterial->SetMaterialPreset(Moon::MaterialPreset::Rock);  // 相同砖块纹理
            triplanarCubeMaterial->SetMappingMode(Moon::MappingMode::Triplanar);  // ✅ CSG必须用Triplanar
            triplanarCubeMaterial->SetTriplanarTiling(0.5f);  // 每2米重复
            triplanarCubeMaterial->SetTriplanarBlend(4.0f);

            MOON_LOG_INFO("Sample", "CSG Triplanar Cube (stretched 1x3x1) created at (3, 2, -3) - texture density stays correct");
        }
        else {
            MOON_LOG_ERROR("Sample", "Failed to create CSG test cube!");
        }

        MOON_LOG_INFO("Sample", "✅ UV vs Triplanar comparison: Left=UV (stretched texture), Right=Triplanar (correct density)");

        // ============================================================================
        // 8. 玻璃球体 - 透明材质演示（程序化，无纹理）
        // ============================================================================
        MOON_LOG_INFO("Sample", "Creating glass sphere demonstration...");

        Moon::SceneNode* glassSphereNode = scene->CreateNode("Glass Sphere");
        glassSphereNode->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, 2.0f, -3.0f));  // 中心位置，高度2米
        glassSphereNode->GetTransform()->SetLocalScale(Moon::Vector3(1.2f, 1.2f, 1.2f));     // 稍大一些

        Moon::MeshRenderer* glassRenderer = glassSphereNode->AddComponent<Moon::MeshRenderer>();
        glassRenderer->SetMesh(meshManager->CreateSphere(1.0f, 64, 32, Moon::Vector3(1.0f, 1.0f, 1.0f)));

        Moon::Material* glassMaterial = glassSphereNode->AddComponent<Moon::Material>();
        glassMaterial->SetMaterialPreset(Moon::MaterialPreset::Glass);  // 玻璃预设：透明、光滑、非金属

        MOON_LOG_INFO("Sample", "Glass sphere created at (0, 2, -3) with transparency and PBR lighting");
        MOON_LOG_INFO("Sample", "Material Showcase Scene created with 10 objects (including glass)");

    }

}
