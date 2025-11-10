// EditorApp.cpp - Moon Engine 编辑器主程序
// 集成 CEF 浏览器显示 React 编辑器界面
// 集成 EngineCore 渲染3D场景

#include "EditorBridge.h"
#include "cef/CefApp.h"
#include <Windows.h>
#include <iostream>
#include <chrono>

// 引擎核心
#include "../engine/core/EngineCore.h"
#include "../engine/core/Logging/Logger.h"
#include "../engine/core/Camera/FPSCameraController.h"
#include "../engine/core/Scene/Scene.h"
#include "../engine/core/Scene/SceneNode.h"
#include "../engine/core/Scene/MeshRenderer.h"
#include "../engine/render/DiligentRenderer.h"
#include "../engine/render/RenderCommon.h"

// 全局引擎对象
static EngineCore* g_Engine = nullptr;
static DiligentRenderer* g_Renderer = nullptr;
static Moon::FPSCameraController* g_CameraController = nullptr;
static HWND g_EngineWindow = nullptr;

// Viewport 矩形信息（从 JavaScript 接收）
struct ViewportRect {
    int x = 0;
    int y = 0;
    int width = 800;
    int height = 600;
    bool updated = false;
};
static ViewportRect g_ViewportRect;

// 引擎窗口类名
static const wchar_t* kEngineWindowClass = L"MoonEngine_Viewport";

// ========================================
// 辅助函数：查找 CEF HTML 渲染窗口
// ========================================
HWND FindCefHtmlRenderWindow(HWND cefWindow) {
    // CEF 窗口层级：
    // Chrome_WidgetWin_1 (outer window with title bar)
    //   └── Chrome_RenderWidgetHostHWND (actual HTML rendering) ← 目标窗口
    
    // 方法 1：直接查找
    HWND htmlWindow = FindWindowExW(cefWindow, nullptr, L"Chrome_RenderWidgetHostHWND", nullptr);
    if (htmlWindow) return htmlWindow;
    
    // 方法 2：通过中间层查找
    HWND chromeWidget = FindWindowExW(cefWindow, nullptr, L"Chrome_WidgetWin_0", nullptr);
    if (chromeWidget) {
        htmlWindow = FindWindowExW(chromeWidget, nullptr, L"Chrome_RenderWidgetHostHWND", nullptr);
        if (htmlWindow) return htmlWindow;
    }
    
    // 方法 3：遍历所有子窗口
    MOON_LOG_INFO("EditorApp", "Searching for HTML render window via enumeration...");
    EnumChildWindows(cefWindow, [](HWND hwnd, LPARAM lParam) -> BOOL {
        wchar_t className[256];
        GetClassNameW(hwnd, className, 256);
        if (wcscmp(className, L"Chrome_RenderWidgetHostHWND") == 0) {
            *reinterpret_cast<HWND*>(lParam) = hwnd;
            MOON_LOG_INFO("EditorApp", "Found HTML render window: %p", hwnd);
            return FALSE; // 停止枚举
        }
        return TRUE; // 继续枚举
    }, reinterpret_cast<LPARAM>(&htmlWindow));
    
    return htmlWindow;
}

