// ============================================================================
// EditorApp_WndProc.cpp - 窗口消息处理
// ============================================================================
#include "EditorApp.h"
#include "UITextureManager.h"
#include "../engine/core/EngineCore.h"
#include "../engine/core/Scene/Scene.h"
#include "../engine/core/Scene/SceneNode.h"
#include "../engine/core/Logging/Logger.h"
#include "../engine/render/diligent/DiligentRenderer.h"
#include "EditorBridge.h"
#include "ImGuiImplWin32.hpp"
#include "imgui.h"
#include "ImGuizmo.h"
#include <Windows.h>
#include <windowsx.h>

// ============================================================================
// 3D场景交互处理（picking）- 由MainWindowProc通过callback调用
// ============================================================================
void Handle3DScenePicking(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg != WM_LBUTTONDOWN) return;
    
    if (g_Renderer && g_Engine) {
        // 避免与 ImGuizmo 冲突
        if (!ImGuizmo::IsOver()) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // 渲染拾取通道
            g_Renderer->RenderSceneForPicking(g_Engine->GetScene());

            // 读取像素下的 ObjectID
            uint32_t objectID = g_Renderer->ReadObjectIDAt(x, y);

            if (objectID != 0) {
                // 查找对应的 SceneNode
                Moon::Scene* scene = g_Engine->GetScene();
                g_SelectedObject = nullptr;
                scene->Traverse([objectID](Moon::SceneNode* node) {
                    if (node->GetID() == objectID) {
                        g_SelectedObject = node;
                    }
                });

                if (g_SelectedObject) {
                    MOON_LOG_INFO("EditorApp", "Selected object: %s (ID=%u)",
                        g_SelectedObject->GetName().c_str(), objectID);

                    // 通知 WebUI
                    if (g_EditorBridge && g_EditorBridge->GetClient() && g_EditorBridge->GetClient()->GetBrowser()) {
                        char jsCode[256];
                        snprintf(jsCode, sizeof(jsCode),
                            "if (window.onNodeSelected) { window.onNodeSelected(%u); }",
                            objectID
                        );
                        auto frame = g_EditorBridge->GetClient()->GetBrowser()->GetMainFrame();
                        frame->ExecuteJavaScript(jsCode, frame->GetURL(), 0);
                    }
                }
            }
            else {
                // 点击空白处取消选择
                g_SelectedObject = nullptr;
                MOON_LOG_INFO("EditorApp", "Deselected (ObjectID = 0)");

                // 通知 WebUI
                if (g_EditorBridge && g_EditorBridge->GetClient() && g_EditorBridge->GetClient()->GetBrowser()) {
                    char jsCode[256];
                    snprintf(jsCode, sizeof(jsCode), "if (window.onNodeSelected) { window.onNodeSelected(null); }");
                    auto frame = g_EditorBridge->GetClient()->GetBrowser()->GetMainFrame();
                    frame->ExecuteJavaScript(jsCode, frame->GetURL(), 0);
                }
            }
        }
    }
}
