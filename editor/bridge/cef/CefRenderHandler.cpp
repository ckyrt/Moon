#include "CefRenderHandler.h"
#include <iostream>

CefRenderHandlerImpl::CefRenderHandlerImpl()
    : m_width(800), m_height(600), m_onPaint(nullptr)
{
    std::cout << "[CefRenderHandler] Created (OSR mode ready)" << std::endl;
}

void CefRenderHandlerImpl::SetOnPaintCallback(OnPaintCallback callback)
{
    m_onPaint = callback;
}

void CefRenderHandlerImpl::SetViewportSize(int width, int height)
{
    m_width = width;
    m_height = height;
    std::cout << "[CefRenderHandler] Viewport size set to " << width << "x" << height << std::endl;
}

void CefRenderHandlerImpl::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
    rect.x = 0;
    rect.y = 0;
    rect.width = m_width;
    rect.height = m_height;
}

void CefRenderHandlerImpl::OnPaint(
    CefRefPtr<CefBrowser> browser,
    PaintElementType type,
    const RectList& dirtyRects,
    const void* buffer,
    int width,
    int height)
{
    // 只处理主视图（不处理弹出窗口）
    if (type != PET_VIEW) {
        return;
    }

    // 调用回调函数，将像素数据传递给渲染系统
    if (m_onPaint) {
        m_onPaint(buffer, width, height);
    }
}
