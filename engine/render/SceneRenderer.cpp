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

// 透明度阈值常量：大于等于此值认为是不透明物体
constexpr float OPACITY_THRESHOLD = 0.99f;

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

// 辅助函数：绑定材质纹理（统一处理）
static void BindMaterialTextures(DiligentRenderer* renderer, Material* material)
{
    if (!material || !material->IsEnabled()) {
        renderer->SetMaterialParameters(nullptr);
        renderer->BindAlbedoTexture("");
        renderer->BindAOTexture("");
        renderer->BindRoughnessTexture("");
        renderer->BindMetalnessTexture("");
        renderer->BindNormalTexture("");
        return;
    }
    
    renderer->SetMaterialParameters(material);
    
    renderer->BindAlbedoTexture(material->HasAlbedoMap() ? material->GetAlbedoMap() : "");
    renderer->BindAOTexture(material->HasAOMap() ? material->GetAOMap() : "");
    renderer->BindRoughnessTexture(material->HasRoughnessMap() ? material->GetRoughnessMap() : "");
    renderer->BindMetalnessTexture(material->HasMetalnessMap() ? material->GetMetalnessMap() : "");
    renderer->BindNormalTexture(material->HasNormalMap() ? material->GetNormalMap() : "");
}

// 渲染不透明物体（opacity >= OPACITY_THRESHOLD）
void RenderOpaqueMeshes(DiligentRenderer* renderer, Scene* scene)
{
    if (!renderer || !scene) {
        return;
    }
    
    scene->Traverse([&](SceneNode* node) {
        MeshRenderer* meshRenderer = node->GetComponent<MeshRenderer>();
        if (!meshRenderer || !meshRenderer->IsEnabled() || !meshRenderer->IsVisible()) {
            return;
        }
        
        Material* material = node->GetComponent<Material>();
        
        // 检查是否为不透明材质（opacity >= OPACITY_THRESHOLD 认为是不透明）
        bool isOpaque = true;
        if (material && material->IsEnabled()) {
            isOpaque = (material->GetOpacity() >= OPACITY_THRESHOLD);
        }
        
        if (!isOpaque) {
            return;  // 跳过透明物体
        }
        
        // 设置材质和纹理
        BindMaterialTextures(renderer, material);
        
        meshRenderer->Render(renderer);
    });
}

// 渲染透明物体（opacity < OPACITY_THRESHOLD）
void RenderTransparentMeshes(DiligentRenderer* renderer, Scene* scene)
{
    if (!renderer || !scene) {
        return;
    }
    
    // TODO: 后续优化可以按深度排序（从后往前渲染）
    scene->Traverse([&](SceneNode* node) {
        MeshRenderer* meshRenderer = node->GetComponent<MeshRenderer>();
        if (!meshRenderer || !meshRenderer->IsEnabled() || !meshRenderer->IsVisible()) {
            return;
        }
        
        Material* material = node->GetComponent<Material>();
        
        // 检查是否为透明材质（opacity < OPACITY_THRESHOLD）
        bool isTransparent = false;
        if (material && material->IsEnabled()) {
            isTransparent = (material->GetOpacity() < OPACITY_THRESHOLD);
        }
        
        if (!isTransparent) {
            return;  // 跳过不透明物体
        }
        
        // 设置材质和纹理
        BindMaterialTextures(renderer, material);
        
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
    
    // 2. 渲染所有不透明物体（Pass 1）
    renderer->SetRenderingTransparent(false);
    RenderOpaqueMeshes(renderer, scene);
    
    // 3. 渲染天空盒（在不透明物体之后，透明物体之前）
    renderer->RenderSkybox();
    
    // 4. 渲染所有透明物体（Pass 2）
    renderer->SetRenderingTransparent(true);
    RenderTransparentMeshes(renderer, scene);
}

} // namespace SceneRendererUtils
} // namespace Moon
