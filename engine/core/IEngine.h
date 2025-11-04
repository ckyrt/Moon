#pragma once
class IEngine {
public:
    virtual ~IEngine() = default;
    virtual void Initialize() = 0;
    virtual void Tick(double dt) = 0;
    virtual void Shutdown() = 0;
};
