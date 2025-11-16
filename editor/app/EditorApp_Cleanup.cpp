// ============================================================================
// EditorApp_Cleanup.cpp - 资源清理
// ============================================================================
#include "EditorApp.h"
#include "../engine/core/EngineCore.h"
#include "../engine/core/Logging/Logger.h"
#include "../engine/render/DiligentRenderer.h"
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

    if (g_Renderer) {
        g_Renderer->Shutdown();
    }

    if (g_EngineWindow) {
        DestroyWindow(g_EngineWindow);
        g_EngineWindow = nullptr;
    }

    if (g_Engine) {
        g_Engine->Shutdown();
    }

    if (g_EditorBridge) {
        g_EditorBridge->Shutdown();
    }

    Moon::Core::Logger::Shutdown();
}
