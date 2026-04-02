#pragma once

#include "VehicleTypes.h"
#include "core/Math/Vector3.h"

namespace Moon {

class Camera;
class VehicleComponent;

class VehicleCameraRig {
public:
    void Attach(Camera* camera, VehicleComponent* vehicle);
    void Detach();
    bool IsAttached() const { return m_camera != nullptr && m_vehicle != nullptr; }
    void Update(float deltaTime);

private:
    static Vector3 Lerp(const Vector3& a, const Vector3& b, float t);

    Camera* m_camera = nullptr;
    VehicleComponent* m_vehicle = nullptr;
};

} // namespace Moon
