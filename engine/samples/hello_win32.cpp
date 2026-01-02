#include <windows.h>
#include <chrono>
#include <thread>
#include "../core/EngineCore.h"
#include "../core/Logging/Logger.h"
#include "../core/Camera/FPSCameraController.h"
#include "../core/Scene/Scene.h"
#include "../core/Scene/SceneNode.h"
#include "../core/Scene/MeshRenderer.h"
#include "../core/Scene/Material.h"
#include "../core/Scene/Light.h"
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
    // 各种常见材质的球体展示（Material Showcase）
    // ============================================================================
    Moon::Scene* scene = engine.GetScene();
    Moon::MeshManager* meshManager = engine.GetMeshManager();
    Moon::TextureManager* textureManager = engine.GetTextureManager();
    
    MOON_LOG_INFO("Sample", "Creating Material Showcase Scene...");
    
    // 参数设置
    const float SPACING = 3.0f;   // 球体间距
    const float RADIUS = 1.0f;    // 球体半径
    
    // 定义材质球体数组（名称 + 设置函数）
    struct MaterialBall {
        const char* name;
        Moon::Vector3 position;
        void (*setupFunc)(Moon::Material*);
    };
    
    // 材质设置函数
    auto setupBrick = [](Moon::Material* m) { m->SetPresetBrick(); };
    auto setupWood = [](Moon::Material* m) { m->SetPresetWood(); };
    auto setupPlastic = [](Moon::Material* m) { m->SetPresetPlastic(); };
    auto setupIron = [](Moon::Material* m) { m->SetPresetIron(); };
    auto setupRubber = [](Moon::Material* m) { m->SetPresetRubber(); };
    auto setupPlaster = [](Moon::Material* m) { m->SetPresetPlaster(); };
    auto setupConcrete = [](Moon::Material* m) { m->SetPresetConcrete(); };
    auto setupPolishedMetal = [](Moon::Material* m) { m->SetPresetPolishedMetal(); };
    auto setupRoughMetal = [](Moon::Material* m) { m->SetPresetMetal(0.8f); };
    
    // 布局：3行3列
    MaterialBall materialBalls[] = {
        // 第一排（前排）
        {"Brick",          {-SPACING, RADIUS, -SPACING}, setupBrick},
        {"Wood",           {0.0f,     RADIUS, -SPACING}, setupWood},
        {"Plastic",        {SPACING,  RADIUS, -SPACING}, setupPlastic},
        
        // 第二排（中排）
        {"Iron",           {-SPACING, RADIUS, 0.0f},     setupIron},
        {"Polished Metal", {0.0f,     RADIUS, 0.0f},     setupPolishedMetal},
        {"Rubber",         {SPACING,  RADIUS, 0.0f},     setupRubber},
        
        // 第三排（后排）
        {"Plaster",        {-SPACING, RADIUS, SPACING},  setupPlaster},
        {"Concrete",       {0.0f,     RADIUS, SPACING},  setupConcrete},
        {"Rough Metal",    {SPACING,  RADIUS, SPACING},  setupRoughMetal},
    };
    
    // 创建材质球体
    for (const auto& ball : materialBalls) {
        Moon::SceneNode* sphereNode = scene->CreateNode(ball.name);
        sphereNode->GetTransform()->SetLocalPosition(ball.position);
        
        // 添加 MeshRenderer 组件
        Moon::MeshRenderer* renderer = sphereNode->AddComponent<Moon::MeshRenderer>();
        renderer->SetMesh(meshManager->CreateSphere(RADIUS, 64, 32, Moon::Vector3(1.0f, 1.0f, 1.0f)));
        
        // 添加 Material 组件并设置材质
        Moon::Material* material = sphereNode->AddComponent<Moon::Material>();
        ball.setupFunc(material);
        
        MOON_LOG_INFO("Sample", "Created %s sphere at (%.1f, %.1f, %.1f)", 
                      ball.name, ball.position.x, ball.position.y, ball.position.z);
    }
    
    MOON_LOG_INFO("Sample", "Material Showcase created: %d spheres with different materials", 
                  sizeof(materialBalls) / sizeof(materialBalls[0]));

    
    // ============================================================================
    // 创建地面平面
    // ============================================================================
    MOON_LOG_INFO("Sample", "Creating ground plane...");
    
    Moon::SceneNode* groundNode = scene->CreateNode("Ground");
    groundNode->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, 0.0f, 0.0f));  // 地面在 Y=0
    
    // 添加 MeshRenderer 组件
    Moon::MeshRenderer* groundRenderer = groundNode->AddComponent<Moon::MeshRenderer>();
    groundRenderer->SetMesh(meshManager->CreatePlane(
        50.0f,   // width (X轴)
        50.0f,   // depth (Z轴)
        1,       // subdivisionsX
        1,       // subdivisionsZ
        Moon::Vector3(0.3f, 0.3f, 0.3f)  // 深灰色（降低反射率）
    ));
    
    // 添加 Material 组件（混凝土材质）
    Moon::Material* groundMaterial = groundNode->AddComponent<Moon::Material>();
    groundMaterial->SetPresetConcrete();
    
    MOON_LOG_INFO("Sample", "Ground plane created (50x50 units)");
    
    // ============================================================================
    // 创建主方向光（模拟太阳光）
    // ============================================================================
    MOON_LOG_INFO("Sample", "Creating main directional light...");
    
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
        
        // Update scene lights (收集场景中所有 Light 组件)
        renderer.UpdateSceneLights(scene);
        
        // Set camera position for PBR specular calculation (在 UpdateSceneLights 之后调用)
        Moon::Vector3 cameraPos = camera->GetPosition();
        renderer.SetCameraPosition(cameraPos);
        
        // Render all scene objects with PBR material parameters
        renderer.BeginFrame();
        
        scene->Traverse([&](Moon::SceneNode* node) {
            Moon::MeshRenderer* meshRenderer = node->GetComponent<Moon::MeshRenderer>();
            if (meshRenderer && meshRenderer->IsVisible() && meshRenderer->IsEnabled()) {
                // ✅ 从 Material 组件获取材质参数
                Moon::Material* material = node->GetComponent<Moon::Material>();
                if (material && material->IsEnabled()) {
                    renderer.SetMaterialParameters(
                        material->GetMetallic(), 
                        material->GetRoughness(),
                        material->GetBaseColor()
                    );
                } else {
                    // 默认材质参数
                    renderer.SetMaterialParameters(0.0f, 0.5f, Moon::Vector3(1.0f, 1.0f, 1.0f));
                }
                
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
