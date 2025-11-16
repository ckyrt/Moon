#pragma once
#include <cmath>

namespace Moon {

struct Vector3 {
    float x, y, z;
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    Vector3 operator+(const Vector3& o) const { return Vector3(x+o.x, y+o.y, z+o.z); }
    Vector3 operator-(const Vector3& o) const { return Vector3(x-o.x, y-o.y, z-o.z); }
    Vector3 operator*(float s) const { return Vector3(x*s, y*s, z*s); }
    float Length() const { return std::sqrt(x*x + y*y + z*z); }
    Vector3 Normalized() const { float l = Length(); return l > 0 ? Vector3(x/l, y/l, z/l) : *this; }
    static Vector3 Cross(const Vector3& a, const Vector3& b) {
        return Vector3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
    }
    static float Dot(const Vector3& a, const Vector3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
};

struct Matrix4x4 {
    float m[4][4];
    Matrix4x4() { for(int i=0; i<4; i++) for(int j=0; j<4; j++) m[i][j] = (i==j) ? 1.0f : 0.0f; }
    
    // Matrix multiplication
    Matrix4x4 operator*(const Matrix4x4& other) const {
        Matrix4x4 result;
        for(int i = 0; i < 4; i++) {
            for(int j = 0; j < 4; j++) {
                result.m[i][j] = 0.0f;
                for(int k = 0; k < 4; k++) {
                    result.m[i][j] += m[i][k] * other.m[k][j];
                }
            }
        }
        return result;
    }
    
    static Matrix4x4 LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up) {
        Vector3 z = (target - eye).Normalized();
        Vector3 x = Vector3::Cross(up, z).Normalized();
        Vector3 y = Vector3::Cross(z, x);
        Matrix4x4 r;
        r.m[0][0]=x.x; r.m[0][1]=y.x; r.m[0][2]=z.x; r.m[0][3]=0;
        r.m[1][0]=x.y; r.m[1][1]=y.y; r.m[1][2]=z.y; r.m[1][3]=0;
        r.m[2][0]=x.z; r.m[2][1]=y.z; r.m[2][2]=z.z; r.m[2][3]=0;
        r.m[3][0]=-Vector3::Dot(x,eye); r.m[3][1]=-Vector3::Dot(y,eye); r.m[3][2]=-Vector3::Dot(z,eye); r.m[3][3]=1;
        return r;
    }
    static Matrix4x4 PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ) {
        float h = 1.0f / std::tan(fovY * 0.5f), w = h / aspect;
        Matrix4x4 r;
        r.m[0][0]=w; r.m[1][1]=h; r.m[2][2]=farZ/(farZ-nearZ); r.m[2][3]=1.0f;
        r.m[3][2]=-nearZ*farZ/(farZ-nearZ); r.m[3][3]=0.0f;
        return r;
    }
    static Matrix4x4 OrthoLH(float w, float h, float nearZ, float farZ) {
        Matrix4x4 r;
        r.m[0][0]=2.0f/w; r.m[1][1]=2.0f/h; r.m[2][2]=1.0f/(farZ-nearZ); r.m[3][2]=-nearZ/(farZ-nearZ);
        return r;
    }
    static Matrix4x4 RotationY(float angle) {
        Matrix4x4 r;
        float c = std::cos(angle), s = std::sin(angle);
        r.m[0][0]=c; r.m[0][2]=-s; r.m[2][0]=s; r.m[2][2]=c;
        return r;
    }
    static Matrix4x4 Translation(float x, float y, float z) {
        Matrix4x4 r;
        r.m[3][0]=x; r.m[3][1]=y; r.m[3][2]=z;
        return r;
    }
    Vector3 MultiplyPoint(const Vector3& v) const
    {
        Vector3 r;
        r.x = v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + m[3][0];
        r.y = v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + m[3][1];
        r.z = v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + m[3][2];
        return r;
    }
    Matrix4x4 Inverse() const
    {
        Matrix4x4 inv;

        const float* src = (const float*)this->m;
        float* dst = (float*)inv.m;

        dst[0] = src[5] * src[10] * src[15] -
            src[5] * src[11] * src[14] -
            src[9] * src[6] * src[15] +
            src[9] * src[7] * src[14] +
            src[13] * src[6] * src[11] -
            src[13] * src[7] * src[10];

        dst[4] = -src[4] * src[10] * src[15] +
            src[4] * src[11] * src[14] +
            src[8] * src[6] * src[15] -
            src[8] * src[7] * src[14] -
            src[12] * src[6] * src[11] +
            src[12] * src[7] * src[10];

        dst[8] = src[4] * src[9] * src[15] -
            src[4] * src[11] * src[13] -
            src[8] * src[5] * src[15] +
            src[8] * src[7] * src[13] +
            src[12] * src[5] * src[11] -
            src[12] * src[7] * src[9];

        dst[12] = -src[4] * src[9] * src[14] +
            src[4] * src[10] * src[13] +
            src[8] * src[5] * src[14] -
            src[8] * src[6] * src[13] -
            src[12] * src[5] * src[10] +
            src[12] * src[6] * src[9];

        dst[1] = -src[1] * src[10] * src[15] +
            src[1] * src[11] * src[14] +
            src[9] * src[2] * src[15] -
            src[9] * src[3] * src[14] -
            src[13] * src[2] * src[11] +
            src[13] * src[3] * src[10];

        dst[5] = src[0] * src[10] * src[15] -
            src[0] * src[11] * src[14] -
            src[8] * src[2] * src[15] +
            src[8] * src[3] * src[14] +
            src[12] * src[2] * src[11] -
            src[12] * src[3] * src[10];

        dst[9] = -src[0] * src[9] * src[15] +
            src[0] * src[11] * src[13] +
            src[8] * src[1] * src[15] -
            src[8] * src[3] * src[13] -
            src[12] * src[1] * src[11] +
            src[12] * src[3] * src[9];

        dst[13] = src[0] * src[9] * src[14] -
            src[0] * src[10] * src[13] -
            src[8] * src[1] * src[14] +
            src[8] * src[2] * src[13] +
            src[12] * src[1] * src[10] -
            src[12] * src[2] * src[9];

        dst[2] = src[1] * src[6] * src[15] -
            src[1] * src[7] * src[14] -
            src[5] * src[2] * src[15] +
            src[5] * src[3] * src[14] +
            src[13] * src[2] * src[7] -
            src[13] * src[3] * src[6];

        dst[6] = -src[0] * src[6] * src[15] +
            src[0] * src[7] * src[14] +
            src[4] * src[2] * src[15] -
            src[4] * src[3] * src[14] -
            src[12] * src[2] * src[7] +
            src[12] * src[3] * src[6];

        dst[10] = src[0] * src[5] * src[15] -
            src[0] * src[7] * src[13] -
            src[4] * src[1] * src[15] +
            src[4] * src[3] * src[13] +
            src[12] * src[1] * src[7] -
            src[12] * src[3] * src[5];

        dst[14] = -src[0] * src[5] * src[14] +
            src[0] * src[6] * src[13] +
            src[4] * src[1] * src[14] -
            src[4] * src[2] * src[13] -
            src[12] * src[1] * src[6] +
            src[12] * src[2] * src[5];

        dst[3] = -src[1] * src[6] * src[11] +
            src[1] * src[7] * src[10] +
            src[5] * src[2] * src[11] -
            src[5] * src[3] * src[10] -
            src[9] * src[2] * src[7] +
            src[9] * src[3] * src[6];

        dst[7] = src[0] * src[6] * src[11] -
            src[0] * src[7] * src[10] -
            src[4] * src[2] * src[11] +
            src[4] * src[3] * src[10] +
            src[8] * src[2] * src[7] -
            src[8] * src[3] * src[6];

        dst[11] = -src[0] * src[5] * src[11] +
            src[0] * src[7] * src[9] +
            src[4] * src[1] * src[11] -
            src[4] * src[3] * src[9] -
            src[8] * src[1] * src[7] +
            src[8] * src[3] * src[5];

        dst[15] = src[0] * src[5] * src[10] -
            src[0] * src[6] * src[9] -
            src[4] * src[1] * src[10] +
            src[4] * src[2] * src[9] +
            src[8] * src[1] * src[6] -
            src[8] * src[2] * src[5];

        float det = src[0] * dst[0] + src[4] * dst[1] + src[8] * dst[2] + src[12] * dst[3];

        if (det == 0.0f)
            return Matrix4x4();  // identity fallback

        det = 1.0f / det;
        for (int i = 0; i < 16; i++)
            dst[i] *= det;

        return inv;
    }

};

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

