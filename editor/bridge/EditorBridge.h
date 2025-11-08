#pragma once
#include "include/cef_app.h"
#include "cef/CefClient.h"
#include <string>

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

    // 关闭编辑器
    void Shutdown();

private:
    CefRefPtr<CefClientHandler> m_client;
    bool m_initialized = false;
};
