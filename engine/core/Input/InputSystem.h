#pragma once

#include "IInputSystem.h"
#include <unordered_set>

namespace Moon {

class InputSystem : public IInputSystem {
public:
    InputSystem();
    ~InputSystem() override = default;
    bool IsKeyDown(KeyCode key) const override;
    bool IsKeyUp(KeyCode key) const override;
    bool IsKeyPressed(KeyCode key) const override;
    bool IsKeyReleased(KeyCode key) const override;
    bool IsMouseButtonDown(MouseButton button) const override;
    bool IsMouseButtonUp(MouseButton button) const override;
    bool IsMouseButtonPressed(MouseButton button) const override;
    bool IsMouseButtonReleased(MouseButton button) const override;
    Vector2 GetMousePosition() const override;
    Vector2 GetMouseDelta() const override;
    Vector2 GetMouseScrollDelta() const override;
    void Update() override;
    
    // Set window handle for proper mouse coordinate conversion
#ifdef _WIN32
    void SetWindowHandle(void* hwnd);
#endif

private:
    std::unordered_set<int> m_currentKeys;
    std::unordered_set<int> m_previousKeys;
    std::unordered_set<int> m_currentButtons;
    std::unordered_set<int> m_previousButtons;
    Vector2 m_mousePosition;
    Vector2 m_previousMousePosition;
    Vector2 m_scrollDelta;
#ifdef _WIN32
    void* m_hWnd = nullptr;
#endif
};

}