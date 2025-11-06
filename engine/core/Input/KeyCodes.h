#pragma once

namespace Moon {

enum class KeyCode {
    A = 0x41, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    D0 = 0x30, D1, D2, D3, D4, D5, D6, D7, D8, D9,
    F1 = 0x70, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Escape = 0x1B, Tab = 0x09, CapsLock = 0x14,
    LeftShift = 0xA0, RightShift = 0xA1,
    LeftControl = 0xA2, RightControl = 0xA3,
    LeftAlt = 0xA4, RightAlt = 0xA5,
    Space = 0x20, Enter = 0x0D, Backspace = 0x08,
    Left = 0x25, Up = 0x26, Right = 0x27, Down = 0x28,
    Insert = 0x2D, Delete = 0x2E, Home = 0x24, End = 0x23,
    PageUp = 0x21, PageDown = 0x22,
    Numpad0 = 0x60, Numpad1 = 0x61, Numpad2 = 0x62, Numpad3 = 0x63,
    Numpad4 = 0x64, Numpad5 = 0x65, Numpad6 = 0x66, Numpad7 = 0x67,
    Numpad8 = 0x68, Numpad9 = 0x69, NumpadMultiply = 0x6A, NumpadAdd = 0x6B,
    NumpadSubtract = 0x6D, NumpadDecimal = 0x6E, NumpadDivide = 0x6F,
    Minus = 0xBD, Equal = 0xBB, LeftBracket = 0xDB, RightBracket = 0xDD,
    Backslash = 0xDC, Semicolon = 0xBA, Apostrophe = 0xDE,
    Comma = 0xBC, Period = 0xBE, Slash = 0xBF, Grave = 0xC0,
    PrintScreen = 0x2C, ScrollLock = 0x91, Pause = 0x13,
    LeftSuper = 0x5B, RightSuper = 0x5C,
    Unknown = 0
};

enum class MouseButton {
    Left = 0, Right = 1, Middle = 2, X1 = 3, X2 = 4, Unknown = -1
};

}