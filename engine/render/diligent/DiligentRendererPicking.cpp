#include "DiligentRenderer.h"
#include "DiligentRendererUtils.h"
#include "../../core/Logging/Logger.h"
#include "../../core/Scene/Scene.h"
#include "../../core/Scene/SceneNode.h"
#include "../../core/Scene/MeshRenderer.h"

#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/TextureView.h"

using namespace Diligent;

// ======= Picking：静态资源（一次性） =======
void DiligentRenderer::CreatePickingStatic()
{
    // PS 常量缓冲（16B 对齐）
    if (!m_pPickingPSConstants) {
        BufferDesc cb{};
        cb.Name = "Picking PS CB";
        cb.BindFlags = BIND_UNIFORM_BUFFER;
        cb.Usage = USAGE_DYNAMIC;
        cb.CPUAccessFlags = CPU_ACCESS_WRITE;
        cb.Size = sizeof(PSConstantsCPU);
        m_pDevice->CreateBuffer(cb, nullptr, &m_pPickingPSConstants);
    }

    if (m_pPickingPSO) return;

    // VS
    RefCntAutoPtr<IShader> vs;
    {
        ShaderCreateInfo ci{};
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name = "Picking VS";
        ci.EntryPoint = "main_vs";
        ci.Source = R"(
cbuffer VSConstants { 
    float4x4 g_WorldViewProj;
    float4x4 g_World;  // 保持 CB 布局一致
};
struct VSInput { 
    float3 Position : ATTRIB0; 
    float3 Normal   : ATTRIB1;  // 必须声明以匹配 stride
    float4 Color    : ATTRIB2;  // 必须声明以匹配 stride
    float2 UV       : ATTRIB3;  // 必须声明以匹配 stride（新增的UV坐标）
};
struct PSInput { float4 Position : SV_Position; };
PSInput main_vs(VSInput i){
    PSInput o; o.Position = mul(float4(i.Position,1), g_WorldViewProj); return o;
})";
        m_pDevice->CreateShader(ci, &vs);
    }
    // PS
    RefCntAutoPtr<IShader> ps;
    {
        ShaderCreateInfo ci{};
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ci.Desc.Name = "Picking PS";
        ci.EntryPoint = "main_ps";
        ci.Source = R"(
cbuffer PSConstants { uint g_ObjectID; };
struct PSInput { float4 Position:SV_Position; };
uint main_ps(PSInput i) : SV_Target { return g_ObjectID; })";
        m_pDevice->CreateShader(ci, &ps);
    }

    GraphicsPipelineStateCreateInfo pci{};
    pci.PSODesc.Name = "Picking PSO";
    pci.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    pci.GraphicsPipeline.NumRenderTargets = 1;
    pci.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_R32_UINT;
    pci.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
    pci.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pci.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    pci.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    // 使用统一的 Vertex Layout
    LayoutElement layout[4];
    Uint32 numElements;
    DiligentRendererUtils::GetVertexLayout(layout, numElements);
    pci.GraphicsPipeline.InputLayout.LayoutElements = layout;
    pci.GraphicsPipeline.InputLayout.NumElements = numElements;

    pci.pVS = vs;
    pci.pPS = ps;

    // 绑定静态变量
    ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_VERTEX, "VSConstants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_PIXEL,  "PSConstants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
    };
    pci.PSODesc.ResourceLayout.Variables = vars;
    pci.PSODesc.ResourceLayout.NumVariables = _countof(vars);

    m_pDevice->CreateGraphicsPipelineState(pci, &m_pPickingPSO);
    m_pPickingPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")->Set(m_pVSConstants);
    m_pPickingPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PSConstants")->Set(m_pPickingPSConstants);
    m_pPickingPSO->CreateShaderResourceBinding(&m_pPickingSRB, true);
}

// ======= Picking：按当前分辨率重建 RT/DS + 读回纹理 =======
void DiligentRenderer::CreateOrResizePickingRTs()
{
    m_pPickingRT.Release();
    m_pPickingRTV.Release();
    m_pPickingDS.Release();
    m_pPickingDSV.Release();

    // RT (R32_UINT)
    TextureDesc rt{};
    rt.Name = "Picking RT";
    rt.Type = RESOURCE_DIM_TEX_2D;
    rt.Width = m_Width;
    rt.Height = m_Height;
    rt.MipLevels = 1;
    rt.Format = TEX_FORMAT_R32_UINT;
    rt.BindFlags = BIND_RENDER_TARGET;
    rt.Usage = USAGE_DEFAULT;
    m_pDevice->CreateTexture(rt, nullptr, &m_pPickingRT);
    m_pPickingRTV = m_pPickingRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);

    // DS (D32)
    TextureDesc ds = rt;
    ds.Name = "Picking DS";
    ds.Format = TEX_FORMAT_D32_FLOAT;
    ds.BindFlags = BIND_DEPTH_STENCIL;
    m_pDevice->CreateTexture(ds, nullptr, &m_pPickingDS);
    m_pPickingDSV = m_pPickingDS->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

    // Readback 1x1
    if (!m_pPickingReadback) {
        TextureDesc rb{};
        rb.Name = "Picking Readback 1x1";
        rb.Type = RESOURCE_DIM_TEX_2D;
        rb.Width = 1; rb.Height = 1;
        rb.MipLevels = 1;
        rb.Format = TEX_FORMAT_R32_UINT;
        rb.Usage = USAGE_STAGING;
        rb.CPUAccessFlags = CPU_ACCESS_READ;
        m_pDevice->CreateTexture(rb, nullptr, &m_pPickingReadback);
    }

    MOON_LOG_INFO("DiligentRenderer", "Picking RT/DS recreated (%ux%u)", m_Width, m_Height);
}

