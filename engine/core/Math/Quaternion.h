#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"
#include <cmath>

namespace Moon
{

struct Quaternion
{
    float x;
    float y;
    float z;
    float w;

    //////////////////////////////////////////////////////////////
    // Constructors
    //////////////////////////////////////////////////////////////

    Quaternion()
        : x(0), y(0), z(0), w(1) {}

    Quaternion(float x, float y, float z, float w)
        : x(x), y(y), z(z), w(w) {}

    static Quaternion Identity()
    {
        return Quaternion(0,0,0,1);
    }

    //////////////////////////////////////////////////////////////
    // Normalize
    //////////////////////////////////////////////////////////////

    Quaternion Normalized() const
    {
        float len = std::sqrt(x*x + y*y + z*z + w*w);

        if(len == 0)
            return Quaternion();

        float inv = 1.0f / len;

        return Quaternion(
            x * inv,
            y * inv,
            z * inv,
            w * inv
        );
    }

    //////////////////////////////////////////////////////////////
    // Conjugate / Inverse
    //////////////////////////////////////////////////////////////

    Quaternion Conjugate() const
    {
        return Quaternion(-x,-y,-z,w);
    }

    Quaternion Inverse() const
    {
        return Conjugate();
    }

    //////////////////////////////////////////////////////////////
    // Quaternion * Quaternion
    //////////////////////////////////////////////////////////////

    Quaternion operator*(const Quaternion& q) const
    {
        return Quaternion(
            w*q.x + x*q.w + y*q.z - z*q.y,
            w*q.y - x*q.z + y*q.w + z*q.x,
            w*q.z + x*q.y - y*q.x + z*q.w,
            w*q.w - x*q.x - y*q.y - z*q.z
        );
    }

    //////////////////////////////////////////////////////////////
    // Rotate Vector (fast version)
    //////////////////////////////////////////////////////////////

    Vector3 operator*(const Vector3& v) const
    {
        Vector3 qv(x,y,z);

        Vector3 t = Vector3::Cross(qv,v) * 2.0f;

        return v + t * w + Vector3::Cross(qv,t);
    }

    Vector3 ToEuler() const
    {
        const float RAD2DEG = 180.0f / 3.14159265358979323846f;

        Vector3 euler;

        // YXZ order

        float test = 2.0f * (w * x - y * z);

        if (test > 0.9999f)
        {
            // singularity north pole
            euler.y = 2.0f * std::atan2(y, w);
            euler.x = 3.14159265358979323846f / 2;
            euler.z = 0;
        }
        else if (test < -0.9999f)
        {
            // singularity south pole
            euler.y = -2.0f * std::atan2(y, w);
            euler.x = -3.14159265358979323846f / 2;
            euler.z = 0;
        }
        else
        {
            euler.x = std::asin(test);

            euler.y = std::atan2(
                2.0f * (w * y + z * x),
                1.0f - 2.0f * (x * x + y * y)
            );

            euler.z = std::atan2(
                2.0f * (w * z + x * y),
                1.0f - 2.0f * (x * x + z * z)
            );
        }

        euler.x *= RAD2DEG;
        euler.y *= RAD2DEG;
        euler.z *= RAD2DEG;

        return euler;
    }

    Matrix4x4 ToMatrix() const
    {
        Matrix4x4 m;

        // 预计算，提高效率
        float xx = x * x;
        float yy = y * y;
        float zz = z * z;

        float xy = x * y;
        float xz = x * z;
        float yz = y * z;

        float wx = w * x;
        float wy = w * y;
        float wz = w * z;

        //
        // 旋转矩阵 (Row-major)
        //

        m.m[0][0] = 1.0f - 2.0f * (yy + zz);
        m.m[0][1] = 2.0f * (xy + wz);
        m.m[0][2] = 2.0f * (xz - wy);
        m.m[0][3] = 0.0f;

        m.m[1][0] = 2.0f * (xy - wz);
        m.m[1][1] = 1.0f - 2.0f * (xx + zz);
        m.m[1][2] = 2.0f * (yz + wx);
        m.m[1][3] = 0.0f;

        m.m[2][0] = 2.0f * (xz + wy);
        m.m[2][1] = 2.0f * (yz - wx);
        m.m[2][2] = 1.0f - 2.0f * (xx + yy);
        m.m[2][3] = 0.0f;

        // 最后一行保持齐次坐标
        m.m[3][0] = 0.0f;
        m.m[3][1] = 0.0f;
        m.m[3][2] = 0.0f;
        m.m[3][3] = 1.0f;

        return m;
    }

