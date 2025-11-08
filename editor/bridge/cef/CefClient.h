#pragma once
#include "include/cef_client.h"
#include "include/cef_life_span_handler.h"
#include "include/cef_load_handler.h"
#include "include/wrapper/cef_helpers.h"
#include <list>

// CEF Client handler
// 处理浏览器事件、生命周期、加载等
class CefClientHandler : public CefClient,
                         public CefLifeSpanHandler,
                         public CefLoadHandler {
public:
    CefClientHandler() = default;

    // CefClient methods
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }

    virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override {
        return this;
    }

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

private:
    CefRefPtr<CefBrowser> m_browser;
    bool m_isClosing = false;

    IMPLEMENT_REFCOUNTING(CefClientHandler);
};
