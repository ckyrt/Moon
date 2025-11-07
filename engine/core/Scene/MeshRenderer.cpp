#include "MeshRenderer.h"
#include "SceneNode.h"

namespace Moon {

MeshRenderer::MeshRenderer(SceneNode* owner)
    : Component(owner)
    , m_visible(true)
{
}

void MeshRenderer::Render(IRenderer* renderer, const Matrix4x4& viewProjection) {
    if (!m_visible || !IsEnabled()) {
        return;
    }
    
    // 获取世界变换矩阵
    const Matrix4x4& worldMatrix = GetOwner()->GetTransform()->GetWorldMatrix();
    
    // TODO: 未来这里会传递实际的 Mesh 数据
    // 现在只是一个占位符，实际渲染逻辑在 DiligentRenderer 中
    // 
    // 这里应该调用：
    // renderer->DrawMesh(mesh, worldMatrix, viewProjection);
}

} // namespace Moon
