#include "EngineCore.h"
#include "Logging/Logger.h"

void EngineCore::Initialize() {
    MOON_LOG_INFO("EngineCore", "Initialize");
}

void EngineCore::Tick(double dt) {
    (void)dt;
    // Game/Editor logic would advance here.
}

void EngineCore::Shutdown() {
    MOON_LOG_INFO("EngineCore", "Shutdown");
}
