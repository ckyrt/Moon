#include <windows.h>
#include <chrono>
#include <thread>
#include "../core/EngineCore.h"
#include "../core/Logging/Logger.h"
#include "../core/Camera/FPSCameraController.h"
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

        // Pass camera view-projection matrix to renderer
        Moon::Matrix4x4 viewProj = camera->GetViewProjectionMatrix();
        renderer.SetViewProjectionMatrix(&viewProj.m[0][0]);
        
        // Render
        renderer.RenderFrame();

        // simple throttle
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    renderer.Shutdown();
    g_pRenderer = nullptr;
    g_pCamera = nullptr;
    engine.Shutdown();
    
    // Shutdown logging system
    Moon::Core::Logger::Shutdown();
    
    return 0;
}
