// ============================================================================
// EditorBridge.cpp - CEF 编辑器桥接层 (Refined Version)
// ============================================================================
// 保持原始功能完全不变
// 清晰拆分：初始化 → 创建窗口 → 创建浏览器 → 消息循环 → Shutdown
// 仅进行结构与可读性优化，不改变逻辑
// ============================================================================

#include "EditorBridge.h"
#include "cef/CefApp.h"
#include "cef/CefRenderHandler.h"
#include "include/cef_browser.h"

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM, GET_WHEEL_DELTA_WPARAM

// ============================================================================
// 全局回调：CEF主窗口尺寸变化时通知
// ============================================================================
static std::function<void(int, int)> g_OnCefWindowResize = nullptr;

void SetCefWindowResizeCallback(std::function<void(int, int)> callback) {
    g_OnCefWindowResize = callback;
}

// ============================================================================
// 全局变量：Viewport区域信息（用于判断事件是否在3D区域）
// ============================================================================
struct ViewportInfo {
    int x, y, width, height;
    bool valid = false;
};

static ViewportInfo g_ViewportInfo = {};

void SetViewportInfo(int x, int y, int width, int height) {
    g_ViewportInfo.x = x;
    g_ViewportInfo.y = y;
    g_ViewportInfo.width = width;
    g_ViewportInfo.height = height;
    g_ViewportInfo.valid = true;
}

// 判断点是否在viewport区域内
static bool IsInViewport(int x, int y) {
    if (!g_ViewportInfo.valid) return false;
    return (x >= g_ViewportInfo.x && x < g_ViewportInfo.x + g_ViewportInfo.width &&
            y >= g_ViewportInfo.y && y < g_ViewportInfo.y + g_ViewportInfo.height);
}

// ============================================================================
// 全局回调：转发viewport区域的鼠标事件到引擎WndProc
// ============================================================================
static std::function<void(UINT, WPARAM, LPARAM)> g_EngineWndProcCallback = nullptr;
static std::function<bool()> g_ImGuiWantsCaptureCallback = nullptr;  // 检查ImGui是否需要鼠标
static std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> g_ImGuiWin32ProcCallback = nullptr;  // ImGui的Win32事件处理

void SetEngineWndProcCallback(std::function<void(UINT, WPARAM, LPARAM)> callback) {
    g_EngineWndProcCallback = callback;
}

void SetImGuiWantsCaptureCallback(std::function<bool()> callback) {
    g_ImGuiWantsCaptureCallback = callback;
}

void SetImGuiWin32ProcCallback(std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> callback) {
    g_ImGuiWin32ProcCallback = callback;
}

// ============================================================================
// 辅助：获取 EXE 路径
// ============================================================================
static std::string GetExecutableDir()
{
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path().string();
}

