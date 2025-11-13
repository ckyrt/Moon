#include "MoonEngineMessageHandler.h"
#include "../SceneSerializer.h"
#include "../../../engine/core/EngineCore.h"
#include "../../../engine/core/Logging/Logger.h"
#include "../../../external/nlohmann/json.hpp"

using json = nlohmann::json;

// 外部函数声明（定义在 EditorApp.cpp 中）
extern void SetSelectedObject(Moon::SceneNode* node);
extern Moon::SceneNode* GetSelectedObject();

MoonEngineMessageHandler::MoonEngineMessageHandler()
    : m_engine(nullptr) {
}

void MoonEngineMessageHandler::SetEngineCore(EngineCore* engine) {
    m_engine = engine;
}

// ============================================================================
// 处理来自 JavaScript 的查询
// ============================================================================
bool MoonEngineMessageHandler::OnQuery(CefRefPtr<CefBrowser> browser,
                                        CefRefPtr<CefFrame> frame,
                                        int64_t query_id,
                                        const CefString& request,
                                        bool persistent,
                                        CefRefPtr<Callback> callback) {
    
    std::string requestStr = request.ToString();
    
    MOON_LOG_INFO("MoonEngineMessage", "OnQuery called with request: %s", requestStr.c_str());
    
    // 处理请求并获取响应
    std::string response = ProcessRequest(requestStr);
    
    MOON_LOG_INFO("MoonEngineMessage", "Response: %s", response.c_str());
    
    // 返回响应给 JavaScript
    callback->Success(response);
    
    return true;
}

void MoonEngineMessageHandler::OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                                                CefRefPtr<CefFrame> frame,
                                                int64_t query_id) {
    // 查询被取消，无需处理
}

// ============================================================================
// 处理具体请求
// ============================================================================
std::string MoonEngineMessageHandler::ProcessRequest(const std::string& request) {
    if (!m_engine) {
        json error;
        error["error"] = "Engine not initialized";
        return error.dump();
    }

    try {
        // 解析 JSON 请求
        json req = json::parse(request);
        std::string command = req["command"];

        Moon::Scene* scene = m_engine->GetScene();

        // ===== getScene =====
        if (command == "getScene") {
            return SceneSerializer::GetSceneHierarchy(scene);
        }

        // ===== getNodeDetails =====
        if (command == "getNodeDetails") {
            uint32_t nodeId = req["nodeId"];
            return SceneSerializer::GetNodeDetails(scene, nodeId);
        }

        // ===== selectNode =====
        if (command == "selectNode") {
            uint32_t nodeId = req["nodeId"];
            Moon::SceneNode* node = scene->FindNodeByID(nodeId);
            if (node) {
                MOON_LOG_INFO("MoonEngineMessage", "Selected node: %s (ID=%u)", 
                             node->GetName().c_str(), nodeId);
                
                // 更新全局选中状态（这样 Gizmo 会显示在这个物体上）
                SetSelectedObject(node);
                
                json result;
                result["success"] = true;
                return result.dump();
            } else {
                json error;
                error["error"] = "Node not found";
                return error.dump();
            }
        }

        // ===== setPosition =====
        if (command == "setPosition") {
            uint32_t nodeId = req["nodeId"];
            float x = req["position"]["x"];
            float y = req["position"]["y"];
            float z = req["position"]["z"];

            Moon::SceneNode* node = scene->FindNodeByID(nodeId);
            if (node) {
                node->GetTransform()->SetLocalPosition({x, y, z});
                MOON_LOG_INFO("MoonEngineMessage", "Set position of node %u to (%.2f, %.2f, %.2f)",
                             nodeId, x, y, z);
                json result;
                result["success"] = true;
                return result.dump();
            } else {
                json error;
                error["error"] = "Node not found";
                return error.dump();
            }
        }

        // ===== setRotation =====
        if (command == "setRotation") {
            uint32_t nodeId = req["nodeId"];
            float x = req["rotation"]["x"];
            float y = req["rotation"]["y"];
            float z = req["rotation"]["z"];

            Moon::SceneNode* node = scene->FindNodeByID(nodeId);
            if (node) {
                node->GetTransform()->SetLocalRotation({x, y, z});
                MOON_LOG_INFO("MoonEngineMessage", "Set rotation of node %u to (%.2f, %.2f, %.2f)",
                             nodeId, x, y, z);
                json result;
                result["success"] = true;
                return result.dump();
            } else {
                json error;
                error["error"] = "Node not found";
                return error.dump();
            }
        }

        // ===== setScale =====
        if (command == "setScale") {
            uint32_t nodeId = req["nodeId"];
            float x = req["scale"]["x"];
            float y = req["scale"]["y"];
            float z = req["scale"]["z"];

            Moon::SceneNode* node = scene->FindNodeByID(nodeId);
            if (node) {
                node->GetTransform()->SetLocalScale({x, y, z});
                MOON_LOG_INFO("MoonEngineMessage", "Set scale of node %u to (%.2f, %.2f, %.2f)",
                             nodeId, x, y, z);
                json result;
                result["success"] = true;
                return result.dump();
            } else {
                json error;
                error["error"] = "Node not found";
                return error.dump();
            }
        }

        // 未知命令
        json error;
        error["error"] = "Unknown command: " + command;
        return error.dump();
    }
    catch (const std::exception& e) {
        json error;
        error["error"] = std::string("Exception: ") + e.what();
        return error.dump();
    }
}
