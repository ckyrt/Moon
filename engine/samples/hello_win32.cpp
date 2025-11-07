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
    
    // Setup Scene with multiple geometry types (Unity-like primitives showcase)
    Moon::Scene* scene = engine.GetScene();
    Moon::MeshManager* meshManager = engine.GetMeshManager();
    
    MOON_LOG_INFO("Sample", "Creating geometry showcase scene with MeshManager...");
    
    // Row 1: Basic shapes (Y = 0)
    // Cube (Red)
    Moon::SceneNode* cube = scene->CreateNode("Cube");
    cube->GetTransform()->SetLocalPosition(Moon::Vector3(-6.0f, 0.0f, 0.0f));
    Moon::MeshRenderer* cubeRenderer = cube->AddComponent<Moon::MeshRenderer>();
    cubeRenderer->SetMesh(meshManager->CreateCube(1.0f, Moon::Vector3(1.0f, 0.3f, 0.3f)));
    
    // Sphere (Green)
    Moon::SceneNode* sphere = scene->CreateNode("Sphere");
    sphere->GetTransform()->SetLocalPosition(Moon::Vector3(-3.0f, 0.0f, 0.0f));
    Moon::MeshRenderer* sphereRenderer = sphere->AddComponent<Moon::MeshRenderer>();
    sphereRenderer->SetMesh(meshManager->CreateSphere(0.6f, 32, 16, Moon::Vector3(0.3f, 1.0f, 0.3f)));
    
    // Cylinder (Blue)
    Moon::SceneNode* cylinder = scene->CreateNode("Cylinder");
    cylinder->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, 0.0f, 0.0f));
    Moon::MeshRenderer* cylinderRenderer = cylinder->AddComponent<Moon::MeshRenderer>();
    cylinderRenderer->SetMesh(meshManager->CreateCylinder(0.5f, 0.5f, 1.5f, 24, Moon::Vector3(0.3f, 0.5f, 1.0f)));
    
    // Cone (Yellow)
    Moon::SceneNode* cone = scene->CreateNode("Cone");
    cone->GetTransform()->SetLocalPosition(Moon::Vector3(3.0f, 0.0f, 0.0f));
    Moon::MeshRenderer* coneRenderer = cone->AddComponent<Moon::MeshRenderer>();
    coneRenderer->SetMesh(meshManager->CreateCone(0.6f, 1.5f, 24, Moon::Vector3(1.0f, 1.0f, 0.3f)));
    
    // Capsule (Magenta)
    Moon::SceneNode* capsule = scene->CreateNode("Capsule");
    capsule->GetTransform()->SetLocalPosition(Moon::Vector3(6.0f, 0.0f, 0.0f));
    Moon::MeshRenderer* capsuleRenderer = capsule->AddComponent<Moon::MeshRenderer>();
    capsuleRenderer->SetMesh(meshManager->CreateCapsule(0.4f, 2.0f, 16, 8, Moon::Vector3(1.0f, 0.3f, 1.0f)));
    
    // Row 2: Advanced shapes (Y = -3)
    // Torus (Cyan)
    Moon::SceneNode* torus = scene->CreateNode("Torus");
    torus->GetTransform()->SetLocalPosition(Moon::Vector3(-4.5f, -3.0f, 0.0f));
    Moon::MeshRenderer* torusRenderer = torus->AddComponent<Moon::MeshRenderer>();
    torusRenderer->SetMesh(meshManager->CreateTorus(0.6f, 0.2f, 32, 16, Moon::Vector3(0.3f, 1.0f, 1.0f)));
    
    // Plane (White)
    Moon::SceneNode* plane = scene->CreateNode("Plane");
    plane->GetTransform()->SetLocalPosition(Moon::Vector3(-1.5f, -3.0f, 0.0f));
    plane->GetTransform()->SetLocalRotation(Moon::Vector3(0.0f, 0.0f, 0.0f));
    Moon::MeshRenderer* planeRenderer = plane->AddComponent<Moon::MeshRenderer>();
    planeRenderer->SetMesh(meshManager->CreatePlane(1.5f, 1.5f, 2, 2, Moon::Vector3(0.9f, 0.9f, 0.9f)));
    
    // Quad (Orange)
    Moon::SceneNode* quad = scene->CreateNode("Quad");
    quad->GetTransform()->SetLocalPosition(Moon::Vector3(1.5f, -3.0f, 0.0f));
    quad->GetTransform()->SetLocalRotation(Moon::Vector3(0.0f, 45.0f, 0.0f));
    Moon::MeshRenderer* quadRenderer = quad->AddComponent<Moon::MeshRenderer>();
    quadRenderer->SetMesh(meshManager->CreateQuad(1.2f, 1.2f, Moon::Vector3(1.0f, 0.6f, 0.2f)));
    
    // Parent-child hierarchy demo (Right side, elevated)
    Moon::SceneNode* parent = scene->CreateNode("Parent");
    parent->GetTransform()->SetLocalPosition(Moon::Vector3(4.5f, -3.0f, 0.0f));
    
    Moon::SceneNode* child1 = scene->CreateNode("Child1");
    child1->SetParent(parent);
    child1->GetTransform()->SetLocalPosition(Moon::Vector3(-0.8f, 0.0f, 0.0f));
    Moon::MeshRenderer* child1Renderer = child1->AddComponent<Moon::MeshRenderer>();
    child1Renderer->SetMesh(meshManager->CreateCube(0.5f, Moon::Vector3(0.8f, 0.4f, 0.2f)));
    
    Moon::SceneNode* child2 = scene->CreateNode("Child2");
    child2->SetParent(parent);
    child2->GetTransform()->SetLocalPosition(Moon::Vector3(0.8f, 0.0f, 0.0f));
    Moon::MeshRenderer* child2Renderer = child2->AddComponent<Moon::MeshRenderer>();
    child2Renderer->SetMesh(meshManager->CreateSphere(0.3f, 16, 8, Moon::Vector3(0.2f, 0.8f, 0.4f)));
    
    MOON_LOG_INFO("Sample", "Scene created with 11 geometry primitives (managed by MeshManager, %zu meshes in cache)", 
                  meshManager->GetMeshCount());

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
        
        // Animate shapes for visual variety
        float rotationSpeed = 45.0f; // degrees per second
        
        // Rotate cube around Y axis
        Moon::Vector3 cubeRot = cube->GetTransform()->GetLocalRotation();
        cube->GetTransform()->SetLocalRotation(Moon::Vector3(cubeRot.x, cubeRot.y + rotationSpeed * static_cast<float>(dt), cubeRot.z));
        
        // Rotate sphere around X axis
        Moon::Vector3 sphereRot = sphere->GetTransform()->GetLocalRotation();
        sphere->GetTransform()->SetLocalRotation(Moon::Vector3(sphereRot.x + rotationSpeed * 0.5f * static_cast<float>(dt), sphereRot.y, sphereRot.z));
        
        // Rotate torus around Y axis (faster)
        Moon::Vector3 torusRot = torus->GetTransform()->GetLocalRotation();
        torus->GetTransform()->SetLocalRotation(Moon::Vector3(torusRot.x, torusRot.y + rotationSpeed * 1.5f * static_cast<float>(dt), torusRot.z));
        
        // Rotate parent hierarchy
        Moon::Vector3 parentRot = parent->GetTransform()->GetLocalRotation();
        parent->GetTransform()->SetLocalRotation(Moon::Vector3(parentRot.x, parentRot.y + 30.0f * static_cast<float>(dt), parentRot.z));

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

    // Cleanup (Meshes are owned by MeshRenderer components, no manual deletion needed)
    renderer.Shutdown();
    g_pRenderer = nullptr;
    g_pCamera = nullptr;
    engine.Shutdown();
    
    // Shutdown logging system
    Moon::Core::Logger::Shutdown();
    
    return 0;
}
