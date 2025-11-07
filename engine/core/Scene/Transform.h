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
public:
    /**
     * @brief 构造函数
     * @param owner 拥有此 Transform 的场景节点
     */
    explicit Transform(SceneNode* owner);
    
    ~Transform() = default;

    // === 本地变换 (Local Transform - 相对于父节点) ===
    
    /**
     * @brief 设置本地位置
     * @param position 本地坐标系下的位置
     */
    void SetLocalPosition(const Vector3& position);
    
    /**
     * @brief 设置本地旋转（Euler 角）
     * @param euler 旋转角度（度数），格式：(pitch, yaw, roll)
     */
    void SetLocalRotation(const Vector3& euler);
    
    /**
     * @brief 设置本地缩放
     * @param scale 缩放系数
     */
    void SetLocalScale(const Vector3& scale);
    
    /**
     * @brief 获取本地位置
     */
    const Vector3& GetLocalPosition() const { return m_localPosition; }
    
    /**
     * @brief 获取本地旋转
     */
    const Vector3& GetLocalRotation() const { return m_localRotation; }
    
    /**
     * @brief 获取本地缩放
     */
    const Vector3& GetLocalScale() const { return m_localScale; }

    // === 世界变换 (World Transform - 绝对坐标) ===
    
    /**
     * @brief 设置世界位置
     * @param position 世界坐标系下的位置
     */
    void SetWorldPosition(const Vector3& position);
    
    /**
     * @brief 设置世界旋转
     * @param euler 世界旋转角度（度数）
     */
    void SetWorldRotation(const Vector3& euler);
    
    /**
     * @brief 获取世界位置
     */
    Vector3 GetWorldPosition();
    
    /**
     * @brief 获取世界旋转
     */
    Vector3 GetWorldRotation();

    // === 方向向量 (Direction Vectors - 世界空间) ===
    
    /**
     * @brief 获取前方向量（世界空间）
     * @return +Z 方向（左手坐标系）
     */
    Vector3 GetForward();
    
    /**
     * @brief 获取右方向量（世界空间）
     * @return +X 方向
     */
    Vector3 GetRight();
    
    /**
     * @brief 获取上方向量（世界空间）
     * @return +Y 方向
     */
    Vector3 GetUp();

    // === 矩阵访问 ===
    
    /**
     * @brief 获取本地变换矩阵
     * @return 本地坐标系下的 TRS 矩阵
     */
    const Matrix4x4& GetLocalMatrix();
    
    /**
     * @brief 获取世界变换矩阵
     * @return 世界坐标系下的变换矩阵
     */
    const Matrix4x4& GetWorldMatrix();

    // === 变换操作 ===
    
    /**
     * @brief 平移
     * @param offset 平移偏移量
     * @param worldSpace true=世界空间，false=本地空间
     */
    void Translate(const Vector3& offset, bool worldSpace = false);
    
    /**
     * @brief 旋转
     * @param eulerAngles 旋转角度（度数）
     * @param worldSpace true=世界空间，false=本地空间
     */
    void Rotate(const Vector3& eulerAngles, bool worldSpace = false);
    
    /**
     * @brief 朝向目标点
     * @param target 目标位置（世界空间）
     * @param up 上方向（世界空间）
     */
    void LookAt(const Vector3& target, const Vector3& up = Vector3(0, 1, 0));

    // === 内部使用 ===
    
    /**
     * @brief 标记变换为脏（需要重新计算）
     * 当父节点变换时会调用此方法
     */
    void MarkDirty();

private:
    SceneNode* m_owner;  ///< 拥有此 Transform 的节点
    
    // 本地变换参数
    Vector3 m_localPosition;
    Vector3 m_localRotation;  ///< Euler angles (degrees)
    Vector3 m_localScale;
    
    // 缓存的矩阵
    Matrix4x4 m_localMatrix;
    Matrix4x4 m_worldMatrix;
    
    // 脏标记
    bool m_localDirty;   ///< 本地矩阵需要更新
    bool m_worldDirty;   ///< 世界矩阵需要更新
    
    /**
     * @brief 更新本地变换矩阵
     */
    void UpdateLocalMatrix();
    
    /**
     * @brief 更新世界变换矩阵
     */
    void UpdateWorldMatrix();
    
    /**
     * @brief 从 Euler 角创建旋转矩阵
     * @param euler 旋转角度（度数）
     * @return 旋转矩阵
     */
    static Matrix4x4 CreateRotationMatrix(const Vector3& euler);
};

} // namespace Moon
