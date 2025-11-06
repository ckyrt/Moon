#pragma once

namespace Moon {

struct Vector2 {
    float x, y;
    Vector2() : x(0.0f), y(0.0f) {}
    Vector2(float x, float y) : x(x), y(y) {}
    Vector2 operator+(const Vector2& o) const { return Vector2(x + o.x, y + o.y); }
    Vector2 operator-(const Vector2& o) const { return Vector2(x - o.x, y - o.y); }
    Vector2 operator*(float s) const { return Vector2(x * s, y * s); }
};

}