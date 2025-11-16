// ============================================================================
// EditorApp_Init.cpp - 初始化函数
// ============================================================================
#include "EditorApp.h"
#include "../engine/core/EngineCore.h"
#include "../engine/core/Camera/Camera.h"
#include "../engine/core/Camera/FPSCameraController.h"
#include "../engine/core/Input/InputSystem.h"
#include "../engine/core/Scene/Scene.h"
#include "../engine/core/Scene/SceneNode.h"
#include "../engine/core/Scene/MeshRenderer.h"
#include "../../engine/core/Assets/MeshManager.h"
#include "../engine/core/Logging/Logger.h"
#include "../engine/render/DiligentRenderer.h"
#include "../engine/render/RenderCommon.h"
#include "EditorBridge.h"
#include "cef/CefApp.h"
#include "ImGuiImplWin32.hpp"
#include "ImGuiImplDiligent.hpp"
#include "imgui.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include <iostream>
#include <Windows.h>

// 窗口类名
static const wchar_t* kEngineWindowClass = L"MoonEngine_Viewport";

// ============================================================================
// 初始化引擎核心
// ============================================================================
void InitEngine(EngineCore*& enginePtr)
{
    std::cout << "[Editor] Initializing EngineCore..." << std::endl;
    static EngineCore engine;
    engine.Initialize();
    enginePtr = &engine;
}

// ============================================================================
// 初始化 CEF 浏览器
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

    // 等待 CEF Browser 窗口创建
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
// 初始化引擎窗口
// ============================================================================
bool InitEngineWindow(HINSTANCE hInstance)
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

    return true;
}

// ============================================================================
// 初始化渲染器
// ============================================================================
bool InitRenderer()
{
    if (!g_EngineWindow) return false;

    static DiligentRenderer renderer;
    g_Renderer = &renderer;

    RenderInitParams params{};
    params.windowHandle = g_EngineWindow;
    params.width = 800;
    params.height = 600;

    return renderer.Initialize(params);
}

// ============================================================================
// 初始化 ImGui
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
// 初始化场景对象
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

    // 创建 Ground Plane
    {
        Moon::SceneNode* ground = scene->CreateNode("Ground");
        ground->GetTransform()->SetLocalPosition({ 0.0f, -0.6f, 0.0f });

        Moon::MeshRenderer* renderer = ground->AddComponent<Moon::MeshRenderer>();

        renderer->SetMesh(
            engine->GetMeshManager()->CreatePlane(
                50.0f,           // width
                50.0f,           // depth
                1,               // subdivisionsX
                1,               // subdivisionsZ
                Moon::Vector3(0.4f, 0.4f, 0.4f) // 灰色
            )
        );
    }
}
