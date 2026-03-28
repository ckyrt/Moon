#pragma once

#include "Assets/MeshManager.h"
#include "Camera/PerspectiveCamera.h"
#include "IEngine.h"
#include "Input/InputSystem.h"
#include "Scene/Scene.h"
#include "Texture/TextureManager.h"

#include <memory>

namespace Moon {
class PhysicsSystem;
}

class EngineCore : public IEngine {
public:
    ~EngineCore();
    void Initialize() override;
    void Tick(double dt) override;
    void Shutdown() override;

    Moon::InputSystem* GetInputSystem() { return m_inputSystem.get(); }
    Moon::PerspectiveCamera* GetCamera() { return m_camera.get(); }
    Moon::Scene* GetScene() { return m_mainScene.get(); }
    Moon::MeshManager* GetMeshManager() { return m_meshManager.get(); }
    Moon::TextureManager* GetTextureManager() { return m_textureManager.get(); }
    Moon::PhysicsSystem* GetPhysicsSystem() { return m_physicsSystem.get(); }

private:
    void SyncPhysicsToScene();

    std::unique_ptr<Moon::InputSystem> m_inputSystem;
    std::unique_ptr<Moon::PerspectiveCamera> m_camera;
    std::unique_ptr<Moon::Scene> m_mainScene;
    std::unique_ptr<Moon::MeshManager> m_meshManager;
    std::unique_ptr<Moon::TextureManager> m_textureManager;
    std::shared_ptr<Moon::PhysicsSystem> m_physicsSystem;
    double m_physicsAccumulator = 0.0;
    static constexpr double kFixedPhysicsStep = 1.0 / 60.0;
};
