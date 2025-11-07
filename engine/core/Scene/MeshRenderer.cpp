#include "MeshRenderer.h"
#include "SceneNode.h"
#include "../Mesh/Mesh.h"
#include "../../render/IRenderer.h"

namespace Moon {

MeshRenderer::MeshRenderer(SceneNode* owner)
    : Component(owner)
    , m_mesh(nullptr)
    , m_visible(true)
{
}

void MeshRenderer::Render(IRenderer* renderer) {
    // 检查是否可见、启用、且有有效的 Mesh
    if (!m_visible || !IsEnabled() || !m_mesh || !m_mesh->IsValid()) {
        return;
    }
    
    // 获取世界变换矩阵
    const Matrix4x4& worldMatrix = GetOwner()->GetTransform()->GetWorldMatrix();
    
    // 调用渲染器绘制 Mesh
    renderer->DrawMesh(m_mesh, worldMatrix);
}

} // namespace Moon
