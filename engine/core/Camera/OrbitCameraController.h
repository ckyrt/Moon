#pragma once
#include "Camera.h"
#include "../Input/IInputSystem.h"
#include "../Input/InputTypes.h"

namespace Moon {

class OrbitCameraController {
public:
    OrbitCameraController(Camera* cam, IInputSystem* input);
    ~OrbitCameraController() = default;
    void Update(float dt);
    void SetTarget(const Vector3& t) { m_target = t; UpdatePosition(); }
    void SetDistance(float d);
    void SetRotationSensitivity(float s) { m_rotSens = s; }
    void SetZoomSensitivity(float s) { m_zoomSens = s; }
    void SetPanSensitivity(float s) { m_panSens = s; }
    void SetDistanceRange(float min, float max) { m_minDist = min; m_maxDist = max; }
    Vector3 GetTarget() const { return m_target; }
    float GetDistance() const { return m_dist; }
    void SetEnabled(bool e) { m_enabled = e; }
    bool IsEnabled() const { return m_enabled; }
private:
    Camera* m_camera;
    IInputSystem* m_input;
    Vector3 m_target;
    float m_dist, m_yaw, m_pitch, m_rotSens, m_zoomSens, m_panSens, m_minDist, m_maxDist;
    bool m_enabled, m_firstMouse;
    Vector2 m_lastMouse;
    void ProcessRotation();
    void ProcessZoom();
    void ProcessPan();
    void UpdatePosition();
};

}