// ============================================================================
// EditorApp.cpp - Moon Engine 编辑器主程序（Refined Version）
// ============================================================================
// ✅ 集成 CEF 浏览器显示 React 编辑器界面
// ✅ 集成 EngineCore 渲染3D场景
// ✅ 集成 ImGui + ImGuizmo 实现 3D 操作手柄
// ✅ 此版本仅优化结构与可读性，不修改任何逻辑或行为
// ============================================================================

#include <Windows.h>
#include <iostream>
#include <chrono>

// CEF
#include "EditorBridge.h"
#include "cef/CefApp.h"

// 引擎核心
#include "../engine/core/EngineCore.h"
#include "../engine/core/Logging/Logger.h"
#include "../engine/core/Camera/FPSCameraController.h"
#include "../engine/core/Scene/Scene.h"
#include "../engine/core/Scene/SceneNode.h"
#include "../engine/core/Scene/MeshRenderer.h"

// 渲染系统
#include "../engine/render/DiligentRenderer.h"
#include "../engine/render/RenderCommon.h"

// ImGui & ImGuizmo
#include "imgui.h"
#include "ImGuiImplWin32.hpp"
#include "ImGuiImplDiligent.hpp"
#include "ImGuizmo.h"

// Diligent
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"

// ImGui Win32 消息处理函数
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================================
// 全局对象
// ============================================================================
static EngineCore* g_Engine = nullptr;
static DiligentRenderer* g_Renderer = nullptr;
static Moon::FPSCameraController* g_CameraController = nullptr;
static Diligent::ImGuiImplWin32* g_ImGuiWin32 = nullptr;
static HWND g_EngineWindow = nullptr;
static Moon::SceneNode* g_SelectedObject = nullptr;

// 引擎窗口类名
static const wchar_t* kEngineWindowClass = L"MoonEngine_Viewport";

// HTML Viewport 信息（JS 提供）
struct ViewportRect {
    int x = 0, y = 0;
    int width = 800, height = 600;
    bool updated = false;
};
static ViewportRect g_ViewportRect;

// ============================================================================
// 辅助函数：查找 CEF 的渲染窗口
// ============================================================================
HWND FindCefHtmlRenderWindow(HWND cefWindow)
{
    HWND htmlWindow = FindWindowExW(cefWindow, nullptr, L"Chrome_RenderWidgetHostHWND", nullptr);
    if (htmlWindow) return htmlWindow;

    HWND chromeWidget = FindWindowExW(cefWindow, nullptr, L"Chrome_WidgetWin_0", nullptr);
    if (chromeWidget) {
        htmlWindow = FindWindowExW(chromeWidget, nullptr, L"Chrome_RenderWidgetHostHWND", nullptr);
        if (htmlWindow) return htmlWindow;
    }

    MOON_LOG_INFO("EditorApp", "Searching for HTML render window via enumeration...");
    EnumChildWindows(cefWindow, [](HWND hwnd, LPARAM lParam) -> BOOL {
        wchar_t cls[256];
        GetClassNameW(hwnd, cls, 256);
        if (wcscmp(cls, L"Chrome_RenderWidgetHostHWND") == 0) {
            *reinterpret_cast<HWND*>(lParam) = hwnd;
            return FALSE;
        }
        return TRUE;
        }, reinterpret_cast<LPARAM>(&htmlWindow));

    return htmlWindow;
}

// ============================================================================
// 引擎窗口过程（Forward to ImGui → then default）
// ============================================================================
LRESULT CALLBACK EngineWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_ImGuiWin32) {
        if (g_ImGuiWin32->Win32_ProcHandler(hWnd, msg, wParam, lParam))
            return true;
    }

    switch (msg) {
    case WM_SIZE:
        if (g_Renderer && wParam != SIZE_MINIMIZED) {
            UINT w = LOWORD(lParam), h = HIWORD(lParam);
            g_Renderer->Resize(w, h);
            if (g_Engine && h > 0)
                g_Engine->GetCamera()->SetAspectRatio(float(w) / float(h));
        }
        break;
    case WM_PAINT:
        ValidateRect(hWnd, nullptr);
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ============================================================================
// 初始化：引擎核心
// ============================================================================
void InitEngine(EngineCore*& enginePtr)
{
    std::cout << "[Editor] Initializing EngineCore..." << std::endl;
    static EngineCore engine;
    engine.Initialize();
    enginePtr = &engine;
}

// ============================================================================
// 初始化：CEF + 编辑器窗口
// ============================================================================
HWND InitCEF(HINSTANCE hInstance, EditorBridge& bridge)
{
    std::cout << "[Editor] Initializing CEF UI..." << std::endl;

    if (!bridge.Initialize(hInstance)) {
        MessageBoxA(nullptr, "Failed to initialize CEF!", "Error", MB_ICONERROR);
        return nullptr;
    }
    if (!bridge.CreateEditorWindow("")) {
        MessageBoxA(nullptr, "Failed to create editor window!", "Error", MB_ICONERROR);
        return nullptr;
    }

    HWND mainWindow = bridge.GetMainWindow();
    if (!mainWindow) return nullptr;

    // 等待 CEF Browser 真实窗口创建
    MSG msg{};
    HWND cefBrowserWindow = nullptr;
    for (int i = 0; i < 100; i++) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        bridge.DoMessageLoopWork();

        if (bridge.GetClient() && bridge.GetClient()->GetBrowser()) {
            cefBrowserWindow =
                bridge.GetClient()->GetBrowser()->GetHost()->GetWindowHandle();
            if (cefBrowserWindow) break;
        }
        Sleep(10);
    }

    if (!cefBrowserWindow) return nullptr;

    return cefBrowserWindow;
}

