#include "InputSystem.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace Moon {

InputSystem::InputSystem()
    : m_mousePosition(0.0f, 0.0f), m_previousMousePosition(0.0f, 0.0f), m_scrollDelta(0.0f, 0.0f) {}

bool InputSystem::IsKeyDown(KeyCode key) const {
#ifdef _WIN32
    return (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
#else
    return false;
#endif
}

bool InputSystem::IsKeyUp(KeyCode key) const {
    return !IsKeyDown(key);
}

bool InputSystem::IsKeyPressed(KeyCode key) const {
    int k = static_cast<int>(key);
    return m_currentKeys.find(k) != m_currentKeys.end() && m_previousKeys.find(k) == m_previousKeys.end();
}

bool InputSystem::IsKeyReleased(KeyCode key) const {
    int k = static_cast<int>(key);
    return m_currentKeys.find(k) == m_currentKeys.end() && m_previousKeys.find(k) != m_previousKeys.end();
}

bool InputSystem::IsMouseButtonDown(MouseButton button) const {
#ifdef _WIN32
    int vk = 0;
    switch (button) {
        case MouseButton::Left: vk = VK_LBUTTON; break;
        case MouseButton::Right: vk = VK_RBUTTON; break;
        case MouseButton::Middle: vk = VK_MBUTTON; break;
        case MouseButton::X1: vk = VK_XBUTTON1; break;
        case MouseButton::X2: vk = VK_XBUTTON2; break;
        default: return false;
    }
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
#else
    return false;
#endif
}

bool InputSystem::IsMouseButtonUp(MouseButton button) const {
    return !IsMouseButtonDown(button);
}

bool InputSystem::IsMouseButtonPressed(MouseButton button) const {
    int b = static_cast<int>(button);
    return m_currentButtons.find(b) != m_currentButtons.end() && m_previousButtons.find(b) == m_previousButtons.end();
}

bool InputSystem::IsMouseButtonReleased(MouseButton button) const {
    int b = static_cast<int>(button);
    return m_currentButtons.find(b) == m_currentButtons.end() && m_previousButtons.find(b) != m_previousButtons.end();
}

Vector2 InputSystem::GetMousePosition() const {
    return m_mousePosition;
}

Vector2 InputSystem::GetMouseDelta() const {
    return m_mousePosition - m_previousMousePosition;
}

Vector2 InputSystem::GetMouseScrollDelta() const {
    return m_scrollDelta;
}

void InputSystem::Update() {
    m_previousKeys = m_currentKeys;
    m_currentKeys.clear();
    m_previousButtons = m_currentButtons;
    m_currentButtons.clear();
    m_previousMousePosition = m_mousePosition;
#ifdef _WIN32
    POINT p;
    if (GetCursorPos(&p)) {
        // Convert screen coordinates to client area coordinates if window handle is set
        if (m_hWnd && ScreenToClient(static_cast<HWND>(m_hWnd), &p)) {
            m_mousePosition.x = static_cast<float>(p.x);
            m_mousePosition.y = static_cast<float>(p.y);
        } else {
            // Fallback to screen coordinates
            m_mousePosition.x = static_cast<float>(p.x);
            m_mousePosition.y = static_cast<float>(p.y);
        }
    }
#endif
    m_scrollDelta = Vector2(0.0f, 0.0f);
}

#ifdef _WIN32
void InputSystem::SetWindowHandle(void* hwnd) {
    m_hWnd = hwnd;
}
#endif

}