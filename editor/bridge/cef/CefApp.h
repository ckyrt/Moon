#pragma once
#include "include/cef_app.h"

// CEF Application handler
// 管理 CEF 的生命周期和进程通信
class CefAppHandler : public CefApp,
                      public CefBrowserProcessHandler {
public:
    CefAppHandler() = default;

    // CefApp methods
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }

private:
    IMPLEMENT_REFCOUNTING(CefAppHandler);
};
