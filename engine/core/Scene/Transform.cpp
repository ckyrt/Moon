#include "Transform.h"
#include "SceneNode.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Moon {

Transform::Transform(SceneNode* owner)
    : m_owner(owner)
    , m_localPosition(0.0f, 0.0f, 0.0f)
    , m_localRotation(0.0f, 0.0f, 0.0f)
    , m_localScale(1.0f, 1.0f, 1.0f)
    , m_localDirty(true)
    , m_worldDirty(true)
{
}

// === 本地变换 ===

void Transform::SetLocalPosition(const Vector3& position) {
    m_localPosition = position;
    MarkDirty();
}

void Transform::SetLocalRotation(const Vector3& euler) {
    m_localRotation = euler;
    MarkDirty();
}

void Transform::SetLocalScale(const Vector3& scale) {
    m_localScale = scale;
    MarkDirty();
}

// === 世界变换 ===

void Transform::SetWorldPosition(const Vector3& position) {
    SceneNode* parent = m_owner->GetParent();
    if (parent) {
        // TODO: 需要将世界位置转换为本地位置
        // 这需要父节点的逆矩阵
        // 暂时简化处理
        m_localPosition = position;
    } else {
        m_localPosition = position;
    }
    MarkDirty();
}

void Transform::SetWorldRotation(const Vector3& euler) {
    SceneNode* parent = m_owner->GetParent();
    if (parent) {
        // TODO: 需要将世界旋转转换为本地旋转
        m_localRotation = euler;
    } else {
        m_localRotation = euler;
    }
    MarkDirty();
}

Vector3 Transform::GetWorldPosition() {
    const Matrix4x4& worldMat = GetWorldMatrix();
    return Vector3(worldMat.m[3][0], worldMat.m[3][1], worldMat.m[3][2]);
}

Vector3 Transform::GetWorldRotation() {
    // TODO: 从世界矩阵提取 Euler 角
    // 暂时返回本地旋转
    return m_localRotation;
}

// === 方向向量 ===

Vector3 Transform::GetForward() {
    const Matrix4x4& worldMat = GetWorldMatrix();
    // 左手坐标系：+Z 是前方
    return Vector3(worldMat.m[2][0], worldMat.m[2][1], worldMat.m[2][2]).Normalized();
}

Vector3 Transform::GetRight() {
    const Matrix4x4& worldMat = GetWorldMatrix();
    // +X 是右方
    return Vector3(worldMat.m[0][0], worldMat.m[0][1], worldMat.m[0][2]).Normalized();
}

Vector3 Transform::GetUp() {
    const Matrix4x4& worldMat = GetWorldMatrix();
    // +Y 是上方
    return Vector3(worldMat.m[1][0], worldMat.m[1][1], worldMat.m[1][2]).Normalized();
}

// === 矩阵访问 ===

const Matrix4x4& Transform::GetLocalMatrix() {
    if (m_localDirty) {
        UpdateLocalMatrix();
        m_localDirty = false;
    }
    return m_localMatrix;
}

const Matrix4x4& Transform::GetWorldMatrix() {
    if (m_localDirty) {
        UpdateLocalMatrix();
        m_localDirty = false;
    }
    
    if (m_worldDirty) {
        UpdateWorldMatrix();
        m_worldDirty = false;
    }
    
    return m_worldMatrix;
}

// === 变换操作 ===

void Transform::Translate(const Vector3& offset, bool worldSpace) {
    if (worldSpace) {
        m_localPosition = m_localPosition + offset;
    } else {
        // 本地空间平移：需要考虑旋转
        Matrix4x4 rotMat = CreateRotationMatrix(m_localRotation);
        // 简化：暂时直接加到本地位置
        m_localPosition = m_localPosition + offset;
    }
    MarkDirty();
}

void Transform::Rotate(const Vector3& eulerAngles, bool worldSpace) {
    if (worldSpace) {
        // 世界空间旋转（复杂）
        m_localRotation = m_localRotation + eulerAngles;
    } else {
        m_localRotation = m_localRotation + eulerAngles;
    }
    MarkDirty();
}

