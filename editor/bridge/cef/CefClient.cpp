// ============================================================================
// CefClient.cpp - CEF 客户端处理器 (Refined Version)
// ============================================================================
// ✅ 保留全部逻辑与功能
// ✅ 结构清晰：MessageHandler → ClientHandler → Load Events → Router
// ============================================================================

#include "CefClient.h"

#include "include/wrapper/cef_helpers.h"
#include "include/cef_task.h"
#include "include/cef_browser.h"

#include <iostream>
#include <algorithm>

// ✅ Moon Logger
#include "../../engine/core/Logging/Logger.h"

// ✅ JSON 解析器
#include "../../../external/nlohmann/json.hpp"
using json = nlohmann::json;

// ✅ Moon Engine Message Handler
#include "MoonEngineMessageHandler.h"

// ============================================================================
// MessageHandler - Browser Side Message Router Handler
// ============================================================================
class MessageHandler : public CefMessageRouterBrowserSide::Handler
{
public:
    explicit MessageHandler(CefClientHandler* client)
        : m_client(client) {
    }

    bool OnQuery(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        int64_t query_id,
        const CefString& request,
        bool persistent,
        CefRefPtr<Callback> callback) override
    {
        std::string requestStr = request.ToString();

        try {
            json j = json::parse(requestStr);
            
            // 只处理有 "type" 字段的消息（viewport-rect 等）
            // 有 "command" 字段的消息由 MoonEngineMessageHandler 处理
            if (!j.contains("type")) {
                return false;  // 不是我们处理的消息，让下一个 handler 处理
            }

            std::string type = j.value("type", "");
            MOON_LOG_INFO("CEF_MessageHandler", "Received query with type: %s", type.c_str());

            // ------------------------------------------------------------
            // ✅ viewport-rect 事件（JavaScript → C++）
            // ------------------------------------------------------------
            if (type == "viewport-rect") {

                int x = j.value("x", 0);
                int y = j.value("y", 0);
                int width = j.value("width", 0);
                int height = j.value("height", 0);

                MOON_LOG_INFO("CEF", "========================================");
                MOON_LOG_INFO("CEF", "Viewport rect received from JavaScript:");
                MOON_LOG_INFO("CEF", "  Position: (%d, %d)", x, y);
                MOON_LOG_INFO("CEF", "  Size:     %dx%d", width, height);
                MOON_LOG_INFO("CEF", "========================================");

                if (m_client && m_client->m_viewportRectCallback) {
                    m_client->m_viewportRectCallback(x, y, width, height);
                }
                else {
                    MOON_LOG_WARN("CEF", "No viewportRectCallback registered!");
                }

                callback->Success("");
                return true;
            }

            MOON_LOG_WARN("CEF_MessageHandler",
                "Unknown message type: %s", type.c_str());
        }
        catch (const json::exception& e) {
            MOON_LOG_ERROR("CEF_MessageHandler",
                "JSON parse error: %s", e.what());
            return false;  // 让下一个 handler 尝试
        }

        return false;
    }

private:
    CefClientHandler* m_client;
};

// ============================================================================
// CefClientHandler - Browser Lifecycle + Router
// ============================================================================
CefClientHandler::CefClientHandler()
    : m_moonEngineHandler(nullptr)
{
    // 创建 Message Router
    CefMessageRouterConfig config;
    m_messageRouter = CefMessageRouterBrowserSide::Create(config);

    // 添加处理器
    m_messageRouter->AddHandler(new MessageHandler(this), false);

    // 添加 Moon Engine 处理器
    m_moonEngineHandler = new MoonEngineMessageHandler();
    m_messageRouter->AddHandler(m_moonEngineHandler, false);

    MOON_LOG_INFO("CEF", "CefClientHandler created with message router and MoonEngine handler");
}

void CefClientHandler::SetEngineCore(EngineCore* engine) {
    if (m_moonEngineHandler) {
        m_moonEngineHandler->SetEngineCore(engine);
        MOON_LOG_INFO("CEF", "EngineCore set for MoonEngineMessageHandler");
    }
}

// ============================================================================
// Browser Created
// ============================================================================
void CefClientHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
    CEF_REQUIRE_UI_THREAD();

    if (!m_browser) {
        m_browser = browser;
        MOON_LOG_INFO("CEF", "Browser created successfully");
    }
}

// ============================================================================
// Window Close → Trigger WM_QUIT
// ============================================================================
bool CefClientHandler::DoClose(CefRefPtr<CefBrowser> browser)
{
    CEF_REQUIRE_UI_THREAD();

    MOON_LOG_INFO("CEF", "DoClose CALLED! Posting WM_QUIT");

    PostQuitMessage(0);
    m_isClosing = true;

    return false;  // Allow browser to close
}

// ============================================================================
// Browser Closed
// ============================================================================
void CefClientHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
    CEF_REQUIRE_UI_THREAD();

    if (m_browser && m_browser->IsSame(browser)) {
        m_browser = nullptr;
        MOON_LOG_INFO("CEF", "Browser closed");
    }
}

// ============================================================================
// Load Completed
// ============================================================================
void CefClientHandler::OnLoadEnd(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    int httpStatusCode)
{
    CEF_REQUIRE_UI_THREAD();

    if (frame->IsMain()) {
        auto url = frame->GetURL().ToString();

        MOON_LOG_INFO("CEF", "========================================");
        MOON_LOG_INFO("CEF", "Page loaded!");
        MOON_LOG_INFO("CEF", "  URL: %s", url.c_str());
        MOON_LOG_INFO("CEF", "  Status: %d", httpStatusCode);
        MOON_LOG_INFO("CEF", "Waiting for viewport-rect messages...");
        MOON_LOG_INFO("CEF", "========================================");
    }
}

// ============================================================================
// Load Error
// ============================================================================
void CefClientHandler::OnLoadError(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    ErrorCode errorCode,
    const CefString& errorText,
    const CefString& failedUrl)
{
    CEF_REQUIRE_UI_THREAD();

    if (errorCode == ERR_ABORTED)
        return;

    MOON_LOG_ERROR("CEF", "Failed to load URL: %s",
        failedUrl.ToString().c_str());
    MOON_LOG_ERROR("CEF", "  Error: %s (%d)",
        errorText.ToString().c_str(), errorCode);
}

// ============================================================================
// Close All Browsers
// ============================================================================
void CefClientHandler::CloseAllBrowsers(bool force_close)
{
    if (!CefCurrentlyOn(TID_UI))
        return; // 简化处理：生产环境应切换线程

    if (m_browser)
        m_browser->GetHost()->CloseBrowser(force_close);
}

// ============================================================================
// Browser → Renderer process messages
// ============================================================================
bool CefClientHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message)
{
    CEF_REQUIRE_UI_THREAD();

    std::string msg = message->GetName().ToString();
    MOON_LOG_INFO("CEF", "OnProcessMessageReceived: %s", msg.c_str());

    if (m_messageRouter->OnProcessMessageReceived(
        browser, frame, source_process, message)) {

        MOON_LOG_INFO("CEF", "Message handled by router");
        return true;
    }

    MOON_LOG_WARN("CEF", "Message NOT handled by router");
    return false;
}
