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
};

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