#pragma once
#include "include/wrapper/cef_message_router.h"
#include "../../../engine/core/Scene/Scene.h"

// Forward declarations
class EngineCore;

/**
 * @brief Moon Engine Message Handler
 * 
 * 处理来自 JavaScript (渲染进程) 的消息请求，
 * 在浏览器进程中执行操作并返回结果。
 */
class MoonEngineMessageHandler : public CefMessageRouterBrowserSide::Handler {
public:
    MoonEngineMessageHandler();
    ~MoonEngineMessageHandler() override = default;

    /**
     * @brief 设置引擎核心指针
     */
    void SetEngineCore(EngineCore* engine);

    /**
     * @brief 处理来自渲染进程的查询
     */
    bool OnQuery(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int64_t query_id,
                 const CefString& request,
                 bool persistent,
                 CefRefPtr<Callback> callback) override;

    /**
     * @brief 取消查询
     */
    void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int64_t query_id) override;

private:
    /**
     * @brief 解析并处理请求
     */
    std::string ProcessRequest(const std::string& request);

    EngineCore* m_engine;
};