// ============================================================================
// 初始化：渲染器
// ============================================================================
bool InitRenderer(HWND parentWindow, HINSTANCE hInstance)
{
    // 注册窗口类
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = EngineWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kEngineWindowClass;

    if (!RegisterClassExW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;
    }

    // 创建引擎渲染窗口（子窗口）
    g_EngineWindow = CreateWindowExW(
        0, kEngineWindowClass, L"Engine Viewport",
        WS_CHILD, 0, 0, 100, 100,
        parentWindow, nullptr, hInstance, nullptr
    );
    if (!g_EngineWindow) return false;

    // 初始化 Renderer
    static DiligentRenderer renderer;
    g_Renderer = &renderer;

    RenderInitParams params{};
    params.windowHandle = g_EngineWindow;
    params.width = 800;
    params.height = 600;

    return renderer.Initialize(params);
}

// ============================================================================
// 初始化：ImGui
// ============================================================================
void InitImGui()
{
    std::cout << "[Editor] Initializing ImGui..." << std::endl;

    Diligent::ImGuiDiligentCreateInfo ci;
    ci.pDevice = g_Renderer->GetDevice();
    ci.BackBufferFmt = g_Renderer->GetSwapChain()->GetDesc().ColorBufferFormat;
    ci.DepthBufferFmt = g_Renderer->GetSwapChain()->GetDesc().DepthBufferFormat;

    g_ImGuiWin32 = new Diligent::ImGuiImplWin32(ci, g_EngineWindow);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
}

