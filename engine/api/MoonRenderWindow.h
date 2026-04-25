#pragma once

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace Moon {
namespace Api {

struct RenderWindowDesc {
    std::wstring title = L"Moon Render Window";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool visible = true;
};

class RenderWindow {
public:
    using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;

    RenderWindow() = default;
    ~RenderWindow()
    {
        Destroy();
    }

    RenderWindow(const RenderWindow&) = delete;
    RenderWindow& operator=(const RenderWindow&) = delete;

    bool Create(const RenderWindowDesc& desc)
    {
        Destroy();

        m_width = desc.width;
        m_height = desc.height;
        m_shouldClose = false;

        HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &RenderWindow::WndProc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = ClassName();
        RegisterClassExW(&wc);

        RECT rect = { 0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height) };
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        m_hwnd = CreateWindowExW(
            0,
            ClassName(),
            desc.title.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            instance,
            this);

        if (!m_hwnd) {
            return false;
        }

        if (desc.visible) {
            ShowWindow(m_hwnd, SW_SHOW);
            UpdateWindow(m_hwnd);
        }

        return true;
    }

    void Destroy()
    {
        if (m_hwnd) {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
    }

    bool PollEvents()
    {
        MSG msg = {};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                m_shouldClose = true;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return !m_shouldClose;
    }

    bool ShouldClose() const { return m_shouldClose; }
    void RequestClose() { m_shouldClose = true; }

    void* GetNativeHandle() const { return m_hwnd; }
    HWND GetHwnd() const { return m_hwnd; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    void SetResizeCallback(ResizeCallback callback)
    {
        m_resizeCallback = std::move(callback);
    }

private:
    static const wchar_t* ClassName()
    {
        return L"MoonApiRenderWindow";
    }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        RenderWindow* window = nullptr;
        if (message == WM_NCCREATE) {
            CREATESTRUCTW* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            window = static_cast<RenderWindow*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        } else {
            window = reinterpret_cast<RenderWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (window) {
            return window->HandleMessage(hwnd, message, wParam, lParam);
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message) {
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                m_width = static_cast<uint32_t>(LOWORD(lParam));
                m_height = static_cast<uint32_t>(HIWORD(lParam));
                if (m_resizeCallback) {
                    m_resizeCallback(m_width, m_height);
                }
            }
            break;
        case WM_CLOSE:
            m_shouldClose = true;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (hwnd == m_hwnd) {
                m_hwnd = nullptr;
            }
            m_shouldClose = true;
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    HWND m_hwnd = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_shouldClose = false;
    ResizeCallback m_resizeCallback;
};

} // namespace Api
} // namespace Moon

#endif // _WIN32