    //////////////////////////////////////////////////////////////
    // Euler (degrees)
    // Rotation order : YXZ (Unity style)
    //////////////////////////////////////////////////////////////

    static Quaternion Euler(const Vector3& eulerDeg)
    {
        const float DEG2RAD = 3.14159265358979323846f / 180.0f;

        float xRad = eulerDeg.x * DEG2RAD;
        float yRad = eulerDeg.y * DEG2RAD;
        float zRad = eulerDeg.z * DEG2RAD;

        float cx = std::cos(xRad * 0.5f);
        float sx = std::sin(xRad * 0.5f);

        float cy = std::cos(yRad * 0.5f);
        float sy = std::sin(yRad * 0.5f);

        float cz = std::cos(zRad * 0.5f);
        float sz = std::sin(zRad * 0.5f);

        Quaternion q;

        q.x = sx*cy*cz + cx*sy*sz;
        q.y = cx*sy*cz - sx*cy*sz;
        q.z = cx*cy*sz - sx*sy*cz;
        q.w = cx*cy*cz + sx*sy*sz;

        return q;
    }

    //////////////////////////////////////////////////////////////
    // LookRotation
    //////////////////////////////////////////////////////////////

    static Quaternion LookRotation(
        const Vector3& forward,
        const Vector3& up = Vector3(0,1,0))
    {
        Vector3 f = forward.Normalized();

        Vector3 r = Vector3::Cross(up,f);

        if(r.Length() < 0.000001f)
        {
            r = Vector3::Cross(Vector3(0,1,0),f);
        }

        r = r.Normalized();

        Vector3 u = Vector3::Cross(f,r);

        Matrix4x4 m;

        m.m[0][0] = r.x;
        m.m[0][1] = r.y;
        m.m[0][2] = r.z;

        m.m[1][0] = u.x;
        m.m[1][1] = u.y;
        m.m[1][2] = u.z;

        m.m[2][0] = f.x;
        m.m[2][1] = f.y;
        m.m[2][2] = f.z;

        return FromMatrix(m);
    }

    //////////////////////////////////////////////////////////////
    // Matrix -> Quaternion
    //////////////////////////////////////////////////////////////

    static Quaternion FromMatrix(const Matrix4x4& m)
    {
        float trace =
            m.m[0][0] +
            m.m[1][1] +
            m.m[2][2];

        if(trace > 0)
        {
            float s = std::sqrt(trace + 1.0f) * 2.0f;

            return Quaternion(
                (m.m[2][1]-m.m[1][2]) / s,
                (m.m[0][2]-m.m[2][0]) / s,
                (m.m[1][0]-m.m[0][1]) / s,
                0.25f * s
            );
        }

        if(m.m[0][0] > m.m[1][1] &&
           m.m[0][0] > m.m[2][2])
        {
            float s = std::sqrt(
                1.0f +
                m.m[0][0] -
                m.m[1][1] -
                m.m[2][2]) * 2.0f;

            return Quaternion(
                0.25f*s,
                (m.m[0][1]+m.m[1][0])/s,
                (m.m[0][2]+m.m[2][0])/s,
                (m.m[2][1]-m.m[1][2])/s
            );
        }
        else if(m.m[1][1] > m.m[2][2])
        {
            float s = std::sqrt(
                1.0f +
                m.m[1][1] -
                m.m[0][0] -
                m.m[2][2]) * 2.0f;

            return Quaternion(
                (m.m[0][1]+m.m[1][0])/s,
                0.25f*s,
                (m.m[1][2]+m.m[2][1])/s,
                (m.m[0][2]-m.m[2][0])/s
            );
        }
        else
        {
            float s = std::sqrt(
                1.0f +
                m.m[2][2] -
                m.m[0][0] -
                m.m[1][1]) * 2.0f;

            return Quaternion(
                (m.m[0][2]+m.m[2][0])/s,
                (m.m[1][2]+m.m[2][1])/s,
                0.25f*s,
                (m.m[1][0]-m.m[0][1])/s
            );
        }
    }

};

}