// ============================================================================
// EditorApp_Gizmo.cpp - ImGuizmo 渲染和变换应用
// ============================================================================
#include "EditorApp.h"
#include "../engine/core/EngineCore.h"
#include "../engine/core/Scene/SceneNode.h"
#include "../engine/core/Scene/Transform.h"
#include "../engine/core/Camera/Camera.h"
#include "../engine/core/Math/Matrix4x4.h"
#include "../engine/core/Math/Vector3.h"
#include "../engine/core/Math/Quaternion.h"
#include "EditorBridge.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include <stdio.h>

// ============================================================================
// 渲染并应用 ImGuizmo 变换
// ============================================================================
void RenderAndApplyGizmo(EngineCore* engine, EditorBridge& bridge)
{
    if (!g_SelectedObject) return;

    Moon::Transform* tr = g_SelectedObject->GetTransform();
    Moon::Transform* parent = g_SelectedObject->GetParent()
        ? g_SelectedObject->GetParent()->GetTransform()
        : nullptr;

    auto view = engine->GetCamera()->GetViewMatrix();
    auto proj = engine->GetCamera()->GetProjectionMatrix();

    //-------------------------------------------------------
    // 选择 Gizmo 模式
    //-------------------------------------------------------
    ImGuizmo::MODE mode =
        (g_GizmoOperation == ImGuizmo::SCALE)
        ? ImGuizmo::LOCAL
        : g_GizmoMode;

    //-------------------------------------------------------
    // 只在非拖动时刷新矩阵（保持拖动连续性）
    //-------------------------------------------------------
    if (!g_WasUsingGizmo) {
        g_GizmoMatrix = tr->GetWorldMatrix();
    }

    //-------------------------------------------------------
    // 调用 Manipulate
    //-------------------------------------------------------
    ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0],
        g_GizmoOperation, mode,
        &g_GizmoMatrix.m[0][0]);

    //-------------------------------------------------------
    // Manipulate 之后读取状态
    //-------------------------------------------------------
    bool usingGizmo = ImGuizmo::IsUsing();

    //-------------------------------------------------------
    // 🆕 拖动开始：通知 Web UI 记录初始状态
    //-------------------------------------------------------
    if (!g_WasUsingGizmo && usingGizmo)
    {
        if (bridge.GetClient() && bridge.GetClient()->GetBrowser())
        {
            char js[256];
            snprintf(js, sizeof(js),
                "if (window.onGizmoStart) { window.onGizmoStart(%d); }",
                g_SelectedObject->GetID()
            );

            auto frame = bridge.GetClient()->GetBrowser()->GetMainFrame();
            frame->ExecuteJavaScript(js, frame->GetURL(), 0);
        }
    }

    //-------------------------------------------------------
    // 拖动中：实时应用变换到 Transform
    //-------------------------------------------------------
    if (usingGizmo)
    {
        if (g_GizmoOperation == ImGuizmo::TRANSLATE)
        {
            Moon::Vector3 worldPos = {
                g_GizmoMatrix.m[3][0],
                g_GizmoMatrix.m[3][1],
                g_GizmoMatrix.m[3][2]
            };

            // world -> local
            if (parent)
                worldPos = parent->GetWorldMatrix().Inverse().MultiplyPoint(worldPos);

            tr->SetLocalPosition(worldPos);
        }
        else if (g_GizmoOperation == ImGuizmo::ROTATE)
        {
            Moon::Vector3 scale = ExtractScale(g_GizmoMatrix);
            Moon::Matrix4x4 rotMat = RemoveScale(g_GizmoMatrix, scale);
            // 从旋转矩阵生成世界旋转（imgui gizmo 给的就是列主序矩阵，无额外转换）
            Moon::Quaternion worldRot = Moon::Quaternion::FromMatrix(rotMat);
            // 注意：ImGuizmo 在矩阵中使用的是右手坐标系，而 Moon 内部的四元数是左手系
            // 因此需要把 x,y,z 分量取反，以对齐旋转方向
            // 这相当于对四元数做一次：q = (-v, w)
            worldRot = Moon::Quaternion(-worldRot.x, -worldRot.y, -worldRot.z, worldRot.w);

            // 保持符号连续性
            worldRot = StabilizeQuaternion(worldRot, g_LastRotation);
            g_LastRotation = worldRot;

            // world → local
            if (parent)
                worldRot = parent->GetWorldRotation().Inverse() * worldRot;

            tr->SetLocalRotation(worldRot);
        }
        else if (g_GizmoOperation == ImGuizmo::SCALE)
        {
            Moon::Vector3 worldScale = ExtractScale(g_GizmoMatrix);

            if (parent)
            {
                Moon::Vector3 parentScale = parent->GetWorldScale();
                if (parentScale.x > 0.0001f) worldScale.x /= parentScale.x;
                if (parentScale.y > 0.0001f) worldScale.y /= parentScale.y;
                if (parentScale.z > 0.0001f) worldScale.z /= parentScale.z;
            }

            tr->SetLocalScale(worldScale);
        }
    }

    //-------------------------------------------------------
    // 🆕 拖动结束：通知 Web UI 创建 Undo Command
    //-------------------------------------------------------
    if (g_WasUsingGizmo && !usingGizmo)
    {
        auto localPos = tr->GetLocalPosition();
        auto localRot = tr->GetLocalRotation();  // 获取四元数，不是欧拉角
        auto localScale = tr->GetLocalScale();

        if (bridge.GetClient() && bridge.GetClient()->GetBrowser())
        {
            char js[512];
            snprintf(js, sizeof(js),
                "if (window.onGizmoEnd) { window.onGizmoEnd(%d, "
                "{x:%.3f, y:%.3f, z:%.3f}, "  // position
                "{x:%.3f, y:%.3f, z:%.3f, w:%.3f}, "  // rotation (quaternion)
                "{x:%.3f, y:%.3f, z:%.3f}"  // scale
                "); }",
                g_SelectedObject->GetID(),
                localPos.x, localPos.y, localPos.z,
                localRot.x, localRot.y, localRot.z, localRot.w,
                localScale.x, localScale.y, localScale.z
            );

            auto frame = bridge.GetClient()->GetBrowser()->GetMainFrame();
            frame->ExecuteJavaScript(js, frame->GetURL(), 0);
        }
    }

    g_WasUsingGizmo = usingGizmo;
}