// 引擎窗口过程
LRESULT CALLBACK EngineWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        if (g_Renderer && wParam != SIZE_MINIMIZED) {
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            g_Renderer->Resize(width, height);
            if (g_Engine && height > 0) {
                g_Engine->GetCamera()->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
            }
        }
        break;
    case WM_PAINT:
        ValidateRect(hWnd, nullptr);
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow)
{
    // ✅【必须 1】让 CEF 处理子进程，否则会无限创建 EditorApp.exe
    CefMainArgs main_args(hInstance);
    CefRefPtr<CefAppHandler> app(new CefAppHandler());
    int exit_code = CefExecuteProcess(main_args, app.get(), nullptr);
    if (exit_code >= 0) {
        return exit_code;   // ✅ 子进程执行完毕后直接退出，不进入主程序逻辑
    }

    // ✅【必须 2】主进程继续往下运行
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    std::cout << "========================================" << std::endl;
    std::cout << "  Moon Engine Editor - CEF Version" << std::endl;
    std::cout << "========================================" << std::endl;

    // ✅ 初始化日志系统（在控制台之后）
    try {
        Moon::Core::Logger::Init();
        std::cout << "[Editor] Logger initialized successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Editor] Logger initialization failed: " << e.what() << std::endl;
        std::cerr << "[Editor] Continuing without file logging..." << std::endl;
    }

    // ========================================
    // 1. 初始化引擎核心（独立于UI）
    // ========================================
    std::cout << "[Editor] Initializing EngineCore..." << std::endl;
    EngineCore engine;
    engine.Initialize();
    g_Engine = &engine;

    // ========================================
    // 2. 初始化编辑器 UI（CEF）
    // ========================================
    std::cout << "[Editor] Initializing CEF UI..." << std::endl;
    // 创建编辑器桥接
    EditorBridge bridge;

    // 初始化 CEF
    if (!bridge.Initialize(hInstance)) {
        std::cerr << "Failed to initialize EditorBridge!" << std::endl;
        MessageBoxA(nullptr, "Failed to initialize CEF!", "Error", MB_ICONERROR);
        return -1;
    }

    if (!bridge.CreateEditorWindow("")) {
        std::cerr << "Failed to create editor window!" << std::endl;
        return -1;
    }

    std::cout << "[Editor] CEF window created successfully" << std::endl;

    // ✅ 获取主窗口句柄（现在是 SetAsChild 模式）
    HWND mainWindow = bridge.GetMainWindow();
    if (!mainWindow) {
        std::cerr << "[Editor] Failed to get main window handle!" << std::endl;
        return -1;
    }

    // 等待CEF浏览器创建完成，获取浏览器窗口句柄
    MSG msg;
    int waitCount = 0;
    HWND cefBrowserWindow = nullptr;
    while (waitCount < 100) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        bridge.DoMessageLoopWork();
        
        // 尝试获取CEF浏览器窗口
        if (bridge.GetClient() && bridge.GetClient()->GetBrowser()) {
            cefBrowserWindow = bridge.GetClient()->GetBrowser()->GetHost()->GetWindowHandle();
            if (cefBrowserWindow) {
                std::cout << "[Editor] CEF browser window handle obtained: " << cefBrowserWindow << std::endl;
                break;
            }
        }
        
        Sleep(10);
        waitCount++;
    }

    if (!cefBrowserWindow) {
        std::cerr << "[Editor] Failed to get CEF browser window handle!" << std::endl;
        return -1;
    }

    // 查找真正的 HTML 渲染窗口
    HWND htmlRenderWindow = FindCefHtmlRenderWindow(cefBrowserWindow);
    HWND parentWindow = htmlRenderWindow ? htmlRenderWindow : cefBrowserWindow;
    
    if (htmlRenderWindow) {
        MOON_LOG_INFO("EditorApp", "Using HTML render window as parent: %p", htmlRenderWindow);
    } else {
        MOON_LOG_WARN("EditorApp", "HTML render window not found, using CEF browser window as fallback: %p", cefBrowserWindow);
    }

    // 设置 Viewport 矩形回调（从 JavaScript 接收坐标）
    bridge.GetClient()->SetViewportRectCallback([](int x, int y, int width, int height) {
        MOON_LOG_INFO("EditorApp", "Viewport rect: (%d, %d) %dx%d", x, y, width, height);
        g_ViewportRect.x = x;
        g_ViewportRect.y = y;
        g_ViewportRect.width = width;
        g_ViewportRect.height = height;
        g_ViewportRect.updated = true;
    });

    // ========================================
    // 3. 创建嵌入式3D渲染窗口
    // ========================================
    std::cout << "[Editor] Creating embedded 3D viewport..." << std::endl;

    // 注册引擎窗口类
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = EngineWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kEngineWindowClass;

    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            std::cerr << "[Editor] Failed to register window class!" << std::endl;
            return -1;
        }
    }

    // 创建引擎渲染窗口（作为 HTML 渲染窗口的子窗口）
    g_EngineWindow = CreateWindowExW(
        0,
        kEngineWindowClass,
        L"Engine Viewport",
        WS_CHILD,  // 子窗口，初始隐藏
        0, 0, 100, 100,  // 临时位置和大小
        parentWindow,  // 父窗口是 HTML 渲染窗口
        nullptr,
        hInstance,
        nullptr
    );

    if (!g_EngineWindow) {
        std::cerr << "[Editor] Failed to create engine viewport!" << std::endl;
        return -1;
    }

    MOON_LOG_INFO("EditorApp", "Engine viewport created (hidden): %p", g_EngineWindow);

    // 初始化渲染器
    DiligentRenderer renderer;
    g_Renderer = &renderer;

    RenderInitParams params{};
    params.windowHandle = g_EngineWindow;
    params.width = 800;
    params.height = 600;

    if (!renderer.Initialize(params)) {
        MessageBoxA(g_EngineWindow, "Failed to initialize renderer!", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    std::cout << "[Editor] Renderer initialized" << std::endl;

    // 设置相机
    Moon::PerspectiveCamera* camera = engine.GetCamera();
    camera->SetAspectRatio(800.0f / 600.0f);

    // 创建FPS相机控制器
    Moon::InputSystem* inputSystem = engine.GetInputSystem();
    inputSystem->SetWindowHandle(g_EngineWindow);
    Moon::FPSCameraController cameraController(camera, inputSystem);
    cameraController.SetMoveSpeed(10.0f);
    cameraController.SetMouseSensitivity(30.0f);
    g_CameraController = &cameraController;

    // 创建示例场景
    Moon::Scene* scene = engine.GetScene();
    Moon::MeshManager* meshManager = engine.GetMeshManager();

    std::cout << "[Editor] Creating sample scene..." << std::endl;

    // 创建几个几何体用于测试
    Moon::SceneNode* cube = scene->CreateNode("Cube");
    cube->GetTransform()->SetLocalPosition(Moon::Vector3(-3.0f, 0.0f, 0.0f));
    Moon::MeshRenderer* cubeRenderer = cube->AddComponent<Moon::MeshRenderer>();
    cubeRenderer->SetMesh(meshManager->CreateCube(1.0f, Moon::Vector3(1.0f, 0.3f, 0.3f)));

    Moon::SceneNode* sphere = scene->CreateNode("Sphere");
    sphere->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, 0.0f, 0.0f));
    Moon::MeshRenderer* sphereRenderer = sphere->AddComponent<Moon::MeshRenderer>();
    sphereRenderer->SetMesh(meshManager->CreateSphere(0.6f, 32, 16, Moon::Vector3(0.3f, 1.0f, 0.3f)));

    Moon::SceneNode* cylinder = scene->CreateNode("Cylinder");
    cylinder->GetTransform()->SetLocalPosition(Moon::Vector3(3.0f, 0.0f, 0.0f));
    Moon::MeshRenderer* cylinderRenderer = cylinder->AddComponent<Moon::MeshRenderer>();
    cylinderRenderer->SetMesh(meshManager->CreateCylinder(0.5f, 0.5f, 1.5f, 24, Moon::Vector3(0.3f, 0.5f, 1.0f)));

    std::cout << "[Editor] Scene created with 3 objects" << std::endl;
    std::cout << "[Editor] Entering main loop..." << std::endl;

    // ========================================
    // 4. 主循环：同时更新引擎和CEF
    // ========================================
    auto prevTime = std::chrono::high_resolution_clock::now();
    bool running = true;
    
    while (running) {
        // 处理Windows消息
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!running) break;

        // ✅ 关键：停止渲染，立即退出
        if (bridge.IsClosing()) {
            std::cout << "[Editor] UI closed, stopping render loop..." << std::endl;
            break;
        }

        // 处理CEF消息（非阻塞）
        bridge.DoMessageLoopWork();

        // 检查CEF是否关闭
        if (bridge.IsClosing()) {
            running = false;
            break;
        }

        // 更新 viewport 窗口位置和大小
        if (g_ViewportRect.updated) {
            // JavaScript 坐标已经是相对于 HTML 渲染窗口的，直接使用
            BOOL success = SetWindowPos(
                g_EngineWindow,
                nullptr,
                g_ViewportRect.x,
                g_ViewportRect.y,
                g_ViewportRect.width,
                g_ViewportRect.height,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW
            );
            
            if (!success) {
                MOON_LOG_ERROR("EditorApp", "SetWindowPos failed: %lu", GetLastError());
            } else {
                MOON_LOG_INFO("EditorApp", "Viewport updated: (%d, %d) %dx%d", 
                             g_ViewportRect.x, g_ViewportRect.y,
                             g_ViewportRect.width, g_ViewportRect.height);
            }

            // 更新渲染器和相机
            if (g_ViewportRect.width > 0 && g_ViewportRect.height > 0) {
                renderer.Resize(g_ViewportRect.width, g_ViewportRect.height);
                float aspectRatio = static_cast<float>(g_ViewportRect.width) / 
                                   static_cast<float>(g_ViewportRect.height);
                camera->SetAspectRatio(aspectRatio);
            }
            
            g_ViewportRect.updated = false;
        }

        // 计算deltaTime
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - prevTime).count();
        prevTime = currentTime;

        // 更新引擎
        engine.Tick(deltaTime);
        cameraController.Update(deltaTime);

        // 动画物体
        float rotationSpeed = 45.0f;
        Moon::Vector3 cubeRot = cube->GetTransform()->GetLocalRotation();
        cube->GetTransform()->SetLocalRotation(Moon::Vector3(cubeRot.x, cubeRot.y + rotationSpeed * deltaTime, cubeRot.z));

        // 渲染场景
        Moon::Matrix4x4 viewProj = camera->GetViewProjectionMatrix();
        renderer.SetViewProjectionMatrix(&viewProj.m[0][0]);

        renderer.BeginFrame();
        scene->Traverse([&](Moon::SceneNode* node) {
            Moon::MeshRenderer* meshRenderer = node->GetComponent<Moon::MeshRenderer>();
            if (meshRenderer && meshRenderer->IsVisible() && meshRenderer->IsEnabled()) {
                meshRenderer->Render(&renderer);
            }
        });
        renderer.EndFrame();

        Sleep(1);
    }

    // ========================================
    // 5. 清理资源
    // ========================================
    std::cout << "[Editor] Shutting down..." << std::endl;

    renderer.Shutdown();
    g_Renderer = nullptr;
    g_CameraController = nullptr;
    g_Engine = nullptr;

    engine.Shutdown();

    if (g_EngineWindow) {
        DestroyWindow(g_EngineWindow);
        g_EngineWindow = nullptr;
    }

    bridge.Shutdown();

    Moon::Core::Logger::Shutdown();
    FreeConsole();
    return 0;
}
