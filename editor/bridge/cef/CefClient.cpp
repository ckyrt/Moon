#include "CefClient.h"
#include "include/wrapper/cef_helpers.h"
#include "include/cef_task.h"
#include <iostream>

// ✅ 引入 Moon Logger
#include "../../engine/core/Logging/Logger.h"

// ✅ 使用 nlohmann/json 单头文件库
#include "../../../external/nlohmann/json.hpp"
using json = nlohmann::json;

// ✅ Message Router Handler
class MessageHandler : public CefMessageRouterBrowserSide::Handler {
public:
    explicit MessageHandler(CefClientHandler* client) : m_client(client) {}

    bool OnQuery(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int64_t query_id,
                 const CefString& request,
                 bool persistent,
                 CefRefPtr<Callback> callback) override {
        
        std::string requestStr = request.ToString();
        
        MOON_LOG_INFO("CEF_MessageHandler", "Received query: %s", requestStr.c_str());
        
        try {
            // ✅ 使用 nlohmann/json 解析
            json j = json::parse(requestStr);
            
            std::string type = j.value("type", "");
            MOON_LOG_INFO("CEF_MessageHandler", "Parsed type: %s", type.c_str());
            
            // 处理 viewport-rect 消息
            if (type == "viewport-rect") {
                int x = j.value("x", 0);
                int y = j.value("y", 0);
                int width = j.value("width", 0);
                int height = j.value("height", 0);

                MOON_LOG_INFO("CEF", "========================================");
                MOON_LOG_INFO("CEF", "Viewport rect received from JavaScript!");
                MOON_LOG_INFO("CEF", "  Position: (%d, %d)", x, y);
                MOON_LOG_INFO("CEF", "  Size: %dx%d", width, height);
                MOON_LOG_INFO("CEF", "========================================");

                // 调用回调
                if (m_client && m_client->m_viewportRectCallback) {
                    MOON_LOG_INFO("CEF", "Calling viewport rect callback...");
                    m_client->m_viewportRectCallback(x, y, width, height);
                    MOON_LOG_INFO("CEF", "Callback completed!");
                } else {
                    MOON_LOG_WARN("CEF", "WARNING: No callback registered!");
                }

                callback->Success("");
                return true;
            }

            MOON_LOG_WARN("CEF_MessageHandler", "Unknown message type: %s", type.c_str());
            
        } catch (const json::exception& e) {
            MOON_LOG_ERROR("CEF_MessageHandler", "JSON parse error: %s", e.what());
            callback->Failure(1, "Invalid JSON");
            return true;
        }

        return false;
    }

private:
    CefClientHandler* m_client;
};

CefClientHandler::CefClientHandler() {
    // 创建 Message Router
    CefMessageRouterConfig config;
    m_messageRouter = CefMessageRouterBrowserSide::Create(config);
    m_messageRouter->AddHandler(new MessageHandler(this), false);
    
    MOON_LOG_INFO("CEF", "CefClientHandler created with message router");
}

void CefClientHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    CEF_REQUIRE_UI_THREAD();

    if (!m_browser) {
        // 保存第一个浏览器实例
        m_browser = browser;
        MOON_LOG_INFO("CEF", "Browser created successfully");
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
        MOON_LOG_INFO("CEF", "Browser closed");
    }
}

void CefClientHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  int httpStatusCode) {
    CEF_REQUIRE_UI_THREAD();

    if (frame->IsMain()) {
        std::string url = frame->GetURL().ToString();
        MOON_LOG_INFO("CEF", "========================================");
        MOON_LOG_INFO("CEF", "Page loaded successfully!");
        MOON_LOG_INFO("CEF", "  URL: %s", url.c_str());
        MOON_LOG_INFO("CEF", "  Status: %d", httpStatusCode);
        MOON_LOG_INFO("CEF", "  Waiting for JavaScript viewport-rect message...");
        MOON_LOG_INFO("CEF", "========================================");
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

    std::string url = failedUrl.ToString();
    std::string error = errorText.ToString();
    MOON_LOG_ERROR("CEF", "Failed to load URL: %s", url.c_str());
    MOON_LOG_ERROR("CEF", "  Error: %s (%d)", error.c_str(), errorCode);
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

bool CefClientHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
    
    CEF_REQUIRE_UI_THREAD();
    
    std::string msgName = message->GetName().ToString();
    MOON_LOG_INFO("CEF", "OnProcessMessageReceived called");
    MOON_LOG_INFO("CEF", "  Message name: %s", msgName.c_str());
    
    // 让 Message Router 处理消息
    if (m_messageRouter->OnProcessMessageReceived(browser, frame, source_process, message)) {
        MOON_LOG_INFO("CEF", "Message handled by router");
        return true;
    }
    
    MOON_LOG_WARN("CEF", "Message NOT handled by router");
    return false;
}
