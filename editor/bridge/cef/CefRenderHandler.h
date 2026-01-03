#pragma once
#include "include/cef_render_handler.h"
#include <functional>

// 回调函数类型：CEF 离屏渲染完成后，传递像素数据
// buffer: BGRA 格式的像素数据
// width, height: 纹理尺寸
using OnPaintCallback = std::function<void(const void* buffer, int width, int height)>;

// CEF 离屏渲染处理器（Off-Screen Rendering）
// 用于将 CEF 浏览器渲染到纹理，而不是窗口
class CefRenderHandlerImpl : public CefRenderHandler {
public:
    CefRenderHandlerImpl();

    // 设置 CEF 渲染完成回调
    void SetOnPaintCallback(OnPaintCallback callback);

    // 设置视口大小（必须在创建浏览器前或调整窗口时调用）
    void SetViewportSize(int width, int height);

    // ========================================
    // CefRenderHandler 接口实现
    // ========================================

    // 获取视口矩形
    void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;

    // CEF 渲染完成回调（主要的渲染输出）
    void OnPaint(
        CefRefPtr<CefBrowser> browser,
        PaintElementType type,
        const RectList& dirtyRects,
        const void* buffer,
        int width,
        int height
    ) override;

private:
    int m_width = 1280;
    int m_height = 720;
    OnPaintCallback m_onPaint;

    IMPLEMENT_REFCOUNTING(CefRenderHandlerImpl);
};
