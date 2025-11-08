#include "CefClient.h"
#include "include/wrapper/cef_helpers.h"
#include "include/cef_task.h"
#include <iostream>

void CefClientHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    if (!m_browser) {
        // 保存第一个浏览器实例
        m_browser = browser;
        std::cout << "[CEF] Browser created successfully" << std::endl;
    }
}

bool CefClientHandler::DoClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    // 允许关闭
    m_isClosing = true;
    return false;
}

void CefClientHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    if (m_browser && m_browser->IsSame(browser)) {
        m_browser = nullptr;
        std::cout << "[CEF] Browser closed" << std::endl;
    }
}

void CefClientHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  int httpStatusCode) {
    CEF_REQUIRE_UI_THREAD();

    if (frame->IsMain()) {
        std::cout << "[CEF] Page loaded: " << frame->GetURL().ToString() 
                  << " (Status: " << httpStatusCode << ")" << std::endl;
    }
}

void CefClientHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    ErrorCode errorCode,
                                    const CefString& errorText,
                                    const CefString& failedUrl) {
    CEF_REQUIRE_UI_THREAD();

    // 忽略取消的请求
    if (errorCode == ERR_ABORTED)
        return;

    std::cerr << "[CEF] Failed to load URL: " << failedUrl.ToString() 
              << "\nError: " << errorText.ToString() 
              << " (" << errorCode << ")" << std::endl;
}

void CefClientHandler::CloseAllBrowsers(bool force_close) {
    if (!CefCurrentlyOn(TID_UI)) {
        // 需要在 UI 线程上执行，但这里简化处理
        // 在实际应用中，应该确保在正确的线程上调用
        return;
    }

    if (m_browser) {
        m_browser->GetHost()->CloseBrowser(force_close);
    }
}
