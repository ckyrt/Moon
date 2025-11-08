#include "EditorBridge.h"
#include "cef/CefApp.h"
#include "include/cef_browser.h"
#include <iostream>

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

bool EditorBridge::CreateEditorWindow(const std::string& url) {
    if (!m_initialized) {
        std::cerr << "[EditorBridge] Not initialized" << std::endl;
        return false;
    }

    // 创建客户端处理器
    m_client = new CefClientHandler();

    // 窗口信息
    CefWindowInfo window_info;
    window_info.SetAsPopup(nullptr, "Moon Engine Editor");
    // 设置窗口初始大小
    window_info.bounds.x = 100;
    window_info.bounds.y = 100;
    window_info.bounds.width = 1600;
    window_info.bounds.height = 900;

    // 浏览器设置
    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60; // 60 FPS

    // 创建浏览器
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

    std::cout << "[EditorBridge] Browser window created" << std::endl;
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

    // 关闭 CEF
    CefShutdown();
    m_initialized = false;

    std::cout << "[EditorBridge] Shutdown complete" << std::endl;
}
