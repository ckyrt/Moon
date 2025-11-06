#include "FPSCameraController.h"
#include "../Input/KeyCodes.h"
#include <cmath>

namespace Moon {
namespace { constexpr float PI = 3.14159265358979323846f; }

FPSCameraController::FPSCameraController(Camera* cam, IInputSystem* input)
    : m_camera(cam), m_input(input), m_speed(5), m_sprint(2), m_sens(0.15f),
      m_yaw(0), m_pitch(0), m_enabled(true), m_firstMouse(true) {
    // Initialize yaw and pitch from current camera direction
    if (m_camera) {
        Vector3 forward = m_camera->GetForward();
        // Calculate yaw (horizontal rotation around Y axis)
        m_yaw = std::atan2(forward.x, forward.z);
        // Calculate pitch (vertical rotation)
        float horizontalLen = std::sqrt(forward.x * forward.x + forward.z * forward.z);
        m_pitch = std::atan2(forward.y, horizontalLen);
    }
}

void FPSCameraController::Update(float dt) {
    if (!m_enabled) return;
    ProcessKeyboard(dt);
    ProcessMouse(dt);
}

void FPSCameraController::ProcessKeyboard(float dt) {
    if (!m_camera || !m_input) return;
    
    // Unity-style: WASD only works when right mouse button is held
    if (!m_input->IsMouseButtonDown(MouseButton::Right)) {
        return;
    }
    
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
    
    // Only rotate when right mouse button is held
    if (!m_input->IsMouseButtonDown(MouseButton::Right)) { 
        m_firstMouse = true; 
        return; 
    }
    
    // Get current mouse position
    Vector2 currentPos = m_input->GetMousePosition();
    
    // Skip the first frame to avoid jump
    if (m_firstMouse) { 
        m_lastMouse = currentPos;
        m_firstMouse = false; 
        return; 
    }
    
    // Calculate delta manually
    Vector2 delta = currentPos - m_lastMouse;
    m_lastMouse = currentPos;
    
    // Apply mouse delta to yaw and pitch
    // Formula: rotation = delta_pixels * sensitivity * scale_factor
    // sensitivity range: 1.0 (slow) to 10.0 (fast)
    // scale_factor: 0.0001 makes 1 pixel move = sensitivity * 0.0001 radians
    float scale = 0.0001f;
    m_yaw += delta.x * m_sens * scale;
    m_pitch -= delta.y * m_sens * scale;
    
    // Clamp pitch to avoid gimbal lock
    constexpr float maxP = PI * 0.49f;
    if (m_pitch > maxP) m_pitch = maxP;
    if (m_pitch < -maxP) m_pitch = -maxP;
    
    UpdateOrientation();
}

void FPSCameraController::UpdateOrientation() {
    // Calculate forward vector from yaw and pitch
    // In left-handed coordinate system: X=right, Y=up, Z=forward
    float cy = std::cos(m_yaw);
    float sy = std::sin(m_yaw);
    float cp = std::cos(m_pitch);
    float sp = std::sin(m_pitch);
    
    // Forward vector in left-handed system
    Vector3 fwd(
        sy * cp,  // X component
        sp,       // Y component (pitch)
        cy * cp   // Z component
    );
    
    m_camera->SetTarget(m_camera->GetPosition() + fwd);
}

}