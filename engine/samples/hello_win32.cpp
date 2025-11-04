#include <windows.h>
#include <chrono>
#include <thread>
#include "../core/EngineCore.h"
#include "../render/IRenderer.h"
#include "../render/NullRenderer.h"
#include "../render/RenderCommon.h"

static const wchar_t* kWndClass = L"UGC_Editor_WndClass";

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_SIZE:
        InvalidateRect(hWnd, nullptr, FALSE);
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
        0, kWndClass, L"Hello Engine (NullRenderer)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr
    );
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // 3) Engine + Renderer
    EngineCore engine;
    engine.Initialize();

    NullRenderer renderer;
    RenderInitParams params{};
    params.windowHandle = hwnd;
    RECT cr;
    GetClientRect(hwnd, &cr);
    params.width  = cr.right - cr.left;
    params.height = cr.bottom - cr.top;
    renderer.Initialize(params);

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

        // Render
        renderer.RenderFrame();

        // simple throttle
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    renderer.Shutdown();
    engine.Shutdown();
    return 0;
}
