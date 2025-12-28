#include <windows.h>
#include <chrono>
#include <thread>
#include "../core/EngineCore.h"
#include "../core/Logging/Logger.h"
#include "../core/Camera/FPSCameraController.h"
#include "../core/Scene/Scene.h"
#include "../core/Scene/SceneNode.h"
#include "../core/Scene/MeshRenderer.h"
#include "../core/Mesh/Mesh.h"
#include "../core/Geometry/MeshGenerator.h"
#include "../render/IRenderer.h"
#include "../render/DiligentRenderer.h"
#include "../render/RenderCommon.h"

static const wchar_t* kWndClass = L"UGC_Editor_WndClass";

// Global renderer pointer for resize handling
static IRenderer* g_pRenderer = nullptr;
static Moon::PerspectiveCamera* g_pCamera = nullptr;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
    cameraController.SetMoveSpeed(10.0f);           // Movement speed (units/sec)
    cameraController.SetMouseSensitivity(30.0f);     // Mouse sensitivity (1.0=slow, 10.0=fast)

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
    // PBR Material Ball Grid (行业标准 Debug Scene)
    // ============================================================================
    Moon::Scene* scene = engine.GetScene();
    Moon::MeshManager* meshManager = engine.GetMeshManager();
    
    MOON_LOG_INFO("Sample", "Creating PBR Material Ball Grid...");
    
    // 参数设置
    const int ROWS = 5;        // Y 轴：Metallic 0 → 1
    const int COLS = 5;        // X 轴：Roughness 0 → 1
    const float SPACING = 2.5f;
    const float RADIUS = 0.8f;
    
    // 白色基础颜色（更好地展示 PBR 效果）
    Moon::Vector3 baseColor(0.9f, 0.9f, 0.9f);
    
    // 创建球阵列
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            // 计算材质参数
            float metallic = static_cast<float>(row) / (ROWS - 1);      // 0 → 1 (从下到上)
            float roughness = static_cast<float>(col) / (COLS - 1);     // 0 → 1 (从左到右)
            
            // 计算位置（居中）
            float x = (col - (COLS - 1) * 0.5f) * SPACING;
            float y = (row - (ROWS - 1) * 0.5f) * SPACING;
            float z = 0.0f;
            
            // 创建球体节点
            char nodeName[64];
            snprintf(nodeName, sizeof(nodeName), "Sphere_M%.2f_R%.2f", metallic, roughness);
            
            Moon::SceneNode* sphereNode = scene->CreateNode(nodeName);
            sphereNode->GetTransform()->SetLocalPosition(Moon::Vector3(x, y, z));
            
            Moon::MeshRenderer* renderer = sphereNode->AddComponent<Moon::MeshRenderer>();
            renderer->SetMesh(meshManager->CreateSphere(RADIUS, 32, 16, baseColor));
            
            // 存储材质参数（暂时通过用户数据或标签，后续会用材质系统）
            // TODO: 实现材质系统后替换为 Material Component
        }
    }
    
    MOON_LOG_INFO("Sample", "PBR Grid created: %dx%d spheres (%d total)", 
                  COLS, ROWS, COLS * ROWS);

    // 4) Main loop
    bool running = true;
    MSG msg;
    auto prev = std::chrono::high_resolution_clock::now();

    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        auto now = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double>(now - prev).count();
        prev = now;

        engine.Tick(dt);
        
        // Update Camera Controller
        cameraController.Update(static_cast<float>(dt));

        // Set camera for rendering
        Moon::Matrix4x4 viewProj = camera->GetViewProjectionMatrix();
        renderer.SetViewProjectionMatrix(&viewProj.m[0][0]);
        
        // Set camera position for PBR specular calculation
        Moon::Vector3 cameraPos = camera->GetPosition();
        renderer.SetCameraPosition(cameraPos);
        
        // Render all scene objects with PBR material parameters
        renderer.BeginFrame();
        
        scene->Traverse([&](Moon::SceneNode* node) {
            Moon::MeshRenderer* meshRenderer = node->GetComponent<Moon::MeshRenderer>();
            if (meshRenderer && meshRenderer->IsVisible() && meshRenderer->IsEnabled()) {
                // 从节点名称解析材质参数 (格式: "Sphere_M0.25_R0.75")
                float metallic = 0.0f;
                float roughness = 0.5f;
                
                const std::string& nodeName = node->GetName();
                if (nodeName.find("Sphere_M") != std::string::npos) {
                    sscanf_s(nodeName.c_str(), "Sphere_M%f_R%f", &metallic, &roughness);
                }
                
                // 设置材质参数
                renderer.SetMaterialParameters(metallic, roughness);
                
                // 渲染
                meshRenderer->Render(&renderer);
            }
        });
        
        // 渲染 Skybox（在所有不透明物体之后）
        renderer.RenderSkybox();
        
        renderer.EndFrame();

        // simple throttle
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Cleanup (Meshes are owned by MeshRenderer components, no manual deletion needed)
    renderer.Shutdown();
    g_pRenderer = nullptr;
    g_pCamera = nullptr;
    engine.Shutdown();
    
    // Shutdown logging system
    Moon::Core::Logger::Shutdown();
    
    return 0;
}
