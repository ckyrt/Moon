#include "VehicleCameraRig.h"

#include "VehicleComponent.h"
#include "core/Camera/Camera.h"
#include "core/Scene/SceneNode.h"
#include "core/Scene/Transform.h"

#include <algorithm>
#include <cmath>

namespace Moon {

namespace {

float Clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

}

void VehicleCameraRig::Attach(Camera* camera, VehicleComponent* vehicle)
{
    m_camera = camera;
    m_vehicle = vehicle;
}

void VehicleCameraRig::Detach()
{
    m_camera = nullptr;
    m_vehicle = nullptr;
}

Vector3 VehicleCameraRig::Lerp(const Vector3& a, const Vector3& b, float t)
{
    return a + (b - a) * t;
}

void VehicleCameraRig::Update(float deltaTime)
{
    if (!m_camera || !m_vehicle || !m_vehicle->GetOwner()) {
        return;
    }

    Transform* transform = m_vehicle->GetOwner()->GetTransform();
    if (!transform) {
        return;
    }

    const VehicleCameraSettings& settings = m_vehicle->GetConfig().camera;
    const Vector3 worldPosition = transform->GetWorldPosition();
    const Vector3 forward = transform->GetForward().Normalized();
    const Vector3 up = transform->GetUp().Normalized();
    const Vector3 right = transform->GetRight().Normalized();

    const Vector3 desiredCamera =
        worldPosition +
        right * settings.followOffset.x +
        up * settings.followOffset.y +
        forward * settings.followOffset.z;
    const Vector3 desiredTarget =
        worldPosition +
        right * settings.lookOffset.x +
        up * settings.lookOffset.y +
        forward * settings.lookOffset.z;

    const float positionAlpha = Clamp01(settings.positionSmoothing * deltaTime);
    const float targetAlpha = Clamp01(settings.targetSmoothing * deltaTime);

    m_camera->SetPosition(Lerp(m_camera->GetPosition(), desiredCamera, positionAlpha));
    m_camera->SetTarget(Lerp(m_camera->GetTarget(), desiredTarget, targetAlpha));
    m_camera->SetUp(up);
}

} // namespace Moon
