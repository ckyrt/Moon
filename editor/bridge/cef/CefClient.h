#pragma once
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"
#include <list>
#include <functional>

// 回调函数类型：接收 viewport 矩形信息
using ViewportRectCallback = std::function<void(int x, int y, int width, int height)>;

// 前向声明
class MessageHandler;
class MoonEngineMessageHandler;
class EngineCore;

// CEF Client handler
// 处理浏览器事件、生命周期、加载等
class CefClientHandler : public CefClient,
                         public CefLifeSpanHandler,
                         public CefLoadHandler {
public:
    CefClientHandler();
    
    // ✅ 友元声明，让 MessageHandler 可以访问私有成员
    friend class MessageHandler;

    // CefClient methods
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }

    virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override {
        return this;
    }

    // ✅ 处理 JavaScript 消息
    virtual bool OnProcessMessageReceived(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefProcessId source_process,
        CefRefPtr<CefProcessMessage> message) override;

    // CefLifeSpanHandler methods
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    bool DoClose(CefRefPtr<CefBrowser> browser) override;
    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefLoadHandler methods
    void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int httpStatusCode) override;

    void OnLoadError(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     ErrorCode errorCode,
                     const CefString& errorText,
                     const CefString& failedUrl) override;

    // 获取浏览器实例
    CefRefPtr<CefBrowser> GetBrowser() const { return m_browser; }

    // 请求关闭
    void CloseAllBrowsers(bool force_close);

    bool IsClosing() const { return m_isClosing; }

    // ✅ 设置 viewport 矩形回调
    void SetViewportRectCallback(ViewportRectCallback callback) {
        m_viewportRectCallback = callback;
    }

    // ✅ 设置引擎核心指针（用于 MoonEngine API）
    void SetEngineCore(EngineCore* engine);

private:
    CefRefPtr<CefBrowser> m_browser;
    bool m_isClosing = false;
    CefRefPtr<CefMessageRouterBrowserSide> m_messageRouter;
    ViewportRectCallback m_viewportRectCallback;
    MoonEngineMessageHandler* m_moonEngineHandler;

    IMPLEMENT_REFCOUNTING(CefClientHandler);
};