// ============================================================================
// Windows 主窗口过程
// ============================================================================
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    EditorBridge* bridge =
        reinterpret_cast<EditorBridge*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    // ✅ 优先传递给ImGui处理
    if (g_ImGuiWin32ProcCallback) {
        LRESULT result = g_ImGuiWin32ProcCallback(hwnd, msg, wParam, lParam);
        if (result) {
            return result;  // ImGui处理了事件
        }
    }

    switch (msg) {

    case WM_SIZE:
        if (bridge && bridge->GetClient()) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            int width = rc.right - rc.left;
            int height = rc.bottom - rc.top;
            
            // OSR模式：更新render handler尺寸并通知CEF
            auto renderHandler = bridge->GetClient()->GetRenderHandlerImpl();
            if (renderHandler) {
                renderHandler->SetViewportSize(width, height);
                if (bridge->GetClient()->GetBrowser()) {
                    bridge->GetClient()->GetBrowser()->GetHost()->WasResized();
                }
            }
            
            // 通知主程序CEF窗口尺寸变化
            if (g_OnCefWindowResize) {
                g_OnCefWindowResize(width, height);
            }
        }
        return 0;

    // ============================================================================
    // 鼠标事件：业界标准分发流程
    // 1. ImGui优先（WantCaptureMouse检查）→ ImGui处理，其他层忽略
    // 2. CEF次之（UI元素响应）→ CEF处理，3D场景忽略  
    // 3. 3D场景最后（picking等）
    // ============================================================================
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_MOUSEWHEEL:
    {
        if (bridge && bridge->GetClient() && bridge->GetClient()->GetBrowser()) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            
            // 步骤1: 检查ImGui是否需要这个事件（WantCaptureMouse）
            bool imguiWantsMouse = g_ImGuiWantsCaptureCallback ? g_ImGuiWantsCaptureCallback() : false;
            bool inViewport = IsInViewport(x, y);
            
            if (imguiWantsMouse) {
                // ImGui（包括ImGuizmo）需要处理，不发给CEF
                return 0;  // 事件已处理，不再传播
            }
            
            // 步骤2: ImGui不需要，发给CEF处理UI交互
            CefMouseEvent mouseEvent;
            mouseEvent.x = x;
            mouseEvent.y = y;
            
            // 设置modifiers以支持拖放操作
            mouseEvent.modifiers = 0;
            if (wParam & MK_CONTROL) mouseEvent.modifiers |= EVENTFLAG_CONTROL_DOWN;
            if (wParam & MK_SHIFT) mouseEvent.modifiers |= EVENTFLAG_SHIFT_DOWN;
            if (wParam & MK_LBUTTON) mouseEvent.modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
            if (wParam & MK_MBUTTON) mouseEvent.modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
            if (wParam & MK_RBUTTON) mouseEvent.modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
            
            auto browserHost = bridge->GetClient()->GetBrowser()->GetHost();
            
            switch (msg) {
            case WM_LBUTTONDOWN:
                browserHost->SendMouseClickEvent(mouseEvent, MBT_LEFT, false, 1);
                
                // 步骤3: 如果点击在viewport区域，执行3D对象拾取
                if (inViewport && g_EngineWndProcCallback) {
                    g_EngineWndProcCallback(msg, wParam, lParam);
                }
                break;
            case WM_LBUTTONUP:
                browserHost->SendMouseClickEvent(mouseEvent, MBT_LEFT, true, 1);
                break;
            case WM_RBUTTONDOWN:
                browserHost->SendMouseClickEvent(mouseEvent, MBT_RIGHT, false, 1);
                break;
            case WM_RBUTTONUP:
                browserHost->SendMouseClickEvent(mouseEvent, MBT_RIGHT, true, 1);
                break;
            case WM_MBUTTONDOWN:
                browserHost->SendMouseClickEvent(mouseEvent, MBT_MIDDLE, false, 1);
                break;
            case WM_MBUTTONUP:
                browserHost->SendMouseClickEvent(mouseEvent, MBT_MIDDLE, true, 1);
                break;
            case WM_MOUSEMOVE:
                browserHost->SendMouseMoveEvent(mouseEvent, false);
                break;
            case WM_MOUSEWHEEL:
            {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                browserHost->SendMouseWheelEvent(mouseEvent, 0, delta);
                break;
            }
            }
            
            return 0;
        }
        break;
    }

    // ============================================================================
    // 键盘事件转发给CEF
    // ============================================================================
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SYSCHAR:
    {
        if (bridge && bridge->GetClient() && bridge->GetClient()->GetBrowser()) {
            CefKeyEvent keyEvent;
            keyEvent.windows_key_code = static_cast<int>(wParam);
            keyEvent.native_key_code = static_cast<int>(lParam);
            keyEvent.is_system_key = (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP || msg == WM_SYSCHAR);
            
            if (msg == WM_CHAR || msg == WM_SYSCHAR) {
                keyEvent.type = KEYEVENT_CHAR;
            } else if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
                keyEvent.type = KEYEVENT_KEYDOWN;
            } else {
                keyEvent.type = KEYEVENT_KEYUP;
            }
            
            // 设置修饰键状态
            if (GetKeyState(VK_SHIFT) & 0x8000)
                keyEvent.modifiers |= EVENTFLAG_SHIFT_DOWN;
            if (GetKeyState(VK_CONTROL) & 0x8000)
                keyEvent.modifiers |= EVENTFLAG_CONTROL_DOWN;
            if (GetKeyState(VK_MENU) & 0x8000)
                keyEvent.modifiers |= EVENTFLAG_ALT_DOWN;
            
            bridge->GetClient()->GetBrowser()->GetHost()->SendKeyEvent(keyEvent);
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        if (bridge && bridge->GetClient() && bridge->GetClient()->GetBrowser()) {

            // 正常关闭 CEF 浏览器（触发 DoClose → PostQuitMessage）
            auto client = bridge->GetClient();
            client->DoClose(nullptr);
            client->GetBrowser()->GetHost()->CloseBrowser(false);

        }
        else {
            PostQuitMessage(0);
        }
        return 0;

    case WM_DESTROY:
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// EditorBridge
// ============================================================================
EditorBridge::EditorBridge() = default;

EditorBridge::~EditorBridge()
{
    Shutdown();
}

// ============================================================================
// 初始化 CEF
// ============================================================================
bool EditorBridge::Initialize(HINSTANCE hInstance)
{
    if (m_initialized) {
        std::cerr << "[EditorBridge] Already initialized" << std::endl;
        return false;
    }

    m_hInstance = hInstance;

    CefMainArgs args(hInstance);
    CefRefPtr<CefAppHandler> app(new CefAppHandler());

    CefSettings settings;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = false;
    settings.windowless_rendering_enabled = true;  // 启用 OSR 模式
    settings.remote_debugging_port = 9222;  // 启用远程调试（访问 http://localhost:9222）

    settings.log_severity = LOGSEVERITY_INFO;
    CefString(&settings.log_file).FromASCII("cef_debug.log");

    if (!CefInitialize(args, settings, app.get(), nullptr)) {
        std::cerr << "[EditorBridge] Failed to initialize CEF" << std::endl;
        return false;
    }

    m_initialized = true;
    std::cout << "[EditorBridge] CEF initialized successfully" << std::endl;
    return true;
}

// ============================================================================
// 创建主编辑器窗口
// ============================================================================
bool EditorBridge::CreateEditorWindow(const std::string& urlOverride)
{
    if (!m_initialized) {
        std::cerr << "[EditorBridge] Not initialized" << std::endl;
        return false;
    }

    // ------------------------------------
    // 注册窗口类
    // ------------------------------------
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MoonEditor_MainWindow";

    if (!RegisterClassExW(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            std::cerr << "[EditorBridge] Failed to register window class" << std::endl;
            return false;
        }
    }

    // ------------------------------------
    // 创建主窗口
    // ------------------------------------
    m_mainWindow = CreateWindowExW(
        0,
        L"MoonEditor_MainWindow",
        L"Moon Engine Editor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 720,
        nullptr,
        nullptr,
        m_hInstance,
        nullptr
    );

    if (!m_mainWindow) {
        std::cerr << "[EditorBridge] Failed to create main window" << std::endl;
        return false;
    }

    // 保存 this 指针
    SetWindowLongPtr(
        m_mainWindow,
        GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(this)
    );

    ShowWindow(m_mainWindow, SW_SHOW);
    UpdateWindow(m_mainWindow);

    // ------------------------------------
    // 生成 URL
    // ------------------------------------
    std::string exeDir = GetExecutableDir();
    std::string distIndex = exeDir + "\\dist\\index.html";

    std::string url = urlOverride.empty()
        ? ("file:///" + distIndex)
        : urlOverride;

    std::replace(url.begin(), url.end(), '\\', '/');
    std::cout << "[EditorBridge] Loading URL: " << url << std::endl;

    // ------------------------------------
    // 创建浏览器
    // ------------------------------------
    m_client = new CefClientHandler();

    CefWindowInfo winInfo;
    RECT rc;
    GetClientRect(m_mainWindow, &rc);

    // 使用 windowless 模式（OSR）
    winInfo.SetAsWindowless(m_mainWindow);

    CefBrowserSettings browserSettings;
    browserSettings.windowless_frame_rate = 60; // 60 FPS
    
    // 强制 device_scale_factor = 1.0，避免 DPI 缩放问题
    // CEF 默认会根据 Windows DPI 设置进行缩放，导致渲染内容偏小
    browserSettings.chrome_status_bubble = STATE_DISABLED;

    bool success = CefBrowserHost::CreateBrowser(
        winInfo,
        m_client,
        url,
        browserSettings,
        nullptr,
        nullptr
    );

    if (!success) {
        std::cerr << "[EditorBridge] Failed to create browser" << std::endl;
        return false;
    }

    return true;
}

// ============================================================================
// CEF 事件循环
// ============================================================================
void EditorBridge::DoMessageLoopWork()
{
    if (m_initialized)
        CefDoMessageLoopWork();
}

// ============================================================================
// CEF 是否正在关闭
// ============================================================================
bool EditorBridge::IsClosing() const
{
    return m_client && m_client->IsClosing();
}

// ============================================================================
// Shutdown
// ============================================================================
void EditorBridge::Shutdown()
{
    if (!m_initialized)
        return;

    std::cout << "[EditorBridge] Shutting down..." << std::endl;

    // 关闭浏览器
    if (m_client) {
        m_client->CloseAllBrowsers(false);
        m_client = nullptr;
    }

    // 销毁主窗口
    if (m_mainWindow) {
        DestroyWindow(m_mainWindow);
        m_mainWindow = nullptr;
    }

    // 关闭 CEF
    CefShutdown();
    m_initialized = false;

    std::cout << "[EditorBridge] Shutdown complete" << std::endl;
}
