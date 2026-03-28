#pragma once

#include "VehicleCameraRig.h"
#include "VehicleTypes.h"
#include "core/Math/Vector3.h"

namespace Moon {

class Camera;
class FPSCameraController;
class IInputSystem;
class Scene;
class VehicleComponent;

class VehicleInteractionService {
public:
    VehicleInteractionService(Scene* scene, Camera* camera, IInputSystem* input, FPSCameraController* freeCamera);

    void BeginFrame();
    void EndFrame(float deltaTime);
    bool IsDriving() const { return m_activeVehicle != nullptr; }
    VehicleComponent* GetActiveVehicle() const { return m_activeVehicle; }

private:
    VehicleComponent* FindClosestEnterableVehicle(bool ignoreRadius = false) const;
    VehicleInputState BuildDrivingInput() const;
    void EnterVehicle(VehicleComponent* vehicle);
    void ExitVehicle();

    Scene* m_scene = nullptr;
    Camera* m_camera = nullptr;
    IInputSystem* m_input = nullptr;
    FPSCameraController* m_freeCamera = nullptr;
    VehicleComponent* m_activeVehicle = nullptr;
    VehicleCameraRig m_cameraRig;
};

} // namespace Moon