// ============================================================================
// 初始化：相机、控制器、示例场景
// ============================================================================
void InitSceneObjects(EngineCore* engine)
{
    auto* camera = engine->GetCamera();
    camera->SetAspectRatio(800.0f / 600.0f);

    Moon::InputSystem* input = engine->GetInputSystem();
    input->SetWindowHandle(g_EngineWindow);

    static Moon::FPSCameraController controller(camera, input);
    controller.SetMoveSpeed(10.0f);
    controller.SetMouseSensitivity(30.0f);
    g_CameraController = &controller;

    Moon::Scene* scene = engine->GetScene();
    Moon::MeshManager* meshMgr = engine->GetMeshManager();

    // =========================================================================
    // ✅ 1. Ground Plane
    // =========================================================================
    {
        Moon::SceneNode* ground = scene->CreateNode("Ground");
        ground->GetTransform()->SetLocalPosition({ 0.0f, -0.6f, 0.0f });

        Moon::MeshRenderer* renderer = ground->AddComponent<Moon::MeshRenderer>();

        renderer->SetMesh(
            meshMgr->CreatePlane(
                50.0f,           // width
                50.0f,           // depth
                1,               // subdivisionsX
                1,               // subdivisionsZ
                Moon::Vector3(0.4f, 0.4f, 0.4f) // 灰色
            )
        );
    }

    // =========================================================================
    // ✅ 2. Cube
    // =========================================================================
    Moon::SceneNode* cube = scene->CreateNode("Cube");
    cube->GetTransform()->SetLocalPosition({ -3.0f, 0.0f, 0.0f });
    auto* cubeRenderer = cube->AddComponent<Moon::MeshRenderer>();
    cubeRenderer->SetMesh(
        meshMgr->CreateCube(1.0f, { 1.0f, 0.3f, 0.3f })
    );

    // =========================================================================
    // ✅ 3. Sphere
    // =========================================================================
    Moon::SceneNode* sphere = scene->CreateNode("Sphere");
    sphere->GetTransform()->SetLocalPosition({ 0.0f, 0.0f, 0.0f });
    auto* sphereRenderer = sphere->AddComponent<Moon::MeshRenderer>();
    sphereRenderer->SetMesh(
        meshMgr->CreateSphere(0.6f, 32, 16, { 0.3f, 1.0f, 0.3f })
    );

    // =========================================================================
    // ✅ 4. Cylinder
    // =========================================================================
    Moon::SceneNode* cylinder = scene->CreateNode("Cylinder");
    cylinder->GetTransform()->SetLocalPosition({ 3.0f, 0.0f, 0.0f });
    auto* cylinderRenderer = cylinder->AddComponent<Moon::MeshRenderer>();
    cylinderRenderer->SetMesh(
        meshMgr->CreateCylinder(0.5f, 0.5f, 1.5f, 24, { 0.3f, 0.5f, 1.0f })
    );
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

        // 旋转 cube
        {
            auto* cube = engine->GetScene()->FindNodeByName("Cube");
            if (cube) {
                auto r = cube->GetTransform()->GetLocalRotation();
                cube->GetTransform()->SetLocalRotation({ r.x, r.y + 45.0f * dt, r.z });
            }
        }

        // 渲染
        auto vp = engine->GetCamera()->GetViewProjectionMatrix();
        g_Renderer->SetViewProjectionMatrix(&vp.m[0][0]);

        g_Renderer->BeginFrame();

        engine->GetScene()->Traverse([&](Moon::SceneNode* node) {
            auto* mr = node->GetComponent<Moon::MeshRenderer>();
            if (mr && mr->IsEnabled() && mr->IsVisible())
                mr->Render(g_Renderer);
            });

        // ImGui + ImGuizmo
        if (g_ImGuiWin32) {
            g_ImGuiWin32->NewFrame(g_ViewportRect.width, g_ViewportRect.height,
                Diligent::SURFACE_TRANSFORM_OPTIMAL);

            ImGuizmo::BeginFrame();
            ImGuizmo::SetRect(0, 0, (float)g_ViewportRect.width, (float)g_ViewportRect.height);

            if (!g_SelectedObject)
                g_SelectedObject = engine->GetScene()->FindNodeByName("Cylinder");

            if (g_SelectedObject) {
                auto view = engine->GetCamera()->GetViewMatrix();
                auto proj = engine->GetCamera()->GetProjectionMatrix();
                auto mat = g_SelectedObject->GetTransform()->GetWorldMatrix();

                ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0],
                    ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
                    &mat.m[0][0]);

                if (ImGuizmo::IsUsing()) {
                    g_SelectedObject->GetTransform()->SetLocalPosition({
                        mat.m[3][0], mat.m[3][1], mat.m[3][2]
                        });
                }
            }

            g_ImGuiWin32->Render(g_Renderer->GetContext());
        }

        g_Renderer->EndFrame();
        Sleep(1);
    }
}

// ============================================================================
// 清理
// ============================================================================
void Shutdown(EditorBridge& bridge, EngineCore* engine)
{
    if (g_ImGuiWin32) {
        delete g_ImGuiWin32;
        g_ImGuiWin32 = nullptr;
    }

    g_Renderer->Shutdown();

    if (g_EngineWindow) {
        DestroyWindow(g_EngineWindow);
        g_EngineWindow = nullptr;
    }

    engine->Shutdown();
    bridge.Shutdown();

    Moon::Core::Logger::Shutdown();
}

// ============================================================================
// WinMain (入口点，保持原始逻辑)
// ============================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // ========== 处理 CEF 子进程 ==========
    CefMainArgs args(hInstance);
    CefRefPtr<CefAppHandler> app(new CefAppHandler());
    int exit_code = CefExecuteProcess(args, app.get(), nullptr);
    if (exit_code >= 0) return exit_code;

    // Console
    AllocConsole();
    FILE* fp; freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    // Logger
    Moon::Core::Logger::Init();

    // ========== 引擎 ==========
    InitEngine(g_Engine);

    // ========== CEF ==========
    EditorBridge bridge;
    HWND cefBrowserWindow = InitCEF(hInstance, bridge);
    if (!cefBrowserWindow) return -1;

    // 绑定 viewport 回调
    bridge.GetClient()->SetViewportRectCallback([](int x, int y, int w, int h) {
        g_ViewportRect = { x, y, w, h, true };
        });

    // 找 HTML 渲染窗口
    HWND htmlWindow = FindCefHtmlRenderWindow(cefBrowserWindow);
    HWND parentWindow = htmlWindow ? htmlWindow : cefBrowserWindow;

    // ========== 渲染器 ==========
    if (!InitRenderer(parentWindow, hInstance)) {
        MessageBoxA(nullptr, "Renderer init failed!", "Error", MB_ICONERROR);
        return -1;
    }

    // ========== ImGui ==========
    InitImGui();

    // ========== 场景 ==========
    InitSceneObjects(g_Engine);

    // ========== 主循环 ==========
    RunMainLoop(bridge, g_Engine);

    // ========== 清理 ==========
    Shutdown(bridge, g_Engine);

    FreeConsole();
    return 0;
}

