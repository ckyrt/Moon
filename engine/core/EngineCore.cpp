#include "EngineCore.h"
#include "Logging/Logger.h"

void EngineCore::Initialize() {
    MOON_LOG_INFO("EngineCore", "Initialize");
    
    // Initialize Input System
    m_inputSystem = std::make_unique<Moon::InputSystem>();
    MOON_LOG_INFO("EngineCore", "InputSystem initialized");
    
    // Initialize Camera with default settings
    m_camera = std::make_unique<Moon::PerspectiveCamera>(60.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    m_camera->SetPosition(Moon::Vector3(0.0f, 2.0f, -5.0f));
    m_camera->SetTarget(Moon::Vector3(0.0f, 0.0f, 0.0f));
    m_camera->SetUp(Moon::Vector3(0.0f, 1.0f, 0.0f));
    MOON_LOG_INFO("EngineCore", "Camera initialized");
    
    // Initialize Main Scene
    m_mainScene = std::make_unique<Moon::Scene>("Main Scene");
    MOON_LOG_INFO("EngineCore", "Main Scene initialized");
}

void EngineCore::Tick(double dt) {
    // Update Input System
    if (m_inputSystem) {
        m_inputSystem->Update();
    }
    
    // Update Main Scene (all nodes and components)
    if (m_mainScene) {
        m_mainScene->Update(static_cast<float>(dt));
    }
    
    // Game/Editor logic would advance here.
}

void EngineCore::Shutdown() {
    MOON_LOG_INFO("EngineCore", "Shutdown");
    
    // Shutdown in reverse order of initialization
    m_mainScene.reset();
    m_camera.reset();
    m_inputSystem.reset();
}
