// ============================================================================
// EditorApp_Cleanup.cpp - 资源清理
// ============================================================================
#include "EditorApp.h"
#include "UITextureManager.h"
#include "UIRenderer.h"
#include "../engine/core/EngineCore.h"
#include "../engine/core/Logging/Logger.h"
#include "../engine/render/diligent/DiligentRenderer.h"
#include "EditorBridge.h"
#include "ImGuiImplWin32.hpp"
#include <Windows.h>

// ============================================================================
// 清理所有资源
// ============================================================================
void CleanupResources()
{
    if (g_ImGuiWin32) {
        delete g_ImGuiWin32;
        g_ImGuiWin32 = nullptr;
    }

    // ✅ 清理 UI 渲染系统
    if (g_UIRenderer) {
        delete g_UIRenderer;
        g_UIRenderer = nullptr;
    }
    if (g_UITextureManager) {
        delete g_UITextureManager;
        g_UITextureManager = nullptr;
    }

    if (g_Renderer) {
        g_Renderer->Shutdown();
    }

    if (g_Engine) {
        g_Engine->Shutdown();
    }

    if (g_EditorBridge) {
        g_EditorBridge->Shutdown();
    }

    Moon::Core::Logger::Shutdown();
}
