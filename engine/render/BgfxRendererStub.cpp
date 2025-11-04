#include "BgfxRenderer.h"
#include <cstdio>

bool BgfxRenderer::Initialize(const RenderInitParams&) {
    std::puts("[BgfxRenderer] Stub initialize (not implemented). Using NullRenderer for now.");
    return false; // returning false so caller can fallback
}
void BgfxRenderer::RenderFrame(){}
void BgfxRenderer::Resize(uint32_t, uint32_t){}
void BgfxRenderer::Shutdown(){}
