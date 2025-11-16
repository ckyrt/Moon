// ============================================================================
// EditorApp.cpp - Moon Engine 编辑器主程序（重构版本）
// ============================================================================
// ✅ 集成 CEF 浏览器显示 React 编辑器界面
// ✅ 集成 EngineCore 渲染3D场景
// ✅ 集成 ImGui + ImGuizmo 实现 3D 操作手柄
// ============================================================================

#include "EditorApp.h"

#include <Windows.h>
#undef GetNextSibling
#undef GetFirstChild

#include <iostream>
#include <chrono>

// CEF
#include "EditorBridge.h"
#include "cef/CefApp.h"

// 引擎核心
#include "../engine/core/EngineCore.h"
#include "../engine/core/Logging/Logger.h"
#include "../engine/core/Camera/FPSCameraController.h"

// 渲染系统
#include "../engine/render/DiligentRenderer.h"

// ImGui & ImGuizmo
#include "imgui.h"
#include "ImGuiImplWin32.hpp"
#include "ImGuizmo.h"

// Diligent
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"

// ============================================================================
// 全局对象定义
// ============================================================================
EngineCore* g_Engine = nullptr;
DiligentRenderer* g_Renderer = nullptr;
Moon::FPSCameraController* g_CameraController = nullptr;
Diligent::ImGuiImplWin32* g_ImGuiWin32 = nullptr;
HWND g_EngineWindow = nullptr;
Moon::SceneNode* g_SelectedObject = nullptr;
EditorBridge* g_EditorBridge = nullptr;

// Gizmo 状态
ImGuizmo::OPERATION g_GizmoOperation = ImGuizmo::TRANSLATE;
ImGuizmo::MODE g_GizmoMode = ImGuizmo::LOCAL;
bool g_WasUsingGizmo = false;
Moon::Quaternion g_LastRotation = Moon::Quaternion(0, 0, 0, 1);
Moon::Matrix4x4 g_GizmoMatrix;

// Viewport 信息
ViewportRect g_ViewportRect;

// ============================================================================
// 公共接口实现
// ============================================================================
void SetSelectedObject(Moon::SceneNode* node)
{
    g_SelectedObject = node;
}

Moon::SceneNode* GetSelectedObject()
{
    return g_SelectedObject;
}

void SetGizmoOperation(const std::string& mode)
{
    if (mode == "translate") {
        g_GizmoOperation = ImGuizmo::TRANSLATE;
    }
    else if (mode == "rotate") {
        g_GizmoOperation = ImGuizmo::ROTATE;
    }
    else if (mode == "scale") {
        g_GizmoOperation = ImGuizmo::SCALE;
    }
}

void SetGizmoMode(const std::string& mode)
{
    if (mode == "world") {
        g_GizmoMode = ImGuizmo::WORLD;
        MOON_LOG_INFO("EditorApp", "Gizmo mode set to WORLD");
    }
    else if (mode == "local") {
        g_GizmoMode = ImGuizmo::LOCAL;
        MOON_LOG_INFO("EditorApp", "Gizmo mode set to LOCAL");
    }
}

// ============================================================================
// 主循环
// ============================================================================
void RunMainLoop(EditorBridge& bridge, EngineCore* engine)
{
    MSG msg{};
    bool running = true;
    auto prevTime = std::chrono::high_resolution_clock::now();

    while (running) {

        // Windows 消息
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running) break;

        // UI 关闭退出
        if (bridge.IsClosing()) break;

        // CEF
        bridge.DoMessageLoopWork();
        if (bridge.IsClosing()) break;

        // 更新 Viewport
        if (g_ViewportRect.updated) {
            SetWindowPos(
                g_EngineWindow, nullptr,
                g_ViewportRect.x, g_ViewportRect.y,
                g_ViewportRect.width, g_ViewportRect.height,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW
            );

            if (g_ViewportRect.width > 0 && g_ViewportRect.height > 0) {
                g_Renderer->Resize(g_ViewportRect.width, g_ViewportRect.height);
                engine->GetCamera()->SetAspectRatio(
                    float(g_ViewportRect.width) / float(g_ViewportRect.height)
                );
            }
            g_ViewportRect.updated = false;
        }

        // deltaTime
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - prevTime).count();
        prevTime = now;

        // 更新引擎
        engine->Tick(dt);
        g_CameraController->Update(dt);

        // 渲染开始
        g_Renderer->BeginFrame();

        // 渲染场景
        RenderScene(engine, g_Renderer);

        // ImGui + ImGuizmo
        if (g_ImGuiWin32)
        {
            g_ImGuiWin32->NewFrame(g_ViewportRect.width, g_ViewportRect.height,
                Diligent::SURFACE_TRANSFORM_OPTIMAL);

            ImGuizmo::BeginFrame();
            ImGuizmo::SetRect(0, 0, (float)g_ViewportRect.width, (float)g_ViewportRect.height);

            // 渲染并应用 Gizmo
            RenderAndApplyGizmo(engine, bridge);

            g_ImGuiWin32->Render(g_Renderer->GetContext());
        }

        g_Renderer->EndFrame();
        Sleep(1);
    }
}

// ============================================================================
// WinMain 入口点
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // 处理 CEF 子进程
    CefMainArgs args(hInstance);
    CefRefPtr<CefAppHandler> app(new CefAppHandler());
    int exit_code = CefExecuteProcess(args, app.get(), nullptr);
    if (exit_code >= 0) return exit_code;

    // Console
    AllocConsole();
    FILE* fp; 
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    // Logger
    Moon::Core::Logger::Init();

    // 初始化引擎
    InitEngine(g_Engine);

    // 初始化 CEF
    EditorBridge bridge;
    g_EditorBridge = &bridge;
    HWND cefBrowserWindow = InitCEF(hInstance, bridge);
    if (!cefBrowserWindow) return -1;

    // 绑定 viewport 回调
    bridge.GetClient()->SetViewportRectCallback([](int x, int y, int w, int h) {
        g_ViewportRect = { x, y, w, h, true };
    });

    // 设置引擎核心到 CEF Client
    bridge.GetClient()->SetEngineCore(g_Engine);

    // 找 HTML 渲染窗口
    HWND htmlWindow = FindCefHtmlRenderWindow(cefBrowserWindow);
    HWND parentWindow = htmlWindow ? htmlWindow : cefBrowserWindow;

    // 注册窗口类
    if (!InitEngineWindow(hInstance)) {
        MessageBoxA(nullptr, "Window class registration failed!", "Error", MB_ICONERROR);
        return -1;
    }

    // 创建引擎窗口
    g_EngineWindow = CreateWindowExW(
        0, L"MoonEngine_Viewport", L"Engine Viewport",
        WS_CHILD, 0, 0, 100, 100,
        parentWindow, nullptr, hInstance, nullptr
    );
    if (!g_EngineWindow) return -1;

    // 初始化渲染器
    if (!InitRenderer()) {
        MessageBoxA(nullptr, "Renderer init failed!", "Error", MB_ICONERROR);
        return -1;
    }

    // 初始化 ImGui
    InitImGui();

    // 初始化场景
    InitSceneObjects(g_Engine);

    // 主循环
    RunMainLoop(bridge, g_Engine);

    // 清理资源
    
    CleanupResources();

    FreeConsole();
    return 0;
}
