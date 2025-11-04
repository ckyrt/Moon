#pragma once
#include <cstdint>

struct RenderInitParams {
    void* windowHandle = nullptr; // HWND on Windows
    uint32_t width = 1280;
    uint32_t height = 720;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool Initialize(const RenderInitParams& params) = 0;
    virtual void RenderFrame() = 0;
    virtual void Resize(uint32_t w, uint32_t h) = 0;
    virtual void Shutdown() = 0;
};
