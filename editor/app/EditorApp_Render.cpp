
// ============================================================================
// EditorApp_Render.cpp - 场景渲染逻辑
// ============================================================================
#include "EditorApp.h"
#include "../engine/core/EngineCore.h"
#include "../engine/core/Scene/Scene.h"
#include "../engine/core/Scene/SceneNode.h"
#include "../engine/core/Scene/MeshRenderer.h"
#include "../engine/core/Scene/Material.h"
#include "../engine/core/Camera/Camera.h"
#include "../engine/render/diligent/DiligentRenderer.h"

// ============================================================================
// 渲染场景中的所有 MeshRenderer
// ============================================================================
void RenderScene(EngineCore* engine, DiligentRenderer* renderer)
{
    // 设置 ViewProjection 矩阵
    auto vp = engine->GetCamera()->GetViewProjectionMatrix();
    renderer->SetViewProjectionMatrix(&vp.m[0][0]);

    // 遍历场景渲染所有启用的 MeshRenderer
    engine->GetScene()->Traverse([&](Moon::SceneNode* node) {
        auto* mr = node->GetComponent<Moon::MeshRenderer>();
        if (mr && mr->IsEnabled() && mr->IsVisible()) {
            // 从 Material 组件获取材质参数和纹理
            Moon::Material* material = node->GetComponent<Moon::Material>();
            if (material && material->IsEnabled()) {
                // 设置材质参数
                renderer->SetMaterialParameters(
                    material->GetMetallic(),
                    material->GetRoughness(),
                    material->GetBaseColor()
                );

                // 绑定 Albedo 贴图
                if (material->HasAlbedoMap()) {
                    renderer->BindAlbedoTexture(material->GetAlbedoMap());
                } else {
                    renderer->BindAlbedoTexture("");  // 使用默认白色纹理
                }

                // 绑定 ARM 贴图
                if (material->HasMetallicMap()) {
                    renderer->BindARMTexture(material->GetMetallicMap());
                } else {
                    renderer->BindARMTexture("");  // 使用默认纹理
                }

                // 绑定法线贴图
                if (material->HasNormalMap()) {
                    renderer->BindNormalTexture(material->GetNormalMap());
                } else {
                    renderer->BindNormalTexture("");  // 使用默认纹理
                }
            } else {
                // ⚠️ 对于没有 Material 组件的旧对象，使用默认材质
                // （新创建的对象都会有 Material，这是为了向后兼容）
                renderer->SetMaterialParameters(0.0f, 0.5f, Moon::Vector3(1.0f, 1.0f, 1.0f));
                renderer->BindAlbedoTexture("");
                renderer->BindARMTexture("");
                renderer->BindNormalTexture("");
            }

            mr->Render(renderer);
        }
    });
}
