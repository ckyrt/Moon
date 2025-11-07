#include "NullRenderer.h"
#include <cstdint>

bool NullRenderer::Initialize(const RenderInitParams& params) {
#ifdef _WIN32
    hwnd_ = static_cast<HWND>(params.windowHandle);
#endif
    width_ = params.width;
    height_ = params.height;
    tick_ = 0;
    return true;
}

void NullRenderer::RenderFrame() {
#ifdef _WIN32
    if (!hwnd_) return;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);

    // Animate background color (simple demo without timers).
    tick_++;
    int r = (tick_ * 2) % 255;
    int g = (tick_ * 5) % 255;
    int b = (tick_ * 7) % 255;

    HBRUSH brush = CreateSolidBrush(RGB(r,g,b));
    RECT rc;
    GetClientRect(hwnd_, &rc);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);

    EndPaint(hwnd_, &ps);
#endif
}

void NullRenderer::Resize(uint32_t w, uint32_t h) {
    width_ = w; height_ = h;
}

void NullRenderer::Shutdown() {
#ifdef _WIN32
    hwnd_ = nullptr;
#endif
}

void NullRenderer::BeginFrame() {
    // No-op for null renderer
}

void NullRenderer::EndFrame()
{
    // No-op for null renderer
}

void NullRenderer::SetViewProjectionMatrix(const float* viewProj16)
{
    (void)viewProj16; // Suppress unused parameter warning
}

void NullRenderer::DrawMesh(Moon::Mesh* mesh, const Moon::Matrix4x4& worldMatrix)
{
    (void)mesh;
    (void)worldMatrix;
    // No-op for null renderer
}

void NullRenderer::DrawCube(const Moon::Matrix4x4& worldMatrix)
{
    (void)worldMatrix; // Suppress unused parameter warning
}
