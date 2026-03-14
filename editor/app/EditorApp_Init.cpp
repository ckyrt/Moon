// ============================================================================
// EditorApp_Init.cpp - 初始化函数
// ============================================================================
#include "EditorApp.h"
#include "UITextureManager.h"
#include "UIRenderer.h"
#include "../engine/core/EngineCore.h"
#include "../engine/core/Camera/Camera.h"
#include "../engine/core/Camera/FPSCameraController.h"
#include "../engine/core/Input/InputSystem.h"
#include "../engine/core/Scene/Scene.h"
#include "../engine/core/Scene/SceneNode.h"
#include "../engine/core/Scene/MeshRenderer.h"
#include "../engine/core/Scene/Light.h"
#include "../engine/core/Scene/Skybox.h"
#include "../../engine/core/Assets/MeshManager.h"
#include "../engine/core/Logging/Logger.h"
#include "../engine/render/diligent/DiligentRenderer.h"
#include "../engine/render/RenderCommon.h"
#include "../engine/physics/PhysicsSystem.h"
#include "../engine/physics/RigidBody.h"
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

    // 设置 CEF 渲染回调（OSR 模式）
    bridge.GetClient()->GetRenderHandlerImpl()->SetOnPaintCallback(
        [](const void* buffer, int width, int height) {
            if (g_UITextureManager) {
                g_UITextureManager->UpdateTextureData(buffer, width, height);
            }
        }
    );

    // 获取CEF主窗口的实际尺寸
    RECT rc;
    GetClientRect(mainWindow, &rc);
    int cefWidth = rc.right - rc.left;
    int cefHeight = rc.bottom - rc.top;
    
    // 设置CEF OSR的viewport大小为主窗口尺寸
    bridge.GetClient()->SetViewportSize(cefWidth, cefHeight);

    // 设置CEF主窗口尺寸变化回调
    SetCefWindowResizeCallback([](int width, int height) {
        MOON_LOG_INFO("CEF_Resize", "CEF window resized to %dx%d", width, height);
        
        if (g_UITextureManager) {
            g_UITextureManager->Resize(width, height);
        }
        
        if (g_Renderer) {
            g_Renderer->Resize(width, height);
        }
    });
    
    // 设置ImGui的WantCaptureMouse回调（优先给ImGui/ImGuizmo处理鼠标事件）
    SetImGuiWantsCaptureCallback([]() -> bool {
        ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureMouse;
    });
    
    // 设置ImGui的Win32事件处理回调（在MainWindowProc中优先调用）
    SetImGuiWin32ProcCallback([](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        if (g_ImGuiWin32) {
            return g_ImGuiWin32->Win32_ProcHandler(hwnd, msg, wParam, lParam) ? 1 : 0;
        }
        return 0;
    });
    
    // 设置3D场景交互回调（仅用于viewport区域的picking）
    extern void Handle3DScenePicking(UINT msg, WPARAM wParam, LPARAM lParam);
    SetEngineWndProcCallback(Handle3DScenePicking);

    MOON_LOG_INFO("EditorApp", "CEF OSR mode configured (size: %dx%d)", cefWidth, cefHeight);

    return cefBrowserWindow;
}

// ============================================================================
// 初始化引擎窗口 - OSR模式下已废弃（只有一个MainWindowProc）
// ============================================================================
// 注意：在OSR模式下，不再需要独立的引擎窗口和EngineWndProc
// 所有事件处理都在MainWindowProc中完成

