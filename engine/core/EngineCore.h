#pragma once
#include "IEngine.h"
#include "Input/InputSystem.h"
#include "Camera/PerspectiveCamera.h"
#include "Scene/Scene.h"
#include "Assets/MeshManager.h"
#include <memory>

class EngineCore : public IEngine {
public:
    void Initialize() override;
    void Tick(double dt) override;
    void Shutdown() override;
    
    // 访问器
    Moon::InputSystem* GetInputSystem() { return m_inputSystem.get(); }
    Moon::PerspectiveCamera* GetCamera() { return m_camera.get(); }
    Moon::Scene* GetScene() { return m_mainScene.get(); }
    Moon::MeshManager* GetMeshManager() { return m_meshManager.get(); }
    
private:
    std::unique_ptr<Moon::InputSystem> m_inputSystem;
    std::unique_ptr<Moon::PerspectiveCamera> m_camera;
    std::unique_ptr<Moon::Scene> m_mainScene;
    std::unique_ptr<Moon::MeshManager> m_meshManager;
};