void Transform::LookAt(const Vector3& target, const Vector3& up) {
    Vector3 worldPos = GetWorldPosition();
    Vector3 forward = (target - worldPos).Normalized();
    
    // TODO: 从方向向量计算 Euler 角
    // 这是一个复杂的数学问题，暂时简化
    
    MarkDirty();
}

// === 内部方法 ===

void Transform::MarkDirty() {
    m_localDirty = true;
    m_worldDirty = true;
    
    // 递归标记所有子节点的世界矩阵为脏
    for (size_t i = 0; i < m_owner->GetChildCount(); ++i) {
        SceneNode* child = m_owner->GetChild(i);
        if (child) {
            child->GetTransform()->m_worldDirty = true;
            child->GetTransform()->MarkDirty();
        }
    }
}

void Transform::UpdateLocalMatrix() {
    // TRS: Translation * Rotation * Scale
    
    // 1. Scale matrix
    Matrix4x4 scaleMat;
    scaleMat.m[0][0] = m_localScale.x;
    scaleMat.m[1][1] = m_localScale.y;
    scaleMat.m[2][2] = m_localScale.z;
    
    // 2. Rotation matrix (from Euler angles)
    Matrix4x4 rotMat = CreateRotationMatrix(m_localRotation);
    
    // 3. Translation matrix
    Matrix4x4 transMat = Matrix4x4::Translation(m_localPosition.x, m_localPosition.y, m_localPosition.z);
    
    // 4. Combine: T * R * S
    m_localMatrix = transMat * rotMat * scaleMat;
}

void Transform::UpdateWorldMatrix() {
    SceneNode* parent = m_owner->GetParent();
    
    if (parent) {
        // World = Parent's World * Local
        const Matrix4x4& parentWorld = parent->GetTransform()->GetWorldMatrix();
        m_worldMatrix = parentWorld * m_localMatrix;
    } else {
        // 没有父节点，世界矩阵就是本地矩阵
        m_worldMatrix = m_localMatrix;
    }
}

Matrix4x4 Transform::CreateRotationMatrix(const Vector3& euler) {
    // Euler angles: (pitch, yaw, roll) in degrees
    // 转换为弧度
    float pitch = euler.x * static_cast<float>(M_PI) / 180.0f;  // X 轴旋转
    float yaw   = euler.y * static_cast<float>(M_PI) / 180.0f;  // Y 轴旋转
    float roll  = euler.z * static_cast<float>(M_PI) / 180.0f;  // Z 轴旋转
    
    // 旋转顺序：Y * X * Z (Yaw * Pitch * Roll)
    // 这是最常用的旋转顺序
    
    float cosP = std::cos(pitch);
    float sinP = std::sin(pitch);
    float cosY = std::cos(yaw);
    float sinY = std::sin(yaw);
    float cosR = std::cos(roll);
    float sinR = std::sin(roll);
    
    Matrix4x4 result;
    
    // 左手坐标系的旋转矩阵
    result.m[0][0] = cosY * cosR + sinY * sinP * sinR;
    result.m[0][1] = cosP * sinR;
    result.m[0][2] = -sinY * cosR + cosY * sinP * sinR;
    result.m[0][3] = 0.0f;
    
    result.m[1][0] = -cosY * sinR + sinY * sinP * cosR;
    result.m[1][1] = cosP * cosR;
    result.m[1][2] = sinY * sinR + cosY * sinP * cosR;
    result.m[1][3] = 0.0f;
    
    result.m[2][0] = sinY * cosP;
    result.m[2][1] = -sinP;
    result.m[2][2] = cosY * cosP;
    result.m[2][3] = 0.0f;
    
    result.m[3][0] = 0.0f;
    result.m[3][1] = 0.0f;
    result.m[3][2] = 0.0f;
    result.m[3][3] = 1.0f;
    
    return result;
}

} // namespace Moon
