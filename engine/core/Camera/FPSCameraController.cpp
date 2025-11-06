#include "FPSCameraController.h"
#include "../Input/KeyCodes.h"
#include <cmath>

namespace Moon {
namespace { constexpr float PI = 3.14159265358979323846f; }

FPSCameraController::FPSCameraController(Camera* cam, IInputSystem* input)
    : m_camera(cam), m_input(input), m_speed(5), m_sprint(2), m_sens(0.002f),
      m_yaw(0), m_pitch(0), m_enabled(true), m_firstMouse(true) {}

void FPSCameraController::Update(float dt) {
    if (!m_enabled) return;
    ProcessKeyboard(dt);
    ProcessMouse(dt);
}

void FPSCameraController::ProcessKeyboard(float dt) {
    if (!m_camera || !m_input) return;
    float spd = m_speed;
    if (m_input->IsKeyDown(KeyCode::LeftShift) || m_input->IsKeyDown(KeyCode::RightShift)) spd *= m_sprint;
    float v = spd * dt;
    Vector3 pos = m_camera->GetPosition(), fwd = m_camera->GetForward(), rgt = m_camera->GetRight();
    if (m_input->IsKeyDown(KeyCode::W)) pos = pos + fwd * v;
    if (m_input->IsKeyDown(KeyCode::S)) pos = pos - fwd * v;
    if (m_input->IsKeyDown(KeyCode::A)) pos = pos - rgt * v;
    if (m_input->IsKeyDown(KeyCode::D)) pos = pos + rgt * v;
    if (m_input->IsKeyDown(KeyCode::E)) pos.y += v;
    if (m_input->IsKeyDown(KeyCode::Q)) pos.y -= v;
    m_camera->SetPosition(pos);
}

void FPSCameraController::ProcessMouse(float dt) {
    if (!m_camera || !m_input) return;
    if (!m_input->IsMouseButtonDown(MouseButton::Right)) { m_firstMouse = true; return; }
    Vector2 mp = m_input->GetMousePosition();
    if (m_firstMouse) { m_lastMouse = mp; m_firstMouse = false; return; }
    Vector2 delta = mp - m_lastMouse;
    m_lastMouse = mp;
    m_yaw += delta.x * m_sens;
    m_pitch -= delta.y * m_sens;
    constexpr float maxP = PI * 0.49f;
    if (m_pitch > maxP) m_pitch = maxP;
    if (m_pitch < -maxP) m_pitch = -maxP;
    UpdateOrientation();
}

void FPSCameraController::UpdateOrientation() {
    float cy = std::cos(m_yaw), sy = std::sin(m_yaw), cp = std::cos(m_pitch), sp = std::sin(m_pitch);
    Vector3 fwd(sy*cp, sp, cy*cp);
    m_camera->SetTarget(m_camera->GetPosition() + fwd);
}

}