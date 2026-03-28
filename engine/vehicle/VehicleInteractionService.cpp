#include "VehicleInteractionService.h"

#include "VehicleComponent.h"
#include "../core/Logging/Logger.h"
#include "core/Camera/Camera.h"
#include "core/Camera/FPSCameraController.h"
#include "core/Input/IInputSystem.h"
#include "core/Input/KeyCodes.h"
#include "core/Scene/Scene.h"
#include "core/Scene/SceneNode.h"
#include "core/Scene/Transform.h"

#include <limits>

namespace Moon {

VehicleInteractionService::VehicleInteractionService(
    Scene* scene,
    Camera* camera,
    IInputSystem* input,
    FPSCameraController* freeCamera)
    : m_scene(scene)
    , m_camera(camera)
    , m_input(input)
    , m_freeCamera(freeCamera) {
}

void VehicleInteractionService::BeginFrame()
{
    if (!m_scene || !m_camera || !m_input) {
        return;
    }

    if (!m_activeVehicle) {
        if (m_input->IsKeyPressed(KeyCode::F7)) {
            EnterVehicle(FindClosestEnterableVehicle(true));
            return;
        }
        if (m_input->IsKeyPressed(KeyCode::E)) {
            EnterVehicle(FindClosestEnterableVehicle());
        }
        return;
    }

    const VehicleInputState inputState = BuildDrivingInput();
    m_activeVehicle->SetInputState(inputState);

    if (inputState.exitRequested) {
        ExitVehicle();
    }
}

void VehicleInteractionService::EndFrame(float deltaTime)
{
    if (m_activeVehicle) {
        m_cameraRig.Update(deltaTime);
    }
}

VehicleComponent* VehicleInteractionService::FindClosestEnterableVehicle(bool ignoreRadius) const
{
    if (!m_scene || !m_camera) {
        return nullptr;
    }

    VehicleComponent* bestVehicle = nullptr;
    float bestDistanceSq = std::numeric_limits<float>::max();
    const Vector3 observerPosition = m_camera->GetPosition();

    m_scene->TraverseActive([&](SceneNode* node) {
        if (!node) {
            return;
        }

        VehicleComponent* vehicle = node->GetComponent<VehicleComponent>();
        if (!vehicle || vehicle->HasDriver()) {
            return;
        }

        const Vector3 delta = node->GetTransform()->GetWorldPosition() - observerPosition;
        const float distanceSq = Vector3::Dot(delta, delta);
        const float enterRadius = vehicle->GetConfig().enterRadius;
        if ((ignoreRadius || distanceSq <= enterRadius * enterRadius) && distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestVehicle = vehicle;
        }
    });

    return bestVehicle;
}

VehicleInputState VehicleInteractionService::BuildDrivingInput() const
{
    VehicleInputState state;
    if (!m_input) {
        return state;
    }

    if (m_input->IsKeyDown(KeyCode::W) || m_input->IsKeyDown(KeyCode::Up)) {
        state.throttle = 1.0f;
    }
    if (m_input->IsKeyDown(KeyCode::S) || m_input->IsKeyDown(KeyCode::Down)) {
        state.brake = 1.0f;
    }
    if (m_input->IsKeyDown(KeyCode::A) || m_input->IsKeyDown(KeyCode::Left)) {
        state.steering -= 1.0f;
    }
    if (m_input->IsKeyDown(KeyCode::D) || m_input->IsKeyDown(KeyCode::Right)) {
        state.steering += 1.0f;
    }

    state.handbrake = m_input->IsKeyDown(KeyCode::Space);
    state.exitRequested = m_input->IsKeyPressed(KeyCode::E);
    return state;
}

void VehicleInteractionService::EnterVehicle(VehicleComponent* vehicle)
{
    if (!vehicle || !m_camera) {
        return;
    }

    m_activeVehicle = vehicle;
    m_activeVehicle->SetDriverOccupied(true);
    m_activeVehicle->SetInputState(VehicleInputState{});

    if (m_freeCamera) {
        m_freeCamera->SetEnabled(false);
    }

    SceneNode* owner = m_activeVehicle->GetOwner();
    const Vector3 position = owner ? owner->GetTransform()->GetWorldPosition() : Vector3(0.0f, 0.0f, 0.0f);
    MOON_LOG_INFO(
        "VehicleInteraction",
        "Entered vehicle '%s' at world position (%.2f, %.2f, %.2f)",
        owner ? owner->GetName().c_str() : "<null>",
        position.x,
        position.y,
        position.z);

    m_cameraRig.Attach(m_camera, m_activeVehicle);
    m_cameraRig.Update(1.0f);
}

void VehicleInteractionService::ExitVehicle()
{
    if (!m_activeVehicle || !m_camera) {
        return;
    }

    SceneNode* owner = m_activeVehicle->GetOwner();
    Transform* transform = owner ? owner->GetTransform() : nullptr;
    if (transform) {
        const float exitDistance = m_activeVehicle->GetConfig().exitDistance;
        const Vector3 exitPosition =
            transform->GetWorldPosition() +
            transform->GetRight().Normalized() * exitDistance +
            Vector3(0.0f, 1.8f, 0.0f);
        const Vector3 lookTarget =
            transform->GetWorldPosition() + transform->GetForward().Normalized() * 8.0f;
        m_camera->SetPosition(exitPosition);
        m_camera->SetTarget(lookTarget);
        m_camera->SetUp(Vector3(0.0f, 1.0f, 0.0f));
    }

    m_activeVehicle->SetDriverOccupied(false);
    m_activeVehicle->SetInputState(VehicleInputState{});
    MOON_LOG_INFO("VehicleInteraction", "Exited active vehicle.");
    m_activeVehicle = nullptr;
    m_cameraRig.Detach();

    if (m_freeCamera) {
        m_freeCamera->SetEnabled(true);
    }
}

} // namespace Moon
