#pragma once

// Forward declarations
class DiligentRenderer;

namespace Moon {
    class Scene;
    class Camera;
}

/**
 * SceneRenderer - 通用场景渲染工具
 * 提供统一的场景渲染逻辑，避免在 EditorApp 和 HelloEngine 中重复代码
 */
namespace Moon {
namespace SceneRendererUtils {
    
    /**
     * 准备渲染（设置相机、光源、天空盒）
     * @param renderer 渲染器
     * @param scene 场景
     * @param camera 相机
     */
    void PrepareRender(DiligentRenderer* renderer, Scene* scene, Camera* camera);
    
    /**
     * 渲染场景中的所有网格对象
     * @param renderer 渲染器
     * @param scene 场景
     */
    void RenderMeshes(DiligentRenderer* renderer, Scene* scene);
    
    /**
     * 完整的场景渲染流程
     * @param renderer 渲染器
     * @param scene 场景
     * @param camera 相机
     */
    void RenderScene(DiligentRenderer* renderer, Scene* scene, Camera* camera);
    
} // namespace SceneRendererUtils
} // namespace Moon
