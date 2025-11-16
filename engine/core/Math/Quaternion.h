#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"
#include <cmath>

namespace Moon {

struct Quaternion
{
    float x, y, z, w;

    Quaternion();
    Quaternion(float x, float y, float z, float w);

    static Quaternion Identity();

    // --- Euler (degrees) ---
    static Quaternion Euler(const Vector3& eulerDeg);
    Vector3 ToEuler() const;    // <--- �Ҳ���� Unity ��Ч ToEuler

    // --- LookRotation ---
    static Quaternion LookRotation(const Vector3& forward, const Vector3& up);

    // --- Matrix conversion ---
    Matrix4x4 ToMatrix() const;
    static Quaternion FromMatrix(const Matrix4x4& m);

    // --- Operators ---
    Quaternion Inverse() const;
    Quaternion operator*(const Quaternion& q) const;
    Vector3 operator*(const Vector3& v) const; // rotate vector
};

/////////////////////////
// IMPLEMENTATION
/////////////////////////

inline Quaternion::Quaternion() : x(0), y(0), z(0), w(1) {}
inline Quaternion::Quaternion(float x, float y, float z, float w)
    : x(x), y(y), z(z), w(w) {
}

inline Quaternion Quaternion::Identity()
{
    return Quaternion(0, 0, 0, 1);
}

static const float DEG2RAD = 3.14159265358979323846f / 180.0f;
static const float RAD2DEG = 180.0f / 3.14159265358979323846f;

/////////////////////////////////
// Euler �� Quaternion (Unity)
/////////////////////////////////
inline Quaternion Quaternion::Euler(const Vector3& eulerDeg)
{
    float pitch = eulerDeg.x * DEG2RAD; // X
    float yaw = eulerDeg.y * DEG2RAD; // Y
    float roll = eulerDeg.z * DEG2RAD; // Z

    float cy = std::cos(yaw * 0.5f);
    float sy = std::sin(yaw * 0.5f);
    float cp = std::cos(pitch * 0.5f);
    float sp = std::sin(pitch * 0.5f);
    float cr = std::cos(roll * 0.5f);
    float sr = std::sin(roll * 0.5f);

    Quaternion q;
    q.w = cr * cp * cy + sr * sp * sy;
    q.x = sr * cp * cy - cr * sp * sy;
    q.y = cr * sp * cy + sr * cp * sy;
    q.z = cr * cp * sy - sr * sp * cy;
    return q;
}

/////////////////////////////////
// Quaternion �� Euler (Unity!)
/////////////////////////////////
inline Vector3 Quaternion::ToEuler() const
{
    Vector3 e;

    // ----- pitch -----
    float sinp = 2.0f * (w * x + y * z);
    float cosp = 1.0f - 2.0f * (x * x + y * y);
    e.x = std::atan2(sinp, cosp);

    // ----- yaw -----
    float siny = 2.0f * (w * y - z * x);
    if (std::fabs(siny) >= 1.0f)
        e.y = std::copysign(3.14159265358979f / 2, siny);
    else
        e.y = std::asin(siny);

    // ----- roll -----
    float sinr = 2.0f * (w * z + x * y);
    float cosr = 1.0f - 2.0f * (y * y + z * z);
    e.z = std::atan2(sinr, cosr);

    // תΪ�Ƕ�
    e.x *= RAD2DEG;
    e.y *= RAD2DEG;
    e.z *= RAD2DEG;
    return e;
}

/////////////////////////////////
// LookRotation (Unity LH)
/////////////////////////////////
inline Quaternion Quaternion::LookRotation(const Vector3& forward, const Vector3& up)
{
    Vector3 f = forward.Normalized();
    Vector3 r = Vector3::Cross(up, f).Normalized();
    Vector3 u = Vector3::Cross(f, r);

    Matrix4x4 m;

    m.m[0][0] = r.x; m.m[0][1] = r.y; m.m[0][2] = r.z;
    m.m[1][0] = u.x; m.m[1][1] = u.y; m.m[1][2] = u.z;
    m.m[2][0] = f.x; m.m[2][1] = f.y; m.m[2][2] = f.z;

    return FromMatrix(m);
}

/////////////////////////////////
// Quaternion �� Quaternion
/////////////////////////////////
inline Quaternion Quaternion::operator*(const Quaternion& q) const
{
    return Quaternion(
        w * q.x + x * q.w + y * q.z - z * q.y,
        w * q.y - x * q.z + y * q.w + z * q.x,
        w * q.z + x * q.y - y * q.x + z * q.w,
        w * q.w - x * q.x - y * q.y - z * q.z
    );
}

/////////////////////////////////
// Rotate Vector by Quaternion
/////////////////////////////////
inline Vector3 Quaternion::operator*(const Vector3& v) const
{
    Quaternion qv(v.x, v.y, v.z, 0);
    Quaternion inv = Inverse();
    Quaternion r = (*this) * qv * inv;
    return Vector3(r.x, r.y, r.z);
}

/////////////////////////////////
// Inverse
/////////////////////////////////
inline Quaternion Quaternion::Inverse() const
{
    float len = x * x + y * y + z * z + w * w;
    if (len == 0) return Quaternion(0, 0, 0, 1);

    float inv = 1.0f / len;
    return Quaternion(-x * inv, -y * inv, -z * inv, w * inv);
}

/////////////////////////////////
// From Matrix
/////////////////////////////////
inline Quaternion Quaternion::FromMatrix(const Matrix4x4& m)
{
    float trace = m.m[0][0] + m.m[1][1] + m.m[2][2];

    if (trace > 0)
    {
        float s = std::sqrt(trace + 1.0f) * 2.0f;
        float inv = 1.0f / s;
        return Quaternion(
            (m.m[2][1] - m.m[1][2]) * inv,
            (m.m[0][2] - m.m[2][0]) * inv,
            (m.m[1][0] - m.m[0][1]) * inv,
            0.25f * s
        );
    }
    else
    {
        if (m.m[0][0] > m.m[1][1] && m.m[0][0] > m.m[2][2])
        {
            float s = std::sqrt(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]) * 2.0f;
            float inv = 1.0f / s;
            return Quaternion(
                0.25f * s,
                (m.m[0][1] + m.m[1][0]) * inv,
                (m.m[0][2] + m.m[2][0]) * inv,
                (m.m[2][1] - m.m[1][2]) * inv
            );
        }
        else if (m.m[1][1] > m.m[2][2])
        {
            float s = std::sqrt(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]) * 2.0f;
            float inv = 1.0f / s;
            return Quaternion(
                (m.m[0][1] + m.m[1][0]) * inv,
                0.25f * s,
                (m.m[1][2] + m.m[2][1]) * inv,
                (m.m[0][2] - m.m[2][0]) * inv
            );
        }
        else
        {
            float s = std::sqrt(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]) * 2.0f;
            float inv = 1.0f / s;
            return Quaternion(
                (m.m[0][2] + m.m[2][0]) * inv,
                (m.m[1][2] + m.m[2][1]) * inv,
                0.25f * s,
                (m.m[1][0] - m.m[0][1]) * inv
            );
        }
    }
}

/////////////////////////////////
// To Matrix (rotation-only)
// ============================
inline Matrix4x4 Quaternion::ToMatrix() const
{
    Matrix4x4 m;

    float xx = x * x, yy = y * y, zz = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;

    m.m[0][0] = 1 - 2 * (yy + zz);
    m.m[0][1] = 2 * (xy + wz);
    m.m[0][2] = 2 * (xz - wy);

    m.m[1][0] = 2 * (xy - wz);
    m.m[1][1] = 1 - 2 * (xx + zz);
    m.m[1][2] = 2 * (yz + wx);

    m.m[2][0] = 2 * (xz + wy);
    m.m[2][1] = 2 * (yz - wx);
    m.m[2][2] = 1 - 2 * (xx + yy);

    m.m[3][3] = 1;
    return m;
}

} // namespace Moon
