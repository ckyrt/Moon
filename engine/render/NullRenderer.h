#pragma once
#include "IRenderer.h"
#include "RenderCommon.h"

class NullRenderer : public IRenderer {
public:
    bool Initialize(const RenderInitParams& params) override;
    void RenderFrame() override;
    void Resize(uint32_t w, uint32_t h) override;
    void Shutdown() override;

private:
#ifdef _WIN32
    HWND hwnd_ = nullptr;
#endif
    unsigned tick_ = 0;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};