class ICamera {
public:
    virtual ~ICamera() = default;
    virtual void SetPosition(const Vector3& p) = 0;
    virtual void SetTarget(const Vector3& t) = 0;
    virtual void SetUp(const Vector3& u) = 0;
    virtual Vector3 GetPosition() const = 0;
    virtual Vector3 GetTarget() const = 0;
    virtual Vector3 GetUp() const = 0;
    virtual Vector3 GetForward() const = 0;
    virtual Vector3 GetRight() const = 0;
    virtual Matrix4x4 GetViewMatrix() const = 0;
    virtual Matrix4x4 GetProjectionMatrix() const = 0;
    virtual Matrix4x4 GetViewProjectionMatrix() const = 0;
};

class Camera : public ICamera {
public:
    Camera();
    virtual ~Camera() = default;
    void SetPosition(const Vector3& p) override;
    void SetTarget(const Vector3& t) override;
    void SetUp(const Vector3& u) override;
    Vector3 GetPosition() const override { return m_position; }
    Vector3 GetTarget() const override { return m_target; }
    Vector3 GetUp() const override { return m_up; }
    Vector3 GetForward() const override;
    Vector3 GetRight() const override;
    Matrix4x4 GetViewMatrix() const override;
    virtual Matrix4x4 GetProjectionMatrix() const override = 0;
    Matrix4x4 GetViewProjectionMatrix() const override;
    
    // Helper method for easier camera positioning
    void LookAt(const Vector3& target);
    
protected:
    Vector3 m_position, m_target, m_up;
    mutable bool m_viewDirty = true;
    mutable bool m_projDirty = true;
    mutable Matrix4x4 m_cachedView;
    mutable Matrix4x4 m_cachedProj;
};

}