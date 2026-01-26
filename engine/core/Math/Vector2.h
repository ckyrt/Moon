#pragma once
#include <cmath>

namespace Moon {

    struct Vector2 {
        float x, y;

        Vector2() : x(0), y(0) {}
        Vector2(float x, float y) : x(x), y(y) {}

        Vector2 operator+(const Vector2& o) const {
            return Vector2(x + o.x, y + o.y);
        }

        Vector2 operator-(const Vector2& o) const {
            return Vector2(x - o.x, y - o.y);
        }

        Vector2 operator*(float s) const {
            return Vector2(x * s, y * s);
        }

        bool operator==(const Vector2& o) const {
            return x == o.x && y == o.y;
        }

        bool operator!=(const Vector2& o) const {
            return !(*this == o);
        }

        Vector2 operator-() const {
            return { -x, -y };
        }

        float Length() const {
            return std::sqrt(x * x + y * y);
        }

        Vector2 Normalized() const {
            float l = Length();
            return l > 0 ? Vector2(x / l, y / l) : *this;
        }

        static float Dot(const Vector2& a, const Vector2& b) {
            return a.x * b.x + a.y * b.y;
        }
    };

} // namespace Moon
