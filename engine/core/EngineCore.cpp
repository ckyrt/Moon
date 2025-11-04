#include "EngineCore.h"
#include <cstdio>

void EngineCore::Initialize() {
    std::puts("[EngineCore] Initialize");
}

void EngineCore::Tick(double dt) {
    (void)dt;
    // Game/Editor logic would advance here.
}

void EngineCore::Shutdown() {
    std::puts("[EngineCore] Shutdown");
}