// ======= Picking：渲染 =======
void DiligentRenderer::RenderSceneForPicking(Moon::Scene* scene, int vpX, int vpY, int vpW, int vpH)
{
    if (!scene || !m_pPickingRTV || !m_pPickingDSV) return;

    MOON_LOG_INFO("Picking", "RenderSceneForPicking: RT size=(%ux%u), viewport=(%d,%d,%dx%d)", 
        m_Width, m_Height, vpX, vpY, vpW, vpH);

    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

    ITextureView* rtvs[] = { m_pPickingRTV };
    m_pImmediateContext->SetRenderTargets(1, rtvs, m_pPickingDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const Uint32 Zero[4] = { 0,0,0,0 };
    m_pImmediateContext->ClearRenderTarget(m_pPickingRTV, reinterpret_cast<const float*>(Zero), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(m_pPickingDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // ✅ 设置 viewport（如果提供了参数）
    if (vpW > 0 && vpH > 0) {
        Viewport vp;
        vp.TopLeftX = static_cast<float>(vpX);
        vp.TopLeftY = static_cast<float>(vpY);
        vp.Width = static_cast<float>(vpW);
        vp.Height = static_cast<float>(vpH);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        m_pImmediateContext->SetViewports(1, &vp, 0, 0);
        
        MOON_LOG_INFO("Picking", "  Set viewport: (%d, %d, %dx%d)", vpX, vpY, vpW, vpH);
    }

    m_pImmediateContext->SetPipelineState(m_pPickingPSO);

    scene->Traverse([&](Moon::SceneNode* node) {
        auto* mr = node->GetComponent<Moon::MeshRenderer>();
        if (!mr || !mr->IsEnabled() || !mr->IsVisible()) return;

        auto sp = mr->GetMesh();
        auto* mesh = sp ? sp.get() : nullptr;
        if (!mesh) return;

        auto* gpu = GetOrCreateMeshResources(mesh);
        if (!gpu || !gpu->VB) return;

        Moon::Matrix4x4 world = node->GetTransform()->GetWorldMatrix();
        Moon::Matrix4x4 wvp = world * m_ViewProj;
        VSConstantsCPU vsc{};
        vsc.WorldViewProjT = DiligentRendererUtils::Transpose(wvp);
        vsc.WorldT = DiligentRendererUtils::Transpose(world);
        UpdateCB(m_pVSConstants, vsc);

        PSConstantsCPU psc{}; psc.ObjectID = node->GetID();
        UpdateCB(m_pPickingPSConstants, psc);

        Uint64 offset = 0;
        IBuffer* vbs[] = { gpu->VB };
        m_pImmediateContext->SetVertexBuffers(0, 1, vbs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(gpu->IB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->CommitShaderResources(m_pPickingSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs da{};
        da.IndexType = VT_UINT32;
        da.NumIndices = static_cast<Uint32>(gpu->IndexCount);
        da.Flags = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->DrawIndexed(da);
    });
}

// ======= Picking：读取像素 ID =======
uint32_t DiligentRenderer::ReadObjectIDAt(int x, int y)
{
    if (x < 0 || y < 0 || x >= static_cast<int>(m_Width) || y >= static_cast<int>(m_Height)) {
        return 0;
    }
    if (!m_pPickingRTV || !m_pPickingReadback) {
        return 0;
    }

    Box src{};
    src.MinX = x; src.MaxX = x + 1;
    src.MinY = y; src.MaxY = y + 1;
    src.MinZ = 0; src.MaxZ = 1;

    CopyTextureAttribs cp{};
    cp.pSrcTexture = m_pPickingRT;
    cp.pDstTexture = m_pPickingReadback;
    cp.pSrcBox = &src;

    m_pImmediateContext->CopyTexture(cp);
    m_pImmediateContext->Flush();
    m_pImmediateContext->WaitForIdle();

    MappedTextureSubresource m{};
    m_pImmediateContext->MapTextureSubresource(m_pPickingReadback, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, m);

    uint32_t id = 0;
    if (m.pData) id = *reinterpret_cast<const uint32_t*>(m.pData);

    m_pImmediateContext->UnmapTextureSubresource(m_pPickingReadback, 0, 0);
    return id;
}
