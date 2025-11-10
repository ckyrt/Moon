#pragma once
#include "include/cef_app.h"
#include "include/wrapper/cef_message_router.h"

// CEF Application handler
// 管理 CEF 的生命周期和进程通信
class CefAppHandler : public CefApp,
                      public CefBrowserProcessHandler,
                      public CefRenderProcessHandler {
public:
    CefAppHandler() = default;

    // CefApp methods
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }

    // ✅ 添加 RenderProcessHandler，用于在渲染进程中注入 cefQuery
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
        return this;
    }

    // CefRenderProcessHandler methods
    void OnContextCreated(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override {
        // 在渲染进程中创建 Message Router
        CefMessageRouterConfig config;
        CefRefPtr<CefMessageRouterRendererSide> router =
            CefMessageRouterRendererSide::Create(config);
        router->OnContextCreated(browser, frame, context);
        m_messageRouters.push_back(router);
    }

    void OnContextReleased(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefV8Context> context) override {
        // 清理 Message Router
        for (auto& router : m_messageRouters) {
            router->OnContextReleased(browser, frame, context);
        }
    }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefProcessId source_process,
                                  CefRefPtr<CefProcessMessage> message) override {
        // 让 Message Router 处理消息
        for (auto& router : m_messageRouters) {
            if (router->OnProcessMessageReceived(browser, frame, source_process, message)) {
                return true;
            }
        }
        return false;
    }

    void OnBeforeCommandLineProcessing(
        const CefString& process_type,
        CefRefPtr<CefCommandLine> command_line) override
    {
        command_line->AppendSwitch("disable-web-security");
        command_line->AppendSwitch("allow-file-access-from-files");
        command_line->AppendSwitch("allow-universal-access-from-files");
        command_line->AppendSwitch("allow-file-access");
    }


private:
    // 渲染进程的 Message Router 列表
    std::vector<CefRefPtr<CefMessageRouterRendererSide>> m_messageRouters;

    IMPLEMENT_REFCOUNTING(CefAppHandler);
};