// ============================================================================
// 初始化渲染器
// ============================================================================
bool InitRenderer()
{
    // 使用 CEF 主窗口作为渲染目标
    extern HWND g_CefWindow;
    if (!g_CefWindow) return false;

    static DiligentRenderer renderer;
    g_Renderer = &renderer;

    RenderInitParams params{};
    params.windowHandle = g_CefWindow;
    
    // 获取CEF主窗口的实际尺寸
    RECT rc;
    GetClientRect(g_CefWindow, &rc);
    params.width = rc.right - rc.left;
    params.height = rc.bottom - rc.top;

    MOON_LOG_INFO("EditorApp", "Initializing renderer with window size: %dx%d", params.width, params.height);

    if (!renderer.Initialize(params)) {
        return false;
    }

    // 创建 UI 渲染系统（使用CEF主窗口尺寸）
    int uiWidth = 1280, uiHeight = 720;
    if (g_EditorBridge && g_EditorBridge->GetMainWindow()) {
        RECT uiRect;
        GetClientRect(g_EditorBridge->GetMainWindow(), &uiRect);
        uiWidth = uiRect.right - uiRect.left;
        uiHeight = uiRect.bottom - uiRect.top;
    }
    
    g_UITextureManager = new UITextureManager(
        g_Renderer->GetDevice(),
        g_Renderer->GetContext()
    );
    g_UITextureManager->Initialize(uiWidth, uiHeight);

    g_UIRenderer = new UIRenderer(
        g_Renderer->GetDevice(),
        g_Renderer->GetContext()
    );
    if (!g_UIRenderer->Initialize()) {
        MOON_LOG_ERROR("EditorApp", "Failed to initialize UI renderer");
        return false;
    }

    MOON_LOG_INFO("EditorApp", "UI rendering system initialized (texture size: %dx%d)", uiWidth, uiHeight);
    
    return true;
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

    // 绑定到CEF窗口，而不是g_EngineWindow
    extern HWND g_CefWindow;
    g_ImGuiWin32 = new Diligent::ImGuiImplWin32(ci, g_CefWindow);

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

    extern HWND g_CefWindow;
    Moon::InputSystem* input = engine->GetInputSystem();
    input->SetWindowHandle(g_CefWindow);

    static Moon::FPSCameraController controller(camera, input);
    controller.SetMoveSpeed(10.0f);
    controller.SetMouseSensitivity(30.0f);
    g_CameraController = &controller;

    // 初始化物理系统
    static Moon::PhysicsSystem physicsSystem;
    physicsSystem.Init();
    g_PhysicsSystem = &physicsSystem;

    Moon::Scene* scene = engine->GetScene();
    //Moon::MeshManager* meshMgr = engine->GetMeshManager();

    if (scene && !scene->FindNodeByName("Editor Sun")) {
        Moon::SceneNode* sunNode = scene->CreateNode("Editor Sun");
        Moon::Light* sunLight = sunNode->AddComponent<Moon::Light>();
        sunLight->SetType(Moon::Light::Type::Directional);
        sunLight->SetIntensity(3.0f);
        sunLight->SetColor(Moon::Vector3(1.0f, 0.98f, 0.92f));
        sunLight->SetCastShadows(true);
        sunNode->GetTransform()->SetLocalPosition({ 12.0f, 18.0f, -12.0f });
        sunNode->GetTransform()->LookAt(Moon::Vector3(0.0f, 0.0f, 0.0f));
    }

    if (scene && !scene->FindNodeByName("Editor Sky")) {
        Moon::SceneNode* skyNode = scene->CreateNode("Editor Sky");
        Moon::Skybox* skybox = skyNode->AddComponent<Moon::Skybox>();
        skybox->LoadEnvironmentMap("textures/environment.hdr");
        skybox->SetIntensity(0.75f);
        skybox->SetEnableIBL(true);
    }

    // 创建 Ground Plane
    //{
    //    Moon::SceneNode* ground = scene->CreateNode("Ground");
    //    ground->GetTransform()->SetLocalPosition({ 0.0f, -0.6f, 0.0f });

    //    Moon::MeshRenderer* renderer = ground->AddComponent<Moon::MeshRenderer>();

    //    renderer->SetMesh(
    //        meshMgr->CreatePlane(
    //            50.0f,           // width
    //            50.0f,           // depth
    //            1,               // subdivisionsX
    //            1,               // subdivisionsZ
    //            Moon::Vector3(0.4f, 0.4f, 0.4f) // 灰色
    //        )
    //    );

    //    Moon::RigidBody* groundBody = ground->AddComponent<Moon::RigidBody>();
    //    groundBody->CreateBody(
    //        g_PhysicsSystem,
    //        Moon::PhysicsShapeType::Box,
    //        Moon::Vector3(25.0f, 0.1f, 25.0f),
    //        0.0f
    //    );
    //}

    //{
    //    Moon::SceneNode* box = scene->CreateNode("PhysicsBox");
    //    box->GetTransform()->SetLocalPosition({ -3.0f, 5.0f, 0.0f });

    //    Moon::MeshRenderer* renderer = box->AddComponent<Moon::MeshRenderer>();
    //    renderer->SetMesh(meshMgr->CreateCube(1.0f, Moon::Vector3(1.0f, 0.2f, 0.2f)));

    //    Moon::RigidBody* body = box->AddComponent<Moon::RigidBody>();
    //    body->CreateBody(
    //        g_PhysicsSystem,
    //        Moon::PhysicsShapeType::Box,
    //        Moon::Vector3(0.5f, 0.5f, 0.5f),
    //        2.0f
    //    );
    //}

    //{
    //    Moon::SceneNode* sphere = scene->CreateNode("PhysicsSphere");
    //    sphere->GetTransform()->SetLocalPosition({ 0.0f, 8.0f, 0.0f });

    //    Moon::MeshRenderer* renderer = sphere->AddComponent<Moon::MeshRenderer>();
    //    renderer->SetMesh(meshMgr->CreateSphere(0.5f, 24, 16, Moon::Vector3(0.2f, 1.0f, 0.2f)));

    //    Moon::RigidBody* body = sphere->AddComponent<Moon::RigidBody>();
    //    body->CreateBody(
    //        g_PhysicsSystem,
    //        Moon::PhysicsShapeType::Sphere,
    //        Moon::Vector3(0.5f, 0.5f, 0.5f),
    //        1.5f
    //    );
    //}

    //{
    //    Moon::SceneNode* capsule = scene->CreateNode("PhysicsCapsule");
    //    capsule->GetTransform()->SetLocalPosition({ 3.0f, 6.0f, 0.0f });

    //    Moon::MeshRenderer* renderer = capsule->AddComponent<Moon::MeshRenderer>();
    //    renderer->SetMesh(meshMgr->CreateCylinder(0.3f, 0.3f, 1.0f, 16, Moon::Vector3(0.2f, 0.2f, 1.0f)));

    //    Moon::RigidBody* body = capsule->AddComponent<Moon::RigidBody>();
    //    body->CreateBody(
    //        g_PhysicsSystem,
    //        Moon::PhysicsShapeType::Capsule,
    //        Moon::Vector3(0.3f, 0.5f, 0.0f),
    //        1.0f
    //    );
    //}

    //{
    //    Moon::SceneNode* cylinder = scene->CreateNode("PhysicsCylinder");
    //    cylinder->GetTransform()->SetLocalPosition({ -1.5f, 10.0f, 2.0f });

    //    Moon::MeshRenderer* renderer = cylinder->AddComponent<Moon::MeshRenderer>();
    //    renderer->SetMesh(meshMgr->CreateCylinder(0.4f, 0.4f, 1.2f, 20, Moon::Vector3(1.0f, 1.0f, 0.2f)));

    //    Moon::RigidBody* body = cylinder->AddComponent<Moon::RigidBody>();
    //    body->CreateBody(
    //        g_PhysicsSystem,
    //        Moon::PhysicsShapeType::Cylinder,
    //        Moon::Vector3(0.4f, 0.6f, 0.0f),
    //        2.5f
    //    );
    //}

    //{
    //    Moon::SceneNode* smallBox = scene->CreateNode("SmallBox");
    //    smallBox->GetTransform()->SetLocalPosition({ 1.5f, 12.0f, -2.0f });

    //    Moon::MeshRenderer* renderer = smallBox->AddComponent<Moon::MeshRenderer>();
    //    renderer->SetMesh(meshMgr->CreateCube(0.6f, Moon::Vector3(1.0f, 0.6f, 0.2f)));

    //    Moon::RigidBody* body = smallBox->AddComponent<Moon::RigidBody>();
    //    body->CreateBody(
    //        g_PhysicsSystem,
    //        Moon::PhysicsShapeType::Box,
    //        Moon::Vector3(0.3f, 0.3f, 0.3f),
    //        0.8f
    //    );
    //}

    MOON_LOG_INFO("EditorApp", "Physics test objects created!");
}
