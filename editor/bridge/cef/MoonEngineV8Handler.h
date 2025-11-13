#pragma once
#include "include/cef_v8.h"
#include <string>

// Forward declarations
namespace Moon {
    class Scene;
}

/**
 * @brief Moon Engine JavaScript API Handler
 * 
 * 在 JavaScript 中暴露 window.moonEngine 对象，
 * 提供场景查询、节点操作等功能。
 */
class MoonEngineV8Handler : public CefV8Handler {
public:
    MoonEngineV8Handler();
    ~MoonEngineV8Handler() override = default;

    /**
     * @brief 执行 JavaScript 调用的函数
     */
    bool Execute(const CefString& name,
                 CefRefPtr<CefV8Value> object,
                 const CefV8ValueList& arguments,
                 CefRefPtr<CefV8Value>& retval,
                 CefString& exception) override;

    /**
     * @brief 设置场景指针（从浏览器进程传递）
     */
    static void SetScene(Moon::Scene* scene);

private:
    /**
     * @brief 处理 getScene() 调用
     */
    bool HandleGetScene(CefRefPtr<CefV8Value>& retval, CefString& exception);

    /**
     * @brief 处理 selectNode(nodeId) 调用
     */
    bool HandleSelectNode(const CefV8ValueList& arguments,
                          CefRefPtr<CefV8Value>& retval,
                          CefString& exception);

    /**
     * @brief 处理 setPosition(nodeId, position) 调用
     */
    bool HandleSetPosition(const CefV8ValueList& arguments,
                           CefRefPtr<CefV8Value>& retval,
                           CefString& exception);

    /**
     * @brief 处理 setRotation(nodeId, rotation) 调用
     */
    bool HandleSetRotation(const CefV8ValueList& arguments,
                           CefRefPtr<CefV8Value>& retval,
                           CefString& exception);

    /**
     * @brief 处理 setScale(nodeId, scale) 调用
     */
    bool HandleSetScale(const CefV8ValueList& arguments,
                        CefRefPtr<CefV8Value>& retval,
                        CefString& exception);

    /**
     * @brief 处理 getNodeDetails(nodeId) 调用
     */
    bool HandleGetNodeDetails(const CefV8ValueList& arguments,
                              CefRefPtr<CefV8Value>& retval,
                              CefString& exception);

    IMPLEMENT_REFCOUNTING(MoonEngineV8Handler);

    static Moon::Scene* s_scene;  // 场景指针（注意：跨进程需要特殊处理）
};

/**
 * @brief 注册 window.moonEngine 对象到 JavaScript 上下文
 */
void RegisterMoonEngineAPI(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Context> context);
