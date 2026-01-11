// ============================================================================
// EditorApp.cpp - Moon Engine 编辑器主程序（重构版本）
// ============================================================================
// 集成 CEF 浏览器显示 React 编辑器界面
// 集成 EngineCore 渲染3D场景
// 集成 ImGui + ImGuizmo 实现 3D 操作手柄
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
#include "../engine/core/Texture/TextureManager.h"

// 渲染系统
#include "../engine/render/diligent/DiligentRenderer.h"

// 物理系统
#include "../engine/physics/PhysicsSystem.h"

// ImGui & ImGuizmo
#include "imgui.h"
#include "ImGuiImplWin32.hpp"
#include "ImGuizmo.h"

// Diligent
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"

// UI 渲染系统
#include "UITextureManager.h"
#include "UIRenderer.h"
// 常量定义
namespace {
    constexpr int LARGE_SCISSOR_RECT = 10000;  // 用于全屏scissor rect的足够大的值
}
// ============================================================================
// 全局对象定义
// ============================================================================
EngineCore* g_Engine = nullptr;
DiligentRenderer* g_Renderer = nullptr;
Moon::PhysicsSystem* g_PhysicsSystem = nullptr;
Moon::FPSCameraController* g_CameraController = nullptr;
Diligent::ImGuiImplWin32* g_ImGuiWin32 = nullptr;

HWND g_CefWindow = nullptr;  // CEF主窗口句柄
Moon::SceneNode* g_SelectedObject = nullptr;
EditorBridge* g_EditorBridge = nullptr;

