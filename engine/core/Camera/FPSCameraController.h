#pragma once
#include "Camera.h"
#include "../Input/IInputSystem.h"
#include "../Input/InputTypes.h"

namespace Moon {

class FPSCameraController {
public:
    FPSCameraController(Camera* cam, IInputSystem* input);
    ~FPSCameraController() = default;
    void Update(float dt);
    void SetMoveSpeed(float s) { m_speed = s; }
    void SetSprintMultiplier(float m) { m_sprint = m; }
    void SetMouseSensitivity(float s) { m_sens = s; }
    float GetMoveSpeed() const { return m_speed; }
    float GetMouseSensitivity() const { return m_sens; }
    void SetEnabled(bool e) { m_enabled = e; }
    bool IsEnabled() const { return m_enabled; }
private:
    Camera* m_camera;
    IInputSystem* m_input;
    float m_speed, m_sprint, m_sens, m_yaw, m_pitch;
    bool m_enabled, m_firstMouse;
    Vector2 m_lastMouse;
    void ProcessKeyboard(float dt);
    void ProcessMouse(float dt);
    void UpdateOrientation();
};

}