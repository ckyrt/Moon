#pragma once
#include "IEngine.h"
#include "Input/InputSystem.h"
#include "Camera/PerspectiveCamera.h"
#include <memory>

class EngineCore : public IEngine {
public:
    void Initialize() override;
    void Tick(double dt) override;
    void Shutdown() override;
    
    Moon::InputSystem* GetInputSystem() { return m_inputSystem.get(); }
    Moon::PerspectiveCamera* GetCamera() { return m_camera.get(); }
    
private:
    std::unique_ptr<Moon::InputSystem> m_inputSystem;
    std::unique_ptr<Moon::PerspectiveCamera> m_camera;
};
