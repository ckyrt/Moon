
// ============================================================================
// EditorApp_Render.cpp - 场景渲染逻辑
// ============================================================================
#include "EditorApp.h"
#include "../engine/core/EngineCore.h"
#include "../engine/core/Scene/Scene.h"
#include "../engine/core/Scene/SceneNode.h"
#include "../engine/core/Scene/MeshRenderer.h"
#include "../engine/core/Camera/Camera.h"
#include "../engine/render/DiligentRenderer.h"

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
        if (mr && mr->IsEnabled() && mr->IsVisible())
            mr->Render(renderer);
    });
}
