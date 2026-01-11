#include "SceneRenderer.h"
#include "../core/Scene/Scene.h"
#include "../core/Scene/SceneNode.h"
#include "../core/Scene/MeshRenderer.h"
#include "../core/Scene/Material.h"
#include "../core/Camera/Camera.h"
#include "../core/Logging/Logger.h"
#include "diligent/DiligentRenderer.h"

// Forward declaration to avoid including DiligentRenderer.h in EngineCore project
// DiligentRenderer is defined in EngineRender project, which depends on DiligentEngine
class DiligentRenderer;

namespace Moon {
namespace SceneRendererUtils {

void PrepareRender(DiligentRenderer* renderer, Scene* scene, Camera* camera)
{
    if (!renderer || !scene || !camera) {
        MOON_LOG_ERROR("SceneRenderer", "Invalid parameters: renderer=%p, scene=%p, camera=%p", 
                       renderer, scene, camera);
        return;
    }
    
    // 1. 设置 ViewProjection 矩阵
    auto vp = camera->GetViewProjectionMatrix();
    renderer->SetViewProjectionMatrix(&vp.m[0][0]);
    
    // 2. 设置相机位置（用于PBR高光计算）
    renderer->SetCameraPosition(camera->GetPosition());
    
    // 3. 更新场景光源（从场景中查找 Light 组件）
    renderer->UpdateSceneLights(scene);
    
    // 4. 更新天空盒（从场景中查找 Skybox 组件并加载环境贴图）
    renderer->UpdateSceneSkybox(scene);
}

void RenderMeshes(DiligentRenderer* renderer, Scene* scene)
{
    if (!renderer || !scene) {
        return;
    }
    
    // 遍历场景渲染所有启用的 MeshRenderer
    scene->Traverse([&](SceneNode* node) {
        MeshRenderer* meshRenderer = node->GetComponent<MeshRenderer>();
        if (!meshRenderer || !meshRenderer->IsEnabled() || !meshRenderer->IsVisible()) {
            return;
        }
        
        // 从 Material 组件获取材质参数和纹理
        Material* material = node->GetComponent<Material>();
        if (material && material->IsEnabled()) {
            // ✅ 设置完整的材质参数（包括mapping mode和triplanar参数）
            renderer->SetMaterialParameters(material);
            
            // 绑定 Albedo 贴图
            if (material->HasAlbedoMap()) {
                renderer->BindAlbedoTexture(material->GetAlbedoMap());
            } else {
                renderer->BindAlbedoTexture("");
            }
            
            // 绑定 AO 贴图
            if (material->HasAOMap()) {
                renderer->BindAOTexture(material->GetAOMap());
            } else {
                renderer->BindAOTexture("");
            }
            
            // 绑定 Roughness 贴图
            if (material->HasRoughnessMap()) {
                renderer->BindRoughnessTexture(material->GetRoughnessMap());
            } else {
                renderer->BindRoughnessTexture("");
            }
            
            // 绑定 Metalness 贴图
            if (material->HasMetalnessMap()) {
                renderer->BindMetalnessTexture(material->GetMetalnessMap());
            } else {
                renderer->BindMetalnessTexture("");
            }
            
            // 绑定法线贴图
            if (material->HasNormalMap()) {
                renderer->BindNormalTexture(material->GetNormalMap());
            } else {
                renderer->BindNormalTexture("");
            }
        } else {
            // 对于没有 Material 组件的对象，使用默认材质
            // (新创建的对象都会有 Material，这是为了向后兼容)
            renderer->SetMaterialParameters(nullptr);
            renderer->BindAlbedoTexture("");
            renderer->BindAOTexture("");
            renderer->BindRoughnessTexture("");
            renderer->BindMetalnessTexture("");
            renderer->BindNormalTexture("");
        }
        
        // 渲染网格
        meshRenderer->Render(renderer);
    });
}

void RenderScene(DiligentRenderer* renderer, Scene* scene, Camera* camera)
{
    if (!renderer || !scene || !camera) {
        return;
    }
    
    // 1. 准备渲染（相机、光源、天空盒）
    PrepareRender(renderer, scene, camera);
    
    // 2. 渲染所有网格对象
    RenderMeshes(renderer, scene);
    
    // 3. 渲染天空盒（在所有不透明物体之后）
    renderer->RenderSkybox();
}

} // namespace SceneRendererUtils
} // namespace Moon
