#pragma once

#include "KeyCodes.h"
#include "InputTypes.h"

namespace Moon {

class IInputSystem {
public:
    virtual ~IInputSystem() = default;
    virtual bool IsKeyDown(KeyCode key) const = 0;
    virtual bool IsKeyUp(KeyCode key) const = 0;
    virtual bool IsKeyPressed(KeyCode key) const = 0;
    virtual bool IsKeyReleased(KeyCode key) const = 0;
    virtual bool IsMouseButtonDown(MouseButton button) const = 0;
    virtual bool IsMouseButtonUp(MouseButton button) const = 0;
    virtual bool IsMouseButtonPressed(MouseButton button) const = 0;
    virtual bool IsMouseButtonReleased(MouseButton button) const = 0;
    virtual Vector2 GetMousePosition() const = 0;
    virtual Vector2 GetMouseDelta() const = 0;
    virtual Vector2 GetMouseScrollDelta() const = 0;
    virtual void Update() = 0;
};

}