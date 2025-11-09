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
    IMPLEMENT_REFCOUNTING(CefAppHandler);
};
