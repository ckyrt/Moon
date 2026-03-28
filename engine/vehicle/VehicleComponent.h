#pragma once

#include "VehicleTypes.h"
#include "core/Scene/Component.h"

#include <vector>

namespace JPH {
class VehicleConstraint;
class VehicleCollisionTester;
class WheeledVehicleController;
}

namespace Moon {

class PhysicsSystem;
class RigidBody;
class SceneNode;

class VehicleComponent : public Component {
public:
    explicit VehicleComponent(SceneNode* owner);
    ~VehicleComponent() override;

    void SetConfig(const VehicleConfig& config);
    const VehicleConfig& GetConfig() const { return m_config; }

    bool InitializeVehicleRuntime(PhysicsSystem* physicsSystem);
    void ShutdownVehicleRuntime();

    void SetInputState(const VehicleInputState& inputState) { m_inputState = inputState; }
    const VehicleInputState& GetInputState() const { return m_inputState; }

    void SetDriverOccupied(bool occupied) { m_driverOccupied = occupied; }
    bool HasDriver() const { return m_driverOccupied; }

    void SetWheelVisuals(const std::vector<SceneNode*>& wheelVisuals) { m_wheelVisuals = wheelVisuals; }
    const std::vector<SceneNode*>& GetWheelVisuals() const { return m_wheelVisuals; }

    const std::vector<WheelRuntimeState>& GetWheelRuntime() const { return m_wheelRuntime; }

    void SetCurrentSpeed(float speed) { m_currentSpeed = speed; }
    float GetCurrentSpeed() const { return m_currentSpeed; }

    RigidBody* GetRigidBody() const;
    void PostPhysicsSync();
    void Update(float deltaTime) override;

private:
    bool ConfigureControllerRuntime();
    void UpdateWheelRuntimeFromJolt();
    void UpdateWheelVisualsFromJolt();
    float ResolveForwardInput(float forwardSpeed) const;

    VehicleConfig m_config;
    VehicleInputState m_inputState;
    std::vector<WheelRuntimeState> m_wheelRuntime;
    std::vector<SceneNode*> m_wheelVisuals;
    PhysicsSystem* m_physicsSystem = nullptr;
    JPH::VehicleConstraint* m_vehicleConstraint = nullptr;
    JPH::VehicleCollisionTester* m_collisionTester = nullptr;
    bool m_controllerRuntimeConfigured = false;
    float m_currentSpeed = 0.0f;
    bool m_driverOccupied = false;
};

} // namespace Moon
