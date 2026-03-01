#pragma once
#include <windows.h>

// Forward declarations
class DiligentRenderer;
namespace Moon {
    class PerspectiveCamera;
    class FPSCounter;
}

namespace HelloEngineImGui {

    // 初始化 ImGui 系统
    void Initialize(HWND hwnd, DiligentRenderer* renderer);

    // 清理 ImGui 系统
    void Shutdown();

    // 每帧开始时调用（在 BeginFrame 之前）
    void BeginFrame();

    // 渲染 ImGui UI（在场景渲染之后，EndFrame 之前）
    void RenderUI(Moon::PerspectiveCamera* camera, Moon::FPSCounter* fpsCounter);

    // 渲染调试网格和坐标轴
    void RenderDebugGrid(Moon::PerspectiveCamera* camera);

    // 启用/禁用调试网格（默认启用）
    void EnableDebugGrid(bool enable);
    bool IsDebugGridEnabled();

    // Win32 消息处理
    bool HandleWin32Message(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

} // namespace HelloEngineImGui
