
// ============================================================================
// EditorApp_Render.cpp - 场景渲染逻辑
// ============================================================================
#include "EditorApp.h"
#include "../engine/core/EngineCore.h"
#include "../engine/render/SceneRenderer.h"
#include "../engine/render/diligent/DiligentRenderer.h"

// ============================================================================
// 渲染场景（使用通用的场景渲染工具）
// ============================================================================
void RenderScene(EngineCore* engine, DiligentRenderer* renderer)
{
    Moon::SceneRendererUtils::RenderScene(renderer, engine->GetScene(), engine->GetCamera());
}
