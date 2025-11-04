#pragma once
#include "IEngine.h"

class EngineCore : public IEngine {
public:
    void Initialize() override;
    void Tick(double dt) override;
    void Shutdown() override;
};
