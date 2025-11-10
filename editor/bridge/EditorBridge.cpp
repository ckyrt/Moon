// ============================================================================
// EditorBridge.cpp - CEF 编辑器桥接层 (Refined Version)
// ============================================================================
// ✅ 保持原始功能完全不变
// ✅ 清晰拆分：初始化 → 创建窗口 → 创建浏览器 → 消息循环 → Shutdown
// ✅ 仅进行结构与可读性优化，不改变逻辑
// ============================================================================

#include "EditorBridge.h"
#include "cef/CefApp.h"
#include "include/cef_browser.h"

#include <iostream>
#include <filesystem>
#include <algorithm>

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

    switch (msg) {

    case WM_SIZE:
        if (bridge && bridge->GetClient() && bridge->GetClient()->GetBrowser()) {

            RECT rc;
            GetClientRect(hwnd, &rc);

            HWND browser = bridge->GetClient()->GetBrowser()->GetHost()->GetWindowHandle();
            if (browser) {
                SetWindowPos(
                    browser, nullptr,
                    0, 0,
                    rc.right - rc.left,
                    rc.bottom - rc.top,
                    SWP_NOZORDER | SWP_NOACTIVATE
                );
            }
        }
        return 0;

    case WM_CLOSE:
        if (bridge && bridge->GetClient() && bridge->GetClient()->GetBrowser()) {

            // ✅ 正常关闭 CEF 浏览器（触发 DoClose → PostQuitMessage）
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
    settings.windowless_rendering_enabled = false;

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

    winInfo.SetAsChild(
        m_mainWindow,
        CefRect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top)
    );

    CefBrowserSettings browserSettings;

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
