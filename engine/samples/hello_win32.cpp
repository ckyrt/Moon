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
    
    // Setup Scene with multiple cubes
    Moon::Scene* scene = engine.GetScene();
    
    // Create shared cube mesh (all cubes use the same geometry)
    Moon::Mesh* cubeMesh = Moon::CreateCubeMesh(1.0f);
    
    // Create cube 1 (left, rotating)
    Moon::SceneNode* cube1 = scene->CreateNode("Cube1");
    cube1->GetTransform()->SetLocalPosition(Moon::Vector3(-3.0f, 0.0f, 0.0f));
    Moon::MeshRenderer* renderer1 = cube1->AddComponent<Moon::MeshRenderer>();
    renderer1->SetMesh(cubeMesh);
    
    // Create cube 2 (center, stationary)
    Moon::SceneNode* cube2 = scene->CreateNode("Cube2");
    cube2->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, 0.0f, 0.0f));
    Moon::MeshRenderer* renderer2 = cube2->AddComponent<Moon::MeshRenderer>();
    renderer2->SetMesh(cubeMesh);
    
    // Create cube 3 (right, elevated)
    Moon::SceneNode* cube3 = scene->CreateNode("Cube3");
    cube3->GetTransform()->SetLocalPosition(Moon::Vector3(3.0f, 2.0f, 0.0f));
    Moon::MeshRenderer* renderer3 = cube3->AddComponent<Moon::MeshRenderer>();
    renderer3->SetMesh(cubeMesh);
    
    // Create a parent-child hierarchy
    Moon::SceneNode* parent = scene->CreateNode("Parent");
    parent->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, -3.0f, 5.0f));
    
    Moon::SceneNode* child1 = scene->CreateNode("Child1");
    child1->SetParent(parent);
    child1->GetTransform()->SetLocalPosition(Moon::Vector3(-1.5f, 0.0f, 0.0f));
    Moon::MeshRenderer* rendererChild1 = child1->AddComponent<Moon::MeshRenderer>();
    rendererChild1->SetMesh(cubeMesh);
    
    Moon::SceneNode* child2 = scene->CreateNode("Child2");
    child2->SetParent(parent);
    child2->GetTransform()->SetLocalPosition(Moon::Vector3(1.5f, 0.0f, 0.0f));
    Moon::MeshRenderer* rendererChild2 = child2->AddComponent<Moon::MeshRenderer>();
    rendererChild2->SetMesh(cubeMesh);
    
    MOON_LOG_INFO("Sample", "Created %d scene nodes with shared mesh", scene->GetRootNodeCount());

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
        
        // Animate cube1 (rotate around Y axis)
        float rotationSpeed = 45.0f; // degrees per second
        Moon::Vector3 currentRot = cube1->GetTransform()->GetLocalRotation();
        cube1->GetTransform()->SetLocalRotation(Moon::Vector3(currentRot.x, currentRot.y + rotationSpeed * static_cast<float>(dt), currentRot.z));
        
        // Animate parent (rotate the hierarchy)
        currentRot = parent->GetTransform()->GetLocalRotation();
        parent->GetTransform()->SetLocalRotation(Moon::Vector3(currentRot.x, currentRot.y + 30.0f * static_cast<float>(dt), currentRot.z));

        // Set camera for rendering
        Moon::Matrix4x4 viewProj = camera->GetViewProjectionMatrix();
        renderer.SetViewProjectionMatrix(&viewProj.m[0][0]);
        
        // Render all scene objects
        renderer.BeginFrame();
        
        scene->Traverse([&](Moon::SceneNode* node) {
            Moon::MeshRenderer* meshRenderer = node->GetComponent<Moon::MeshRenderer>();
            if (meshRenderer && meshRenderer->IsVisible() && meshRenderer->IsEnabled()) {
                meshRenderer->Render(&renderer);
            }
        });
        
        renderer.EndFrame();

        // simple throttle
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // Cleanup
    delete cubeMesh;
    
    renderer.Shutdown();
    g_pRenderer = nullptr;
    g_pCamera = nullptr;
    engine.Shutdown();
    
    // Shutdown logging system
    Moon::Core::Logger::Shutdown();
    
    return 0;
}
