// ============================================================================
// EditorApp_Utils.cpp - 辅助函数
// ============================================================================
#include "EditorApp.h"
#include "../engine/core/Math/Vector3.h"
#include "../engine/core/Math/Matrix4x4.h"
#include "../engine/core/Math/Quaternion.h"
#include "../engine/core/Logging/Logger.h"
#include <Windows.h>

// ============================================================================
// 查找 CEF 的 HTML 渲染窗口
// ============================================================================
HWND FindCefHtmlRenderWindow(HWND cefWindow)
{
    HWND htmlWindow = FindWindowExW(cefWindow, nullptr, L"Chrome_RenderWidgetHostHWND", nullptr);
    if (htmlWindow) return htmlWindow;

    HWND chromeWidget = FindWindowExW(cefWindow, nullptr, L"Chrome_WidgetWin_0", nullptr);
    if (chromeWidget) {
        htmlWindow = FindWindowExW(chromeWidget, nullptr, L"Chrome_RenderWidgetHostHWND", nullptr);
        if (htmlWindow) return htmlWindow;
    }

    MOON_LOG_INFO("EditorApp", "Searching for HTML render window via enumeration...");
    EnumChildWindows(cefWindow, [](HWND hwnd, LPARAM lParam) -> BOOL {
        wchar_t cls[256];
        GetClassNameW(hwnd, cls, 256);
        if (wcscmp(cls, L"Chrome_RenderWidgetHostHWND") == 0) {
            *reinterpret_cast<HWND*>(lParam) = hwnd;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&htmlWindow));

    return htmlWindow;
}

// ============================================================================
// 提取矩阵的缩放分量（列向量长度）
// ============================================================================
Moon::Vector3 ExtractScale(const Moon::Matrix4x4& m)
{
    return {
        Moon::Vector3(m.m[0][0], m.m[0][1], m.m[0][2]).Length(),
        Moon::Vector3(m.m[1][0], m.m[1][1], m.m[1][2]).Length(),
        Moon::Vector3(m.m[2][0], m.m[2][1], m.m[2][2]).Length()
    };
}

// ============================================================================
// 移除缩放，得到纯旋转矩阵
// ============================================================================
Moon::Matrix4x4 RemoveScale(const Moon::Matrix4x4& m, const Moon::Vector3& s)
{
    Moon::Matrix4x4 r = m;
    if (s.x > 0.0001f) { r.m[0][0] /= s.x; r.m[0][1] /= s.x; r.m[0][2] /= s.x; }
    if (s.y > 0.0001f) { r.m[1][0] /= s.y; r.m[1][1] /= s.y; r.m[1][2] /= s.y; }
    if (s.z > 0.0001f) { r.m[2][0] /= s.z; r.m[2][1] /= s.z; r.m[2][2] /= s.z; }
    return r;
}

// ============================================================================
// 四元数符号稳定化（修复双覆盖问题）
// ============================================================================
Moon::Quaternion StabilizeQuaternion(
    const Moon::Quaternion& newQ,
    const Moon::Quaternion& lastQ)
{
    float dot =
        newQ.x * lastQ.x +
        newQ.y * lastQ.y +
        newQ.z * lastQ.z +
        newQ.w * lastQ.w;

    if (dot < 0.f)
    {
        return Moon::Quaternion(-newQ.x, -newQ.y, -newQ.z, -newQ.w);
    }
    return newQ;
}
