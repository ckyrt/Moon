#include "EditorBridge.h"
#include "cef/CefApp.h"
#include "include/cef_browser.h"
#include <iostream>
#include <filesystem>

EditorBridge::EditorBridge() {
}

EditorBridge::~EditorBridge() {
    Shutdown();
}

bool EditorBridge::Initialize(HINSTANCE hInstance) {
    if (m_initialized) {
        std::cerr << "[EditorBridge] Already initialized" << std::endl;
        return false;
    }

    m_hInstance = hInstance;

    // CEF 设置
    CefMainArgs main_args(hInstance);
    
    // 创建 CEF App
    CefRefPtr<CefAppHandler> app(new CefAppHandler());

    // CEF 设置参数
    CefSettings settings;
    settings.no_sandbox = true;  // 简化开发，生产环境建议启用沙箱
    settings.multi_threaded_message_loop = false;  // 使用单线程消息循环
    settings.windowless_rendering_enabled = false; // 使用窗口渲染
    
    // 日志设置
    settings.log_severity = LOGSEVERITY_INFO;
    CefString(&settings.log_file).FromASCII("cef_debug.log");

    // 初始化 CEF
    bool success = CefInitialize(main_args, settings, app.get(), nullptr);
    if (!success) {
        std::cerr << "[EditorBridge] Failed to initialize CEF" << std::endl;
        return false;
    }

    m_initialized = true;
    std::cout << "[EditorBridge] CEF initialized successfully" << std::endl;
    return true;
}

std::string GetExecutableDir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::filesystem::path exePath(buffer);
    return exePath.parent_path().string();
}

// 主窗口过程
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // 获取 EditorBridge 指针
    EditorBridge* bridge = reinterpret_cast<EditorBridge*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    switch (msg) {
    case WM_SIZE:
        if (bridge && bridge->GetClient() && bridge->GetClient()->GetBrowser()) {
            // 调整 CEF 浏览器窗口大小以填充整个客户区
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            HWND browserHwnd = bridge->GetClient()->GetBrowser()->GetHost()->GetWindowHandle();
            if (browserHwnd) {
                SetWindowPos(browserHwnd, nullptr, 
                           0, 0, 
                           rect.right - rect.left, 
                           rect.bottom - rect.top,
                           SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        return 0;
        
    case WM_CLOSE:
        // 用户点击关闭按钮
        if (bridge && bridge->GetClient() && bridge->GetClient()->GetBrowser()) {
            // ✅ 先关闭 CEF 浏览器（会触发 DoClose → PostQuitMessage）
            bridge->GetClient()->DoClose(nullptr);
            bridge->GetClient()->GetBrowser()->GetHost()->CloseBrowser(false);
        } else {
            // 如果没有浏览器，直接退出
            PostQuitMessage(0);
        }
        return 0;
        
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool EditorBridge::CreateEditorWindow(const std::string& urlOverride) {
    if (!m_initialized) {
        std::cerr << "[EditorBridge] Not initialized" << std::endl;
        return false;
    }

    // ✅ 创建主窗口（作为 CEF 的父窗口）
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MoonEditor_MainWindow";

    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            std::cerr << "[EditorBridge] Failed to register window class" << std::endl;
            return false;
        }
    }

    // 创建主窗口
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

    // ✅ 保存 this 指针到窗口，以便窗口过程可以访问
    SetWindowLongPtr(m_mainWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    ShowWindow(m_mainWindow, SW_SHOW);
    UpdateWindow(m_mainWindow);

    // 🔍 获取 EXE 所在目录
    std::string exeDir = GetExecutableDir();
    std::string distIndex = exeDir + "\\dist\\index.html";

    // ✅ 自动选择 URL：有 override 用 override，否则用自动路径
    std::string url = urlOverride.empty()
        ? ("file:///" + distIndex)
        : urlOverride;

    // 替换反斜杠为斜杠（CEF / Chrome 需要）
    std::replace(url.begin(), url.end(), '\\', '/');

    std::cout << "[EditorBridge] Loading URL: " << url << std::endl;

    // 创建客户端处理器
    m_client = new CefClientHandler();

    // ✅ 使用 SetAsChild 模式（而不是 SetAsPopup）
    // 这样我们才能收到 DoClose 回调
    CefWindowInfo window_info;
    RECT rect;
    GetClientRect(m_mainWindow, &rect);
    CefRect cefRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    window_info.SetAsChild(m_mainWindow, cefRect);

    CefBrowserSettings browser_settings;

    bool success = CefBrowserHost::CreateBrowser(
        window_info,
        m_client,
        url,
        browser_settings,
        nullptr,
        nullptr
    );

    if (!success) {
        std::cerr << "[EditorBridge] Failed to create browser" << std::endl;
        return false;
    }

    return true;
}

void EditorBridge::DoMessageLoopWork() {
    if (m_initialized) {
        CefDoMessageLoopWork();
    }
}

bool EditorBridge::IsClosing() const {
    return m_client && m_client->IsClosing();
}

void EditorBridge::Shutdown() {
    if (!m_initialized) {
        return;
    }

    std::cout << "[EditorBridge] Shutting down..." << std::endl;

    // 关闭所有浏览器
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
