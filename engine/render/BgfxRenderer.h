#pragma once
#include "IRenderer.h"

// Placeholder for future bgfx integration.
// Implement later (AI task): create device, swap chain, clear frame.
class BgfxRenderer : public IRenderer {
public:
    bool Initialize(const RenderInitParams& params) override;
    void RenderFrame() override;
    void Resize(uint32_t w, uint32_t h) override;
    void Shutdown() override;
};
