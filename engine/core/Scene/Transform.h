#pragma once
#include "../Camera/Camera.h"  // For Vector3, Matrix4x4

namespace Moon {

// Forward declaration
class SceneNode;

/**
 * @brief 变换组件 - 管理场景节点的位置、旋转、缩放
 * 
 * Transform 组件存储和计算场景节点的空间变换。
 * 支持本地坐标系（相对于父节点）和世界坐标系。
 * 使用脏标记机制进行矩阵缓存优化。
 */

class Transform {

    friend class SceneNode;
public:
    Transform(SceneNode* owner);

    // Local setters
    void SetLocalPosition(const Vector3& p);
    void SetLocalRotation(const Quaternion& q);
    void SetLocalScale(const Vector3& s);

    // World setters
    void SetWorldPosition(const Vector3& p);
    void SetWorldRotation(const Quaternion& q);
    void SetWorldScale(const Vector3& s);

    // Getters
    Vector3 GetLocalPosition() const { return m_localPosition; }
    Quaternion GetLocalRotation() const { return m_localRotation; }
    Vector3 GetLocalScale() const { return m_localScale; }

    Vector3 GetWorldPosition();
    Quaternion GetWorldRotation();
    Vector3 GetWorldScale();

    Vector3 GetForward();
    Vector3 GetRight();
    Vector3 GetUp();

    const Matrix4x4& GetLocalMatrix();
    const Matrix4x4& GetWorldMatrix();

    // Transform operations
    void Translate(const Vector3& v, bool worldSpace);
    void Rotate(const Vector3& euler, bool worldSpace);
    void LookAt(const Vector3& target, const Vector3& up = Vector3(0, 1, 0));

    void SetLocalRotation(const Vector3& eulerDeg);
    void SetWorldRotation(const Vector3& eulerDeg);
    Vector3 GetLocalEulerAngles();
    Vector3 GetWorldEulerAngles();

private:
    void UpdateLocalMatrix();
    void UpdateWorldMatrix();
    void MarkDirty();
    void MarkChildrenWorldDirty();  // 递归标记所有子孙节点世界矩阵为脏

private:
    SceneNode* m_owner;

    Vector3 m_localPosition;
    Quaternion m_localRotation;
    Vector3 m_localScale;

    Matrix4x4 m_localMatrix;
    Matrix4x4 m_worldMatrix;

    bool m_localDirty;
    bool m_worldDirty;
};

} // namespace Moon
