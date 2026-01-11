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
#include "../core/Scene/Skybox.h"
#include "../core/Mesh/Mesh.h"
#include "../core/Geometry/MeshGenerator.h"
#include "../core/CSG/CSGOperations.h"
#include "../render/IRenderer.h"
#include "../render/diligent/DiligentRenderer.h"
#include "../render/RenderCommon.h"
#include "../render/SceneRenderer.h"  // 🔄 使用通用场景渲染工具
#include "CSGTest.h"  // CSG 测试

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
    
    // 运行 CSG 测试
    MOON_LOG_INFO("HelloEngine", "Running CSG tests...");
    if (Moon::CSGTest::RunTests()) {
        MOON_LOG_INFO("HelloEngine", "✓ CSG tests passed!");
    } else {
        MOON_LOG_ERROR("HelloEngine", "✗ CSG tests failed!");
    }

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
    // 各种常见材质的场景展示（Material Showcase Scene）
    // ============================================================================
    Moon::Scene* scene = engine.GetScene();
    Moon::MeshManager* meshManager = engine.GetMeshManager();
    Moon::TextureManager* textureManager = engine.GetTextureManager();
    
    MOON_LOG_INFO("Sample", "Creating Material Showcase Scene...");
    
    // ============================================================================
    // 1. 地面 - rock_terrain (大平面)
    // ============================================================================
    Moon::SceneNode* groundNode = scene->CreateNode("Ground_RockTerrain");
    groundNode->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, 0.0f, 0.0f));
    groundNode->GetTransform()->SetLocalScale(Moon::Vector3(2.0f, 1.0f, 2.0f));  // 放大地面
    
    Moon::MeshRenderer* groundRenderer = groundNode->AddComponent<Moon::MeshRenderer>();
    groundRenderer->SetMesh(meshManager->CreatePlane(25.0f, 25.0f, 1, 1, Moon::Vector3(0.5f, 0.5f, 0.5f)));
    
    Moon::Material* groundMaterial = groundNode->AddComponent<Moon::Material>();
    groundMaterial->SetMaterialPreset(Moon::MaterialPreset::Concrete);
    groundMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 普通mesh使用UV
    
    // ============================================================================
    // 2. 石膏墙立方体 - painted_plaster_wall
    // ============================================================================
    Moon::SceneNode* plasterCubeNode = scene->CreateNode("PlasterWall_Cube");
    plasterCubeNode->GetTransform()->SetLocalPosition(Moon::Vector3(-5.0f, 2.0f, 0.0f));
    plasterCubeNode->GetTransform()->SetLocalScale(Moon::Vector3(2.0f, 4.0f, 0.5f));  // 墙的形状
    
    Moon::MeshRenderer* plasterRenderer = plasterCubeNode->AddComponent<Moon::MeshRenderer>();
    plasterRenderer->SetMesh(meshManager->CreateCube(1.0f, Moon::Vector3(0.95f, 0.95f, 0.92f)));
    
    Moon::Material* plasterMaterial = plasterCubeNode->AddComponent<Moon::Material>();
    plasterMaterial->SetMaterialPreset(Moon::MaterialPreset::Plastic);
    plasterMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 普通mesh使用UV
    
    // ============================================================================
    // 3. 红砖墙立方体 - red_brick
    // ============================================================================
    Moon::SceneNode* brickWallNode = scene->CreateNode("BrickWall_Cube");
    brickWallNode->GetTransform()->SetLocalPosition(Moon::Vector3(5.0f, 2.0f, 0.0f));
    brickWallNode->GetTransform()->SetLocalScale(Moon::Vector3(2.0f, 4.0f, 0.5f));  // 墙的形状
    
    Moon::MeshRenderer* brickRenderer = brickWallNode->AddComponent<Moon::MeshRenderer>();
    brickRenderer->SetMesh(meshManager->CreateCube(1.0f, Moon::Vector3(0.6f, 0.3f, 0.2f)));
    
    Moon::Material* brickMaterial = brickWallNode->AddComponent<Moon::Material>();
    brickMaterial->SetMaterialPreset(Moon::MaterialPreset::Rock);
    brickMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 普通mesh使用UV
    
    // ============================================================================
    // 4. 橡胶跑道矩形 - rubberized_track
    // ============================================================================
    Moon::SceneNode* trackNode = scene->CreateNode("RubberTrack_Rectangle");
    trackNode->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, 0.05f, -5.0f));  // 稍微抬高避免 z-fighting
    trackNode->GetTransform()->SetLocalScale(Moon::Vector3(8.0f, 1.0f, 3.0f));  // 跑道形状
    
    Moon::MeshRenderer* trackRenderer = trackNode->AddComponent<Moon::MeshRenderer>();
    trackRenderer->SetMesh(meshManager->CreatePlane(1.0f, 1.0f, 1, 1, Moon::Vector3(0.2f, 0.2f, 0.2f)));
    
    Moon::Material* trackMaterial = trackNode->AddComponent<Moon::Material>();
    trackMaterial->SetMaterialPreset(Moon::MaterialPreset::Fabric);
    trackMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 普通mesh使用UV
    
    // ============================================================================
    // 5. 生锈金属球 - rusty_metal
    // ============================================================================
    Moon::SceneNode* metalSphereNode = scene->CreateNode("RustyMetal_Sphere");
    metalSphereNode->GetTransform()->SetLocalPosition(Moon::Vector3(-2.0f, 1.5f, 3.0f));
    
    Moon::MeshRenderer* metalRenderer = metalSphereNode->AddComponent<Moon::MeshRenderer>();
    metalRenderer->SetMesh(meshManager->CreateSphere(1.0f, 64, 32, Moon::Vector3(1.0f, 1.0f, 1.0f)));
    
    Moon::Material* metalMaterial = metalSphereNode->AddComponent<Moon::Material>();
    metalMaterial->SetMaterialPreset(Moon::MaterialPreset::Metal);
    metalMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 普通mesh使用UV
    
    // ============================================================================
    // 6. 木头球 - wood_floor
    // ============================================================================
    Moon::SceneNode* woodSphereNode = scene->CreateNode("Wood_Sphere");
    woodSphereNode->GetTransform()->SetLocalPosition(Moon::Vector3(2.0f, 1.5f, 3.0f));
    
    Moon::MeshRenderer* woodRenderer = woodSphereNode->AddComponent<Moon::MeshRenderer>();
    woodRenderer->SetMesh(meshManager->CreateSphere(1.0f, 64, 32, Moon::Vector3(1.0f, 1.0f, 1.0f)));
    
    Moon::Material* woodMaterial = woodSphereNode->AddComponent<Moon::Material>();
    woodMaterial->SetMaterialPreset(Moon::MaterialPreset::Wood);
    woodMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 普通mesh使用UV
    
    // ============================================================================
    // 7. CSG 演示场景 - Box - Sphere (布尔减法)
    // ============================================================================
    MOON_LOG_INFO("Sample", "Creating CSG demonstration object...");
    
    // 创建 CSG Box（flatShading=false用于布尔操作，保留共享顶点）
    auto csgBox = Moon::CSG::CreateCSGBox(2.0f, 2.0f, 2.0f, 
                                          Moon::Vector3(0, 0, 0), 
                                          Moon::Vector3(0, 0, 0), 
                                          Moon::Vector3(1, 1, 1), 
                                          false);  // 保留共享顶点用于Boolean
    if (csgBox && csgBox->IsValid()) {
        MOON_LOG_INFO("Sample", "CSG Box created: %zu vertices, %zu triangles", 
            csgBox->GetVertexCount(), csgBox->GetTriangleCount());
    }
    
    // 创建 CSG Sphere（flatShading=false用于布尔操作）
    auto csgSphere = Moon::CSG::CreateCSGSphere(1.2f, 32, 
                                                Moon::Vector3(0, 0, 0), 
                                                Moon::Vector3(0, 0, 0), 
                                                Moon::Vector3(1, 1, 1), 
                                                false);  // 保留共享顶点用于Boolean
    if (csgSphere && csgSphere->IsValid()) {
        MOON_LOG_INFO("Sample", "CSG Sphere created: %zu vertices, %zu triangles", 
            csgSphere->GetVertexCount(), csgSphere->GetTriangleCount());
    }
    
    // 执行布尔减法：Box - Sphere（从立方体中挖出球形）
    auto csgResult = Moon::CSG::PerformBoolean(csgBox.get(), csgSphere.get(), Moon::CSG::Operation::Subtract);
    if (csgResult && csgResult->IsValid()) {
        MOON_LOG_INFO("Sample", "CSG Boolean Subtract successful: %zu vertices, %zu triangles", 
            csgResult->GetVertexCount(), csgResult->GetTriangleCount());
        
        // 添加到场景
        Moon::SceneNode* csgNode = scene->CreateNode("CSG_Box_Minus_Sphere");
        csgNode->GetTransform()->SetLocalPosition(Moon::Vector3(0.0f, 2.0f, 5.0f));
        
        Moon::MeshRenderer* csgRenderer = csgNode->AddComponent<Moon::MeshRenderer>();
        csgRenderer->SetMesh(csgResult);
        
        Moon::Material* csgMaterial = csgNode->AddComponent<Moon::Material>();
        csgMaterial->SetMaterialPreset(Moon::MaterialPreset::Metal);
        csgMaterial->SetMappingMode(Moon::MappingMode::Triplanar);  // ✅ CSG必须用Triplanar
        csgMaterial->SetTriplanarTiling(0.5f);
        csgMaterial->SetTriplanarBlend(4.0f);
        
        MOON_LOG_INFO("Sample", "CSG object added to scene at position (0, 2, 5)");
    } else {
        MOON_LOG_ERROR("Sample", "CSG Boolean operation failed!");
    }
    
    // ============================================================================
    // 8. UV vs Triplanar 对比测试 - 拉伸的立方体（更明显的差异）
    // ============================================================================
    MOON_LOG_INFO("Sample", "Creating UV vs Triplanar comparison test with stretched cubes...");
    
    // 8a. 普通立方体 - UV模式 + 拉伸 → 纹理会变形
    Moon::SceneNode* uvCubeNode = scene->CreateNode("UV_Cube_Stretched");
    uvCubeNode->GetTransform()->SetLocalPosition(Moon::Vector3(-3.0f, 2.0f, -3.0f));
    uvCubeNode->GetTransform()->SetLocalScale(Moon::Vector3(1.0f, 3.0f, 1.0f));  // ✅ 拉高3倍
    
    Moon::MeshRenderer* uvCubeRenderer = uvCubeNode->AddComponent<Moon::MeshRenderer>();
    uvCubeRenderer->SetMesh(meshManager->CreateCube(1.0f, Moon::Vector3(1.0f, 1.0f, 1.0f)));
    
    Moon::Material* uvCubeMaterial = uvCubeNode->AddComponent<Moon::Material>();
    uvCubeMaterial->SetMaterialPreset(Moon::MaterialPreset::Rock);  // 砖块纹理更明显
    uvCubeMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ 显式UV模式
    uvCubeMaterial->SetMappingMode(Moon::MappingMode::UV);  // ✅ UV模式：拉伸后纹理变形
    
    MOON_LOG_INFO("Sample", "UV Cube (stretched 1x3x1) created at (-3, 2, -3) - texture will be stretched");
    
    // 8b. CSG立方体 - Triplanar模式 + 拉伸 → 纹理保持正确比例
    auto csgCube = Moon::CSG::CreateCSGBox(1.0f, 1.0f, 1.0f,
                                           Moon::Vector3(0, 0, 0),
                                           Moon::Vector3(0, 0, 0),
                                           Moon::Vector3(1, 1, 1),
                                           true);  // flatShading=true
    
    if (csgCube && csgCube->IsValid()) {
        Moon::SceneNode* triplanarCubeNode = scene->CreateNode("Triplanar_CSG_Cube_Stretched");
        triplanarCubeNode->GetTransform()->SetLocalPosition(Moon::Vector3(3.0f, 2.0f, -3.0f));
        triplanarCubeNode->GetTransform()->SetLocalScale(Moon::Vector3(1.0f, 3.0f, 1.0f));  // ✅ 同样拉高3倍
        
        Moon::MeshRenderer* triplanarCubeRenderer = triplanarCubeNode->AddComponent<Moon::MeshRenderer>();
        triplanarCubeRenderer->SetMesh(csgCube);
        
        Moon::Material* triplanarCubeMaterial = triplanarCubeNode->AddComponent<Moon::Material>();
        triplanarCubeMaterial->SetMaterialPreset(Moon::MaterialPreset::Rock);  // 相同砖块纹理
        triplanarCubeMaterial->SetMappingMode(Moon::MappingMode::Triplanar);  // ✅ CSG必须用Triplanar
        triplanarCubeMaterial->SetTriplanarTiling(0.5f);  // 每2米重复
        triplanarCubeMaterial->SetTriplanarBlend(4.0f);
        
        MOON_LOG_INFO("Sample", "CSG Triplanar Cube (stretched 1x3x1) created at (3, 2, -3) - texture density stays correct");
    } else {
        MOON_LOG_ERROR("Sample", "Failed to create CSG test cube!");
    }
    
    MOON_LOG_INFO("Sample", "✅ UV vs Triplanar comparison: Left=UV (stretched texture), Right=Triplanar (correct density)");
    MOON_LOG_INFO("Sample", "Material Showcase Scene created with 9 objects");
    
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

    // ============================================================================
    // 创建 Skybox（天空盒）
    // ============================================================================
    MOON_LOG_INFO("Sample", "Creating skybox...");
    
    Moon::SceneNode* skyboxNode = scene->CreateNode("Skybox");
    
    // 添加 Skybox 组件
    Moon::Skybox* skybox = skyboxNode->AddComponent<Moon::Skybox>();
    skybox->LoadEnvironmentMap("assets/textures/environment.hdr");
    skybox->SetIntensity(1.0f);  // 天空盒亮度
    skybox->SetEnableIBL(true);  // 启用基于图像的照明
    
    MOON_LOG_INFO("Sample", "Skybox created with HDR environment map");

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

        // 🔄 使用通用的场景渲染工具（替换重复代码）
        renderer.BeginFrame();
        Moon::SceneRendererUtils::RenderScene(&renderer, scene, camera);
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
