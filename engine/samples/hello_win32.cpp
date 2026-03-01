#include <windows.h>
#include <chrono>
#include <thread>
#include "../core/EngineCore.h"
#include "../core/Logging/Logger.h"
#include "../core/Camera/FPSCameraController.h"
#include "../core/Profiling/FPSCounter.h"
#include "../core/Scene/Scene.h"
#include "../core/Scene/SceneNode.h"
#include "../core/Scene/MeshRenderer.h"
#include "../core/Scene/Material.h"
#include "../core/Scene/Light.h"
#include "../core/Scene/Skybox.h"
#include "../core/Mesh/Mesh.h"
#include "../core/Geometry/MeshGenerator.h"
#include "../core/CSG/CSGOperations.h"
#include "../render/IRenderer.h"
#include "../render/diligent/DiligentRenderer.h"
#include "../render/RenderCommon.h"
#include "../render/SceneRenderer.h"  // 🔄 使用通用场景渲染工具
#include "TestScenes.h"
#include "HelloEngineImGui.h"  // ImGui 模块

static const wchar_t* kWndClass = L"UGC_Editor_WndClass";

// Global renderer pointer for resize handling
static IRenderer* g_pRenderer = nullptr;
static Moon::PerspectiveCamera* g_pCamera = nullptr;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // ImGui处理消息
    if (HelloEngineImGui::HandleWin32Message(hWnd, msg, wParam, lParam))
        return true;
        
    switch (msg) {
    case WM_SIZE:
        if (g_pRenderer && wParam != SIZE_MINIMIZED) {
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            g_pRenderer->Resize(width, height);
            // Update camera aspect ratio
            if (g_pCamera && height > 0) {
                g_pCamera->SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
            }
        }
        break;
    case WM_PAINT:
        // Rendering will occur in our loop; here just validate.
        ValidateRect(hWnd, nullptr);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    // 设置工作目录到 exe 所在目录
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) {
        *lastSlash = L'\0';
        SetCurrentDirectoryW(exePath);
    }
    
    // 1) Register class
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = kWndClass;
    RegisterClassExW(&wc);

    // 2) Create window
    RECT rc = {0, 0, 1280, 720};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(
        0, kWndClass, L"Hello Engine (DiligentRenderer)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr
    );
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Initialize logging system
    Moon::Core::Logger::Init();

    // 3) Engine + Renderer
    EngineCore engine;
    engine.Initialize();

    // Get Camera and Input System from Engine
    Moon::PerspectiveCamera* camera = engine.GetCamera();
    Moon::InputSystem* inputSystem = engine.GetInputSystem();
    
    // Set window handle for proper mouse input
    inputSystem->SetWindowHandle(hwnd);
    
    // Set global camera pointer for resize handling
    g_pCamera = camera;
    
    // Update camera aspect ratio based on window size
    RECT cr;
    GetClientRect(hwnd, &cr);
    float width = static_cast<float>(cr.right - cr.left);
    float height = static_cast<float>(cr.bottom - cr.top);
    camera->SetAspectRatio(width / height);
    
    // Create FPS Camera Controller (Unity-like controls)
    Moon::FPSCameraController cameraController(camera, inputSystem);
    cameraController.SetMoveSpeed(5.0f);            // Movement speed (units/sec)
    cameraController.SetMouseSensitivity(20.0f);    // Mouse sensitivity

    DiligentRenderer renderer;
    g_pRenderer = &renderer;  // Set global pointer for resize handling
    
    RenderInitParams params{};
    params.windowHandle = hwnd;
    params.width  = cr.right - cr.left;
    params.height = cr.bottom - cr.top;
    
    if (!renderer.Initialize(params)) {
        MessageBoxA(hwnd, "Failed to initialize DiligentRenderer!", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }
    
    // ============================================================================
    // 初始化 ImGui 模块
    // ============================================================================
    HelloEngineImGui::Initialize(hwnd, &renderer);
    
    // ============================================================================
    // 各种常见材质的场景展示（Material Showcase Scene）
    // ============================================================================

    //TestScenes::TestCSG(&engine);
    //TestScenes::TestCSGBlueprint(&engine);  // 测试 JSON Blueprint 系统
    TestScenes::TestMaterial(&engine);
    //TestScenes::TestZipLoad(&engine);

    // ============================================================================
    // 创建主方向光（模拟太阳光）
    // ============================================================================
    MOON_LOG_INFO("Sample", "Creating main directional light...");
    
    Moon::Scene* scene = engine.GetScene();
    Moon::SceneNode* mainLightNode = scene->CreateNode("Main Light");
    
    // 设置光源方向：从右上前方照射（类似正午太阳）
    // Rotation: (45°, -30°, 0°) 表示向下 45° 并向左转 30°
    mainLightNode->GetTransform()->SetLocalRotation(Moon::Vector3(45.0f, -30.0f, 0.0f));
    
    // 添加 Light 组件
    Moon::Light* mainLight = mainLightNode->AddComponent<Moon::Light>();
    mainLight->SetType(Moon::Light::Type::Directional);
    mainLight->SetColor(Moon::Vector3(1.0f, 0.95f, 0.9f));  // 暖白色（太阳光）
    mainLight->SetIntensity(1.5f);  // 强度（0 = 无光，1.0 = 正常，> 1.0 = 明亮）
    
    MOON_LOG_INFO("Sample", "Main directional light created (Intensity: %.1f)", 
                  mainLight->GetIntensity());

    // Light direction test controls
    // Hold Left/Right arrow keys to rotate the directional light around world up.
    constexpr float kLightYawDegPerSec = 60.0f;

    // ============================================================================
    // 创建 Skybox（天空盒）
    // ============================================================================
    MOON_LOG_INFO("Sample", "Creating skybox...");
    
    Moon::SceneNode* skyboxNode = scene->CreateNode("Skybox");
    
    // 添加 Skybox 组件
    Moon::Skybox* skybox = skyboxNode->AddComponent<Moon::Skybox>();
    skybox->LoadEnvironmentMap("assets/textures/environment.hdr");
    skybox->SetEnableIBL(true);  // 启用基于图像的照明
    
    MOON_LOG_INFO("Sample", "Skybox created with HDR environment map");

    // 4) Main loop
    bool running = true;
    MSG msg;
    auto prev = std::chrono::high_resolution_clock::now();
    
    // FPS counter
    Moon::FPSCounter fpsCounter(60);
    auto lastTitleUpdate = prev;

    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        auto now = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double>(now - prev).count();
        prev = now;
        
        // Update FPS counter
        fpsCounter.Update(static_cast<float>(dt));
        
        // Update window title with FPS every 0.5 seconds
        auto timeSinceUpdate = std::chrono::duration<double>(now - lastTitleUpdate).count();
        if (timeSinceUpdate >= 0.5) {
            wchar_t title[256];
            swprintf_s(title, L"Moon Engine - HelloEngine | FPS: %.1f | Frame Time: %.2f ms", 
                      fpsCounter.GetFPS(), fpsCounter.GetFrameTimeMs());
            SetWindowTextW(hwnd, title);
            lastTitleUpdate = now;
        }

        engine.Tick(dt);
        
        // Update Camera Controller
        cameraController.Update(static_cast<float>(dt));

        // ============================================================================
        // Directional light direction testing
        // ============================================================================
        if (inputSystem && mainLightNode) {
            float yawDelta = 0.0f;
            if (inputSystem->IsKeyDown(Moon::KeyCode::Left))  yawDelta -= kLightYawDegPerSec * static_cast<float>(dt);
            if (inputSystem->IsKeyDown(Moon::KeyCode::Right)) yawDelta += kLightYawDegPerSec * static_cast<float>(dt);
            if (yawDelta != 0.0f) {
                mainLightNode->GetTransform()->Rotate(Moon::Vector3(0.0f, yawDelta, 0.0f), /*worldSpace*/ true);
            }
        }
        
        // ============================================================================
        // 渲染场景和ImGui UI
        // ============================================================================
        HelloEngineImGui::BeginFrame();
        
        renderer.BeginFrame();
        Moon::SceneRendererUtils::RenderScene(&renderer, scene, camera);
        
        // 渲染调试网格
        HelloEngineImGui::RenderDebugGrid(camera);
        
        // 渲染 ImGui UI（FPS等信息）
        HelloEngineImGui::RenderUI(camera, &fpsCounter);
        
        renderer.EndFrame();

        // simple throttle
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Cleanup (Meshes are owned by MeshRenderer components, no manual deletion needed)
    HelloEngineImGui::Shutdown();
    renderer.Shutdown();
    g_pRenderer = nullptr;
    g_pCamera = nullptr;
    engine.Shutdown();
    
    // Shutdown logging system
    Moon::Core::Logger::Shutdown();
    
    return 0;
}
