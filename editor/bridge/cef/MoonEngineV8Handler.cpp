#include "MoonEngineV8Handler.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_helpers.h"
#include "../../../engine/core/Logging/Logger.h"
#include "../../../external/nlohmann/json.hpp"

using json = nlohmann::json;

// 静态成员初始化
Moon::Scene* MoonEngineV8Handler::s_scene = nullptr;

MoonEngineV8Handler::MoonEngineV8Handler() {
}

void MoonEngineV8Handler::SetScene(Moon::Scene* scene) {
    s_scene = scene;
}

// ============================================================================
// Execute - 处理 JavaScript 函数调用
// ============================================================================
bool MoonEngineV8Handler::Execute(const CefString& name,
                                   CefRefPtr<CefV8Value> object,
                                   const CefV8ValueList& arguments,
                                   CefRefPtr<CefV8Value>& retval,
                                   CefString& exception) {
    
    // 确保在渲染进程中运行
    CEF_REQUIRE_RENDERER_THREAD();

    std::string functionName = name.ToString();

    // 这些函数只是包装器，实际通过 window.cefQuery 与浏览器进程通信
    // JavaScript 层会处理异步调用
    
    if (functionName == "getScene") {
        json req;
        req["command"] = "getScene";
        retval = CefV8Value::CreateString(req.dump());
        return true;
    }

    if (functionName == "selectNode") {
        if (arguments.size() < 1) {
            exception = "selectNode requires 1 argument: nodeId";
            return true;
        }
        json req;
        req["command"] = "selectNode";
        req["nodeId"] = arguments[0]->GetIntValue();
        retval = CefV8Value::CreateString(req.dump());
        return true;
    }

    if (functionName == "setPosition") {
        if (arguments.size() < 2) {
            exception = "setPosition requires 2 arguments: nodeId, position";
            return true;
        }
        
        CefRefPtr<CefV8Value> pos = arguments[1];
        if (!pos->IsObject()) {
            exception = "position must be an object with x, y, z properties";
            return true;
        }

        json req;
        req["command"] = "setPosition";
        req["nodeId"] = arguments[0]->GetIntValue();
        req["position"]["x"] = pos->GetValue("x")->GetDoubleValue();
        req["position"]["y"] = pos->GetValue("y")->GetDoubleValue();
        req["position"]["z"] = pos->GetValue("z")->GetDoubleValue();
        
        retval = CefV8Value::CreateString(req.dump());
        return true;
    }

    if (functionName == "setRotation") {
        if (arguments.size() < 2) {
            exception = "setRotation requires 2 arguments: nodeId, rotation";
            return true;
        }
        
        CefRefPtr<CefV8Value> rot = arguments[1];
        if (!rot->IsObject()) {
            exception = "rotation must be an object with x, y, z properties";
            return true;
        }

        json req;
        req["command"] = "setRotation";
        req["nodeId"] = arguments[0]->GetIntValue();
        req["rotation"]["x"] = rot->GetValue("x")->GetDoubleValue();
        req["rotation"]["y"] = rot->GetValue("y")->GetDoubleValue();
        req["rotation"]["z"] = rot->GetValue("z")->GetDoubleValue();
        
        retval = CefV8Value::CreateString(req.dump());
        return true;
    }

    if (functionName == "setScale") {
        if (arguments.size() < 2) {
            exception = "setScale requires 2 arguments: nodeId, scale";
            return true;
        }
        
        CefRefPtr<CefV8Value> scale = arguments[1];
        if (!scale->IsObject()) {
            exception = "scale must be an object with x, y, z properties";
            return true;
        }

        json req;
        req["command"] = "setScale";
        req["nodeId"] = arguments[0]->GetIntValue();
        req["scale"]["x"] = scale->GetValue("x")->GetDoubleValue();
        req["scale"]["y"] = scale->GetValue("y")->GetDoubleValue();
        req["scale"]["z"] = scale->GetValue("z")->GetDoubleValue();
        
        retval = CefV8Value::CreateString(req.dump());
        return true;
    }

    if (functionName == "getNodeDetails") {
        if (arguments.size() < 1) {
            exception = "getNodeDetails requires 1 argument: nodeId";
            return true;
        }
        json req;
        req["command"] = "getNodeDetails";
        req["nodeId"] = arguments[0]->GetIntValue();
        retval = CefV8Value::CreateString(req.dump());
        return true;
    }

    return false;
}

// ============================================================================
// 注册 window.moonEngine API
// ============================================================================
void RegisterMoonEngineAPI(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Context> context) {
    CEF_REQUIRE_RENDERER_THREAD();

    // 获取全局对象 (window)
    CefRefPtr<CefV8Value> global = context->GetGlobal();

    // 检查 cefQuery 是否存在
    if (!global->HasValue("cefQuery")) {
        MOON_LOG_WARN("MoonEngineV8", "cefQuery not found, MoonEngine API may not work properly");
    }

    // 注入 JavaScript 包装器代码
    // 这些函数会使用 cefQuery 与浏览器进程通信
    std::string jsCode = R"(
        (function() {
            // 创建 window.moonEngine 对象
            window.moonEngine = {
                // 异步调用辅助函数
                _call: function(request) {
                    return new Promise((resolve, reject) => {
                        if (!window.cefQuery) {
                            reject(new Error('cefQuery not available'));
                            return;
                        }
                        window.cefQuery({
                            request: request,
                            onSuccess: function(response) {
                                try {
                                    resolve(JSON.parse(response));
                                } catch(e) {
                                    resolve(response);
                                }
                            },
                            onFailure: function(error_code, error_message) {
                                reject(new Error(error_message));
                            }
                        });
                    });
                },

                // 获取场景数据
                getScene: function() {
                    return this._call(JSON.stringify({ command: 'getScene' }));
                },

                // 选中节点
                selectNode: function(nodeId) {
                    return this._call(JSON.stringify({ command: 'selectNode', nodeId: nodeId }));
                },

                // 设置位置
                setPosition: function(nodeId, position) {
                    return this._call(JSON.stringify({ 
                        command: 'setPosition', 
                        nodeId: nodeId, 
                        position: position 
                    }));
                },

                // 设置旋转
                setRotation: function(nodeId, rotation) {
                    return this._call(JSON.stringify({ 
                        command: 'setRotation', 
                        nodeId: nodeId, 
                        rotation: rotation 
                    }));
                },

                // 设置缩放
                setScale: function(nodeId, scale) {
                    return this._call(JSON.stringify({ 
                        command: 'setScale', 
                        nodeId: nodeId, 
                        scale: scale 
                    }));
                },

                // 获取节点详情
                getNodeDetails: function(nodeId) {
                    return this._call(JSON.stringify({ 
                        command: 'getNodeDetails', 
                        nodeId: nodeId 
                    }));
                }
            };
            
            console.log('[MoonEngine] window.moonEngine API registered');
        })();
    )";

    // 执行 JavaScript 代码
    CefRefPtr<CefV8Value> result;
    CefRefPtr<CefV8Exception> exception;
    if (!context->Eval(jsCode, "", 0, result, exception)) {
        if (exception) {
            MOON_LOG_ERROR("MoonEngineV8", "Failed to inject moonEngine API: %s",
                          exception->GetMessage().ToString().c_str());
        }
    } else {
        MOON_LOG_INFO("MoonEngineV8", "window.moonEngine API registered successfully");
    }
}
