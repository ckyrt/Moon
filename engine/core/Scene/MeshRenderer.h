#pragma once
#include "Component.h"
#include "../Camera/Camera.h"
#include <memory>

// Forward declarations
class IRenderer;

namespace Moon {

// Forward declarations
class Mesh;

/**
 * @brief MeshRenderer 组件 - 负责渲染网格
 * 
 * 持有 Mesh 的共享所有权（shared_ptr），支持多个 Renderer 共享同一个 Mesh。
 * Mesh 的生命周期由引用计数自动管理，最后一个引用消失时自动释放。
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
     * @brief 设置要渲染的 Mesh（共享所有权）
     * @param mesh Mesh 智能指针
     */
    void SetMesh(std::shared_ptr<Mesh> mesh) { m_mesh = mesh; }
    
    /**
     * @brief 获取当前的 Mesh
     */
    std::shared_ptr<Mesh> GetMesh() const { return m_mesh; }
    
    /**
     * @brief 设置是否可见
     */
    void SetVisible(bool visible) { m_visible = visible; }
    
    /**
     * @brief 获取是否可见
     */
    bool IsVisible() const { return m_visible; }

private:
    std::shared_ptr<Mesh> m_mesh;  ///< 要渲染的 Mesh（共享所有权）
    bool m_visible;                ///< 是否可见
};

} // namespace Moon
