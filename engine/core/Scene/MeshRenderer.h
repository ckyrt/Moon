#pragma once
#include "Component.h"
#include "../Camera/Camera.h"

// Forward declarations
class IRenderer;

namespace Moon {

/**
 * @brief MeshRenderer 组件 - 负责渲染网格
 * 
 * 这是一个简化版的 MeshRenderer，用于渲染基本的立方体。
 * 未来可以扩展为支持任意 Mesh 数据。
 */
class MeshRenderer : public Component {
public:
    /**
     * @brief 构造函数
     * @param owner 拥有此组件的场景节点
     */
    explicit MeshRenderer(SceneNode* owner);
    
    ~MeshRenderer() override = default;

    /**
     * @brief 渲染网格
     * @param renderer 渲染器接口
     * @param viewProjection 视图投影矩阵
     */
    void Render(IRenderer* renderer, const Matrix4x4& viewProjection);
    
    /**
     * @brief 设置是否可见
     */
    void SetVisible(bool visible) { m_visible = visible; }
    
    /**
     * @brief 获取是否可见
     */
    bool IsVisible() const { return m_visible; }

private:
    bool m_visible;  ///< 是否可见
};

} // namespace Moon
