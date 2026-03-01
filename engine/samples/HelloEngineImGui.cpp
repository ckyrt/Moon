#include "HelloEngineImGui.h"
#include "../core/Camera/Camera.h"
#include "../core/Camera/PerspectiveCamera.h"
#include "../core/Profiling/FPSCounter.h"
#include "../render/diligent/DiligentRenderer.h"
#include "DebugGridRenderer.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include "ImGuiImplWin32.hpp"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"

namespace HelloEngineImGui {

static ImGuiContext* g_ImGuiContext = nullptr;
static Diligent::ImGuiImplWin32* g_ImGuiWin32 = nullptr;
static DiligentRenderer* g_Renderer = nullptr;
static bool g_DebugGridEnabled = true;   // 调试网格开关

static DebugGridRenderer g_DebugGridRenderer;

} // namespace HelloEngineImGui

// Forward declare ImGui Win32 message handler (global scope)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace HelloEngineImGui {

void Initialize(HWND hwnd, DiligentRenderer* renderer)
{
    g_Renderer = renderer;
    g_DebugGridRenderer.Initialize(renderer);

    IMGUI_CHECKVERSION();
    g_ImGuiContext = ImGui::CreateContext();
    ImGui::SetCurrentContext(g_ImGuiContext);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // 初始化 ImGui Win32 (使用 Diligent 的实现)
    Diligent::ImGuiDiligentCreateInfo ci;
    ci.pDevice = renderer->GetDevice();
    ci.BackBufferFmt = renderer->GetSwapChain()->GetDesc().ColorBufferFormat;
    ci.DepthBufferFmt = renderer->GetSwapChain()->GetDesc().DepthBufferFormat;
    
    g_ImGuiWin32 = new Diligent::ImGuiImplWin32(ci, hwnd);
    
    ImGui::StyleColorsDark();
}

void Shutdown()
{
    if (g_ImGuiWin32) {
        delete g_ImGuiWin32;
        g_ImGuiWin32 = nullptr;
    }

    g_DebugGridRenderer.Shutdown();

    // 注意：不要手动调用 ImGui::DestroyContext()
    // ImGuiImplWin32 的析构函数会自动处理 ImGui 的清理
    g_ImGuiContext = nullptr;

    g_Renderer = nullptr;
}

void BeginFrame()
{
    if (!g_ImGuiWin32) return;

    ImGuiIO& io = ImGui::GetIO();
    g_ImGuiWin32->NewFrame(
        static_cast<Diligent::Uint32>(io.DisplaySize.x),
        static_cast<Diligent::Uint32>(io.DisplaySize.y),
        Diligent::SURFACE_TRANSFORM_OPTIMAL);

    ImGuizmo::BeginFrame();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
}

void RenderUI(Moon::PerspectiveCamera* camera, Moon::FPSCounter* fpsCounter)
{
    // ============================================================================
    // FPS 显示窗口（右上角）
    // ============================================================================
    if (fpsCounter) {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration 
                                       | ImGuiWindowFlags_AlwaysAutoResize 
                                       | ImGuiWindowFlags_NoSavedSettings 
                                       | ImGuiWindowFlags_NoFocusOnAppearing 
                                       | ImGuiWindowFlags_NoNav;

        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos;
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = work_pos.x + work_size.x - PAD;
        window_pos.y = work_pos.y + PAD;
        window_pos_pivot.x = 1.0f;
        window_pos_pivot.y = 0.0f;
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        window_flags |= ImGuiWindowFlags_NoMove;

        ImGui::SetNextWindowBgAlpha(0.35f);
        if (ImGui::Begin("FPS Overlay", nullptr, window_flags)) {
            ImGui::Text("FPS: %.1f", fpsCounter->GetFPS());
            ImGui::Text("Frame Time: %.2f ms", fpsCounter->GetFrameTimeMs());
            if (camera) {
                auto pos = camera->GetPosition();
                ImGui::Separator();
                ImGui::Text("Camera Pos:");
                ImGui::Text("  X: %.2f", pos.x);
                ImGui::Text("  Y: %.2f", pos.y);
                ImGui::Text("  Z: %.2f", pos.z);
            }
            ImGui::Separator();
            ImGui::Checkbox("Debug Grid", &g_DebugGridEnabled);
        }
        ImGui::End();
    }

    // 在右上角显示 ViewManipulate（坐标轴小立方体）
    if (camera) {
        ImGuiIO& io = ImGui::GetIO();
        float viewManipulateSize = 128.0f;  // 小立方体的大小
        float padding = 10.0f;
        
        // 位置：右上角，在 FPS 窗口下方
        ImVec2 position(io.DisplaySize.x - viewManipulateSize - padding, 
                       100.0f);  // 距离顶部100像素，给FPS窗口留空间
        
        auto viewMatrix = camera->GetViewMatrix();
        float distance = 8.0f;  // 视图距离
        
        // 背景颜色：半透明深灰色
        ImU32 bgColor = 0x10101010;
        
        ImGuizmo::ViewManipulate(&viewMatrix.m[0][0], distance, position, 
                                ImVec2(viewManipulateSize, viewManipulateSize), bgColor);
    }

    // 渲染所有 ImGui 内容
    ImGui::Render();
    if (g_ImGuiWin32 && g_Renderer) {
        g_ImGuiWin32->Render(g_Renderer->GetContext());
    }
}

void RenderDebugGrid(Moon::PerspectiveCamera* camera)
{
    if (!camera || !g_DebugGridEnabled) return;
    g_DebugGridRenderer.Render(camera);
}

void EnableDebugGrid(bool enable)
{
    g_DebugGridEnabled = enable;
}

bool IsDebugGridEnabled()
{
    return g_DebugGridEnabled;
}

bool HandleWin32Message(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_ImGuiWin32) {
        return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    }
    return false;
}

} // namespace HelloEngineImGui
