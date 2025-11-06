#include "OrbitCameraController.h"
#include <algorithm>
#include <cmath>

namespace Moon {

template<typename T> T clamp(const T& v, const T& lo, const T& hi) { return std::max(lo, std::min(v, hi)); }

OrbitCameraController::OrbitCameraController(Camera* cam, IInputSystem* input)
: m_camera(cam), m_input(input), m_target(0,0,0), m_dist(10.0f), m_yaw(0.0f), m_pitch(30.0f)
, m_rotSens(0.2f), m_zoomSens(1.0f), m_panSens(0.01f), m_minDist(1.0f), m_maxDist(100.0f)
, m_enabled(true), m_firstMouse(true), m_lastMouse(0,0) { UpdatePosition(); }

void OrbitCameraController::Update(float dt) {
    if(!m_enabled) return;
    ProcessRotation();
    ProcessZoom();
    ProcessPan();
    UpdatePosition();
}

void OrbitCameraController::ProcessRotation() {
    bool mid = m_input->IsMouseButtonDown(MouseButton::Middle);
    bool rgt = m_input->IsMouseButtonDown(MouseButton::Right);
    if(!mid && !rgt) { m_firstMouse = true; return; }
    Vector2 pos = m_input->GetMousePosition();
    if(m_firstMouse) { m_lastMouse = pos; m_firstMouse = false; }
    Vector2 delta = pos - m_lastMouse;
    m_lastMouse = pos;
    m_yaw += delta.x * m_rotSens;
    m_pitch = clamp(m_pitch - delta.y * m_rotSens, -89.0f, 89.0f);
    while(m_yaw > 360.0f) m_yaw -= 360.0f;
    while(m_yaw < 0.0f) m_yaw += 360.0f;
}

void OrbitCameraController::ProcessZoom() {
    float scroll = m_input->GetMouseScrollDelta().y;
    if(scroll == 0.0f) return;
    m_dist = clamp(m_dist - scroll * m_zoomSens, m_minDist, m_maxDist);
}

void OrbitCameraController::ProcessPan() {
    bool shift = m_input->IsKeyDown(KeyCode::LeftShift) || m_input->IsKeyDown(KeyCode::RightShift);
    bool mid = m_input->IsMouseButtonDown(MouseButton::Middle);
    if(!shift || !mid) return;
    Vector2 delta = m_input->GetMouseDelta();
    Vector3 right = m_camera->GetRight();
    Vector3 up = m_camera->GetUp();
    m_target = m_target - right * (delta.x * m_panSens * m_dist);
    m_target = m_target + up * (delta.y * m_panSens * m_dist);
}

void OrbitCameraController::UpdatePosition() {
    float yawRad = m_yaw * 3.14159265f / 180.0f;
    float pitchRad = m_pitch * 3.14159265f / 180.0f;
    float x = m_dist * cosf(pitchRad) * sinf(yawRad);
    float y = m_dist * sinf(pitchRad);
    float z = m_dist * cosf(pitchRad) * cosf(yawRad);
    Vector3 pos = m_target + Vector3(x, y, z);
    m_camera->SetPosition(pos);
    m_camera->SetTarget(m_target);
}

void OrbitCameraController::SetDistance(float d) {
    m_dist = clamp(d, m_minDist, m_maxDist);
    UpdatePosition();
}

}