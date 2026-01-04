#pragma once
#include "include/cef_app.h"
#include "cef/CefClient.h"
#include <string>
#include <functional>
#include <Windows.h>

// CEF主窗口尺寸变化回调
void SetCefWindowResizeCallback(std::function<void(int, int)> callback);

// 设置Viewport区域信息（用于判断鼠标事件是否在3D区域）
void SetViewportInfo(int x, int y, int width, int height);

// 设置3D场景交互回调（仅用于picking等，在viewport区域LBUTTONDOWN时调用）
void SetEngineWndProcCallback(std::function<void(UINT, WPARAM, LPARAM)> callback);

// 设置ImGui的WantCaptureMouse检查回调（判断ImGui是否需要鼠标事件）
void SetImGuiWantsCaptureCallback(std::function<bool()> callback);

// 设置ImGui的Win32事件处理回调（在MainWindowProc中优先调用）
void SetImGuiWin32ProcCallback(std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> callback);

// 编辑器桥接类
// 负责初始化 CEF，创建浏览器窗口，管理生命周期
class EditorBridge {
public:
    EditorBridge();
    ~EditorBridge();

    // 初始化 CEF
    bool Initialize(HINSTANCE hInstance);

    // 创建编辑器窗口
    bool CreateEditorWindow(const std::string& url);

    // 运行消息循环（非阻塞）
    void DoMessageLoopWork();

    // 检查是否正在关闭
    bool IsClosing() const;

    // 获取客户端处理器（用于获取浏览器窗口句柄）
    CefRefPtr<CefClientHandler> GetClient() const { return m_client; }

    // 获取主窗口句柄
    HWND GetMainWindow() const { return m_mainWindow; }

    // 关闭编辑器
    void Shutdown();

private:
    CefRefPtr<CefClientHandler> m_client;
    bool m_initialized = false;
    HWND m_mainWindow = nullptr;
    HINSTANCE m_hInstance = nullptr;
};
