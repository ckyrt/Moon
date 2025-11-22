#pragma once
#include "Vector3.h"
#include <cmath>
#include "../Logging/Logger.h"

namespace Moon {

//
// Matrix4x4
// ----------
// 用于 3D 图形计算的 4×4 矩阵类，采用 **左手坐标系 (Left-Handed)** 约定，
// 并使用 **行向量(row-vector) × 矩阵** 的乘法顺序。
// 内部存储为 m[row][column]，即按行存储的 4×4 矩阵。
//
// 功能概述：
//   - 提供基础的矩阵乘法、点变换、逆矩阵计算。
//   - 提供常用的图形变换矩阵构造函数，如：
//       * LookAtLH       —— 左手系相机视图矩阵
//       * PerspectiveFovLH —— 左手系透视投影矩阵
//       * OrthoLH        —— 左手系正交投影矩阵
//       * RotationY      —— 绕 Y 轴旋转
//       * Translation    —— 平移矩阵
//
// 注意事项：
//   1. MultiplyPoint 假定输入点的 w = 1（齐次坐标中常用于位置变换）。
//   2. Inverse() 使用通用 4×4 矩阵求逆算法，适用于任意非奇异矩阵。
//   3. 该类默认构造为 **单位矩阵**。
//   4. 矩阵相乘遵循 M_result = M_this × M_other（保持图形学中的常用顺序）。
//
// 适用范围：
//   - 游戏引擎中的相机、物体变换、投影矩阵计算。
//   - 任何基于左手坐标系的渲染流程（如 DirectX 风格）。
//
//

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
        // 行向量乘法: [x, y, z, 1] × M
        // x' = x * m[0][0] + y * m[1][0] + z * m[2][0] + 1 * m[3][0]
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

    void Print(const char* name = nullptr) const
    {
        if (name)
            MOON_LOG_INFO("Matrix", "%s:", name);

        MOON_LOG_INFO(
            "Matrix",
            "[%.3f, %.3f, %.3f, %.3f]\n"
            "[%.3f, %.3f, %.3f, %.3f]\n"
            "[%.3f, %.3f, %.3f, %.3f]\n"
            "[%.3f, %.3f, %.3f, %.3f]",
            m[0][0], m[0][1], m[0][2], m[0][3],
            m[1][0], m[1][1], m[1][2], m[1][3],
            m[2][0], m[2][1], m[2][2], m[2][3],
            m[3][0], m[3][1], m[3][2], m[3][3]
        );

    }
};

} // namespace Moon
