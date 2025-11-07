#pragma once
#include "Component.h"
#include "../Camera/Camera.h"

// Forward declarations
class IRenderer;

namespace Moon {

// Forward declarations
class Mesh;

/**
 * @brief MeshRenderer 组件 - 负责渲染网格
 * 
 * 持有 Mesh 数据的引用，并通过渲染器进行绘制。
 * 注意：MeshRenderer 不拥有 Mesh，只持有指针。
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
     * 
     * 如果 Mesh 有效且可见，将调用 renderer->DrawMesh() 进行渲染
     */
    void Render(IRenderer* renderer);
    
    /**
     * @brief 设置要渲染的 Mesh
     * @param mesh Mesh 指针（不转移所有权）
     */
    void SetMesh(Mesh* mesh) { m_mesh = mesh; }
    
    /**
     * @brief 获取当前的 Mesh
     */
    Mesh* GetMesh() const { return m_mesh; }
    
    /**
     * @brief 设置是否可见
     */
    void SetVisible(bool visible) { m_visible = visible; }
    
    /**
     * @brief 获取是否可见
     */
    bool IsVisible() const { return m_visible; }

private:
    Mesh* m_mesh;      ///< 要渲染的 Mesh（不拥有）
    bool m_visible;    ///< 是否可见
};

} // namespace Moon