// UI 渲染系统（OSR 模式）
UITextureManager* g_UITextureManager = nullptr;
UIRenderer* g_UIRenderer = nullptr;

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

        // 更新 Viewport（只更新相机宽高比）
        if (g_ViewportRect.updated) {
            if (g_ViewportRect.width > 0 && g_ViewportRect.height > 0) {
                // 更新相机宽高比
                engine->GetCamera()->SetAspectRatio(
                    float(g_ViewportRect.width) / float(g_ViewportRect.height)
                );
                
                // 通知CEF viewport区域位置（用于事件判断）
                SetViewportInfo(g_ViewportRect.x, g_ViewportRect.y, 
                               g_ViewportRect.width, g_ViewportRect.height);
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
        
        // 更新物理系统
        if (g_PhysicsSystem) {
            g_PhysicsSystem->Step(dt);
        }

        // 渲染开始
        g_Renderer->BeginFrame();

        // 1. 设置viewport，只在viewport区域渲染3D内容
        if (g_ViewportRect.width > 0 && g_ViewportRect.height > 0) {
            auto* ctx = g_Renderer->GetContext();
            
            // 设置viewport
            Diligent::Viewport vp;
            vp.TopLeftX = static_cast<float>(g_ViewportRect.x);
            vp.TopLeftY = static_cast<float>(g_ViewportRect.y);
            vp.Width = static_cast<float>(g_ViewportRect.width);
            vp.Height = static_cast<float>(g_ViewportRect.height);
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            ctx->SetViewports(1, &vp, 0, 0);
            
            // 设置scissor rect（裁剪3D内容到viewport区域）
            Diligent::Rect scissor;
            scissor.left = g_ViewportRect.x;
            scissor.top = g_ViewportRect.y;
            scissor.right = g_ViewportRect.x + g_ViewportRect.width;
            scissor.bottom = g_ViewportRect.y + g_ViewportRect.height;
            ctx->SetScissorRects(1, &scissor, 0, 0);
            
            // 渲染3D场景
            RenderScene(engine, g_Renderer);
            
            // 重置 scissor rect 为全屏，让后续渲染不被裁剪
            Diligent::Rect fullScreenScissor;
            fullScreenScissor.left = 0;
            fullScreenScissor.top = 0;
            fullScreenScissor.right = LARGE_SCISSOR_RECT;
            fullScreenScissor.bottom = LARGE_SCISSOR_RECT;
            ctx->SetScissorRects(1, &fullScreenScissor, 0, 0);
            
            // ImGui + ImGuizmo
            if (g_ImGuiWin32)
            {
                // 获取CEF窗口的真实大小
                RECT cefRect;
                GetClientRect(g_CefWindow, &cefRect);
                int cefWindowWidth = cefRect.right - cefRect.left;
                int cefWindowHeight = cefRect.bottom - cefRect.top;
                    
                g_ImGuiWin32->NewFrame(cefWindowWidth, cefWindowHeight,
                    Diligent::SURFACE_TRANSFORM_OPTIMAL);

                ImGuizmo::BeginFrame();
                // ImGuizmo::SetRect使用CEF窗口坐标系
                ImGuizmo::SetRect((float)g_ViewportRect.x, (float)g_ViewportRect.y, 
                                 (float)g_ViewportRect.width, (float)g_ViewportRect.height);

                // 渲染并应用 Gizmo
                RenderAndApplyGizmo(engine, bridge);

                // 在Render之前确保DisplaySize正确（整个CEF窗口大小）
                ImGuiIO& io = ImGui::GetIO();
                io.DisplaySize = ImVec2((float)cefWindowWidth, (float)cefWindowHeight);

                g_ImGuiWin32->Render(g_Renderer->GetContext());
            }
        }
        
        // 2. 最后渲染 UI 层（fullscreen，带alpha混合，覆盖在3D场景上）
        if (g_UIRenderer && g_UITextureManager) {
            auto* ctx = g_Renderer->GetContext();
            
            // 获取CEF窗口的真实大小（全屏渲染UI）
            RECT cefRect;
            GetClientRect(g_CefWindow, &cefRect);
            int cefWindowWidth = cefRect.right - cefRect.left;
            int cefWindowHeight = cefRect.bottom - cefRect.top;
            
            // 恢复viewport到全屏
            Diligent::Viewport fullVp;
            fullVp.TopLeftX = 0.0f;
            fullVp.TopLeftY = 0.0f;
            fullVp.Width = static_cast<float>(cefWindowWidth);
            fullVp.Height = static_cast<float>(cefWindowHeight);
            fullVp.MinDepth = 0.0f;
            fullVp.MaxDepth = 1.0f;
            ctx->SetViewports(1, &fullVp, 0, 0);
            
            // 恢复scissor rect到全屏
            Diligent::Rect fullScissor;
            fullScissor.left = 0;
            fullScissor.top = 0;
            fullScissor.right = cefWindowWidth;
            fullScissor.bottom = cefWindowHeight;
            ctx->SetScissorRects(1, &fullScissor, 0, 0);
            
            auto* pRTV = g_Renderer->GetSwapChain()->GetCurrentBackBufferRTV();
            g_UIRenderer->RenderUI(g_UITextureManager, pRTV);
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

    // 初始化 Console
    AllocConsole();
    FILE* fp = nullptr; 
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    // 初始化 Logger
    Moon::Core::Logger::Init();

    // 设置资源根路径为exe所在目录
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring exePathW(exePath);
    size_t lastSlash = exePathW.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        exePathW = exePathW.substr(0, lastSlash);
        // 转换为UTF-8 string
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, exePathW.c_str(), (int)exePathW.length(), NULL, 0, NULL, NULL);
        std::string exeDir(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, exePathW.c_str(), (int)exePathW.length(), &exeDir[0], size_needed, NULL, NULL);
        Moon::TextureManager::SetResourceBasePath(exeDir);
    }

    // 初始化引擎
    InitEngine(g_Engine);

    // 初始化 CEF
    EditorBridge bridge;
    g_EditorBridge = &bridge;
    HWND cefBrowserWindow = InitCEF(hInstance, bridge);
    if (!cefBrowserWindow) return -1;

    // 绑定 viewport 回调
    bridge.GetClient()->SetViewportRectCallback([](int x, int y, int w, int h) {
        g_ViewportRect.x = x;
        g_ViewportRect.y = y;
        g_ViewportRect.width = w;
        g_ViewportRect.height = h;
        g_ViewportRect.updated = true;
    });

    // 绑定 viewport 点击回调（用于 3D picking）
    bridge.GetClient()->SetViewportPickCallback([](int x, int y) {
        if (g_Renderer && g_Engine && g_ViewportRect.width > 0 && g_ViewportRect.height > 0) {
            // 避免与 ImGuizmo 冲突
            if (!ImGuizmo::IsOver()) {
                // JavaScript 传递的是相对于 viewport canvas 的本地坐标
                // 需要转换为相对于整个窗口的全局坐标（因为 picking RT 是全屏的）
                int globalX = x + g_ViewportRect.x;
                int globalY = y + g_ViewportRect.y;

                // 渲染拾取通道（传递 viewport 参数）
                g_Renderer->RenderSceneForPicking(g_Engine->GetScene(), 
                    g_ViewportRect.x, g_ViewportRect.y, 
                    g_ViewportRect.width, g_ViewportRect.height);

                // 读取像素下的 ObjectID（使用全局坐标）
                uint32_t objectID = g_Renderer->ReadObjectIDAt(globalX, globalY);

                if (objectID != 0) {
                    // 查找对应的 SceneNode
                    Moon::Scene* scene = g_Engine->GetScene();
                    g_SelectedObject = nullptr;
                    scene->Traverse([objectID](Moon::SceneNode* node) {
                        if (node->GetID() == objectID) {
                            g_SelectedObject = node;
                        }
                    });

                    if (g_SelectedObject) {
                        MOON_LOG_INFO("EditorApp", "Selected object: %s (ID=%u)",
                            g_SelectedObject->GetName().c_str(), objectID);

                        // 通知 WebUI
                        if (g_EditorBridge && g_EditorBridge->GetClient() && g_EditorBridge->GetClient()->GetBrowser()) {
                            char jsCode[256];
                            snprintf(jsCode, sizeof(jsCode),
                                "if (window.onNodeSelected) { window.onNodeSelected(%u); }",
                                objectID
                            );
                            auto frame = g_EditorBridge->GetClient()->GetBrowser()->GetMainFrame();
                            frame->ExecuteJavaScript(jsCode, frame->GetURL(), 0);
                        }
                    }
                }
                else {
                    g_SelectedObject = nullptr;
                }
            }
        }
    });

    // 设置引擎核心到 CEF Client
    bridge.GetClient()->SetEngineCore(g_Engine);

    // 找 HTML 渲染窗口
    HWND htmlWindow = FindCefHtmlRenderWindow(cefBrowserWindow);
    HWND parentWindow = htmlWindow ? htmlWindow : cefBrowserWindow;
    g_CefWindow = parentWindow;  // 保存CEF窗口句柄供ImGui使用

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
