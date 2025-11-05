#include "BgfxRenderer.h"
#include "../core/Logging/Logger.h"
#include <cstdio>

bool BgfxRenderer::Initialize(const RenderInitParams&) {
    MOON_LOG_WARN("BgfxRenderer", "BgfxRenderer stub initialize - not implemented yet, using DiligentRenderer instead");
    return false; // returning false so caller can fallback
}

void BgfxRenderer::RenderFrame(){
    // Stub - no implementation yet
}

void BgfxRenderer::Resize(uint32_t w, uint32_t h){
    MOON_LOG_INFO("BgfxRenderer", "Resize stub called: %ux%u", w, h);
}

void BgfxRenderer::Shutdown(){
    MOON_LOG_INFO("BgfxRenderer", "Shutdown stub called");
}
