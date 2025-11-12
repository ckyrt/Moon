#include "DiligentRenderer.h"

// Moon Engine
#include "../core/Logging/Logger.h"
#include "../core/Camera/Camera.h"
#include "../core/Scene/Scene.h"
#include "../core/Scene/SceneNode.h"
#include "../core/Scene/MeshRenderer.h"
#include "../core/Mesh/Mesh.h"

#include <cmath>
#include <cstring>

// Diligent includes
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/TextureView.h"

#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Platforms/Win32/interface/Win32NativeWindow.h"
#endif

using namespace Diligent;

// ======= 工具函数 =======
Moon::Matrix4x4 DiligentRenderer::Transpose(const Moon::Matrix4x4& a)
{
    Moon::Matrix4x4 t{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            t.m[i][j] = a.m[j][i];
    return t;
}

/**
 * @brief 从 Vertex 结构自动生成 Diligent Engine 的 InputLayout
 * 
 * 该函数根据 Vertex::GetLayoutDesc() 提供的布局信息，自动生成 LayoutElement 数组。
 * 这样当 Vertex 结构修改时（例如添加法线、UV），只需更新 Vertex::GetLayoutDesc()，
 * 所有 PSO 的 InputLayout 会自动同步更新。
 * 
 * 优势：
 * - ✅ 单一真实来源（Single Source of Truth）在 Vertex 结构内部
 * - ✅ 使用 offsetof() 自动计算偏移，无需手动维护
 * - ✅ 编译时 static_assert 验证布局正确性
 * 
 * @param[out] outLayout 指向至少能容纳 Vertex 属性数量的 LayoutElement 数组
 * @param[out] outNumElements 返回属性数量
 */
void DiligentRenderer::GetVertexLayout(LayoutElement* outLayout, Uint32& outNumElements)
{
    int attrCount = 0;
    const Moon::VertexAttributeDesc* attrs = Moon::Vertex::GetLayoutDesc(attrCount);
    
    for (int i = 0; i < attrCount; ++i) {
        outLayout[i].InputIndex = i;                              // ATTRIB0, ATTRIB1, ... 的索引
        outLayout[i].BufferSlot = 0;                              // 使用第 0 个 vertex buffer
        outLayout[i].NumComponents = attrs[i].numComponents;      // 从 Vertex 读取分量数
        outLayout[i].ValueType = VT_FLOAT32;                      // 目前都是 float
        outLayout[i].IsNormalized = False;                        // 不归一化
        outLayout[i].RelativeOffset = attrs[i].offsetInBytes;     // 从 Vertex 读取偏移
    }
    
    outNumElements = static_cast<Uint32>(attrCount);
}
template<typename T>
void DiligentRenderer::UpdateCB(IBuffer* buf, const T& data)
{
    void* p = nullptr;
    m_pImmediateContext->MapBuffer(buf, MAP_WRITE, MAP_FLAG_DISCARD, p);
    std::memcpy(p, &data, sizeof(T));
    m_pImmediateContext->UnmapBuffer(buf, MAP_WRITE);
}

// ======= 构造/析构 =======
DiligentRenderer::DiligentRenderer()
{
}
DiligentRenderer::~DiligentRenderer()
{
    Shutdown();
}

// ======= 初始化 =======
bool DiligentRenderer::Initialize(const RenderInitParams& params)
{
#ifdef _WIN32
    m_hWnd = params.windowHandle;
#endif
    m_Width = params.width;
    m_Height = params.height;

    MOON_LOG_INFO("DiligentRenderer", "Starting initialization...");
    try {
        CreateDeviceAndSwapchain(params);
        CreateVSConstants();
        CreateMainPass();  // 主渲染管线（用于正常场景渲染）

        // Picking：一次性创建静态资源；并按当前分辨率创建 RT/DS + 读回
        CreatePickingStatic();
        CreateOrResizePickingRTs();

        MOON_LOG_INFO("DiligentRenderer", "Initialized successfully!");
        return true;
    }
    catch (const std::exception& e) {
        MOON_LOG_ERROR("DiligentRenderer", "Initialize failed: %s", e.what());
        return false;
    }
}

void DiligentRenderer::CreateDeviceAndSwapchain(const RenderInitParams& params)
{
    auto* pFactory = GetEngineFactoryD3D11();

    EngineD3D11CreateInfo ci{};
    pFactory->CreateDeviceAndContextsD3D11(ci, &m_pDevice, &m_pImmediateContext);
    MOON_LOG_INFO("DiligentRenderer", "D3D11 device/context created");

    SwapChainDesc sc{};
    sc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM_SRGB;
    sc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    sc.Usage = SWAP_CHAIN_USAGE_RENDER_TARGET;
    sc.BufferCount = 2;
#ifdef _WIN32
    Win32NativeWindow wnd{ static_cast<HWND>(params.windowHandle) };
    pFactory->CreateSwapChainD3D11(m_pDevice, m_pImmediateContext, sc, FullScreenModeDesc{}, wnd, &m_pSwapChain);
#else
#error "This sample currently targets Win32 only."
#endif
    MOON_LOG_INFO("DiligentRenderer", "SwapChain created");

    m_pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    m_pDSV = m_pSwapChain->GetDepthBufferDSV();
}

void DiligentRenderer::CreateVSConstants()
{
    BufferDesc cb{};
    cb.Name = "VS Constants";
    cb.BindFlags = BIND_UNIFORM_BUFFER;
    cb.Usage = USAGE_DYNAMIC;
    cb.CPUAccessFlags = CPU_ACCESS_WRITE;
    cb.Size = sizeof(VSConstantsCPU);
    m_pDevice->CreateBuffer(cb, nullptr, &m_pVSConstants);
}

void DiligentRenderer::CreateMainPass()
{
    // 通用 Mesh 着色器（位置 + 颜色）
    const char* vsCode = R"(
cbuffer Constants { float4x4 g_WorldViewProj; };
struct VSInput { 
    float3 Pos : ATTRIB0; 
    float4 Color : ATTRIB1;
};
struct PSInput { 
    float4 Pos : SV_POSITION; 
    float4 Color : COLOR;
};
void main(in VSInput i, out PSInput o) {
    o.Pos = mul(float4(i.Pos, 1.0), g_WorldViewProj);
    o.Color = i.Color;
}
)";

    const char* psCode = R"(
struct PSInput { 
    float4 Pos : SV_POSITION; 
    float4 Color : COLOR;
};
float4 main(in PSInput i) : SV_TARGET {
    return i.Color;
}
)";

    // Create Vertex Shader
    RefCntAutoPtr<IShader> vs;
    {
        ShaderCreateInfo ci{};
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name = "Main VS";
        ci.Source = vsCode;
        m_pDevice->CreateShader(ci, &vs);
    }

    // Create Pixel Shader
    RefCntAutoPtr<IShader> ps;
    {
        ShaderCreateInfo ci{};
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ci.Desc.Name = "Main PS";
        ci.Source = psCode;
        m_pDevice->CreateShader(ci, &ps);
    }

    // 使用统一的 Vertex Layout（避免手写重复声明）
    LayoutElement layout[2];
    Uint32 numElements;
    GetVertexLayout(layout, numElements);

    GraphicsPipelineStateCreateInfo pci{};
    pci.PSODesc.Name = "Main PSO";
    pci.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    pci.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    pci.GraphicsPipeline.NumRenderTargets = 1;
    pci.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    pci.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    pci.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pci.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    pci.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    pci.GraphicsPipeline.InputLayout.LayoutElements = layout;
    pci.GraphicsPipeline.InputLayout.NumElements = numElements;

    pci.pVS = vs;
    pci.pPS = ps;

    m_pDevice->CreateGraphicsPipelineState(pci, &m_pPSO);
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_pVSConstants);
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);

    MOON_LOG_INFO("DiligentRenderer", "Main PSO created");
}

// ======= 帧流程 =======
void DiligentRenderer::BeginFrame()
{
    if (!m_pSwapChain) return;

    ITextureView* rtvs[] = { m_pRTV };
    m_pImmediateContext->SetRenderTargets(1, rtvs, m_pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const float ClearColor[] = { 0.2f,0.4f,0.6f,1.0f };
    m_pImmediateContext->ClearRenderTarget(m_pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(m_pDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Viewport vp{};
    vp.Width = static_cast<float>(m_Width);
    vp.Height = static_cast<float>(m_Height);
    vp.MinDepth = 0.f; vp.MaxDepth = 1.f;
    m_pImmediateContext->SetViewports(1, &vp, 0, 0);
}

void DiligentRenderer::EndFrame()
{
    if (m_pSwapChain) m_pSwapChain->Present();
}

void DiligentRenderer::RenderFrame()
{
    // Legacy method - not used with scene system
}

// ======= Resize =======
void DiligentRenderer::Resize(uint32_t w, uint32_t h)
{
    if (w == 0 || h == 0) return;
    if (w == m_Width && h == m_Height) return;

    MOON_LOG_INFO("DiligentRenderer", "Resizing %ux%u -> %ux%u", m_Width, m_Height, w, h);
    m_Width = w; m_Height = h;

    if (m_pSwapChain) {
        m_pSwapChain->Resize(w, h);
        m_pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        m_pDSV = m_pSwapChain->GetDepthBufferDSV();

        // 只重建与分辨率相关的 Picking 资源
        CreateOrResizePickingRTs();
    }
}

// ======= 相机矩阵（row-major 输入） =======
void DiligentRenderer::SetViewProjectionMatrix(const float* m16)
{
    if (!m16) return;
    // 逐元素拷贝（row-major）
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            m_ViewProj.m[r][c] = m16[r * 4 + c];
}

// ======= Mesh 缓存 =======
DiligentRenderer::MeshGPUResources* DiligentRenderer::GetOrCreateMeshResources(Moon::Mesh* mesh)
{
    auto it = m_MeshCache.find(mesh);
    if (it != m_MeshCache.end()) return &it->second;

    MeshGPUResources gpu{};
    const auto& vertices = mesh->GetVertices(); // std::vector<Moon::Vertex>
    const auto& indices = mesh->GetIndices();  // std::vector<uint32_t>

    // VB
    BufferDesc vb{};
    vb.Name = "Mesh VB";
    vb.BindFlags = BIND_VERTEX_BUFFER;
    vb.Usage = USAGE_IMMUTABLE;
    vb.Size = static_cast<Uint32>(vertices.size() * sizeof(vertices[0]));
    BufferData vbData{ vertices.data(), vb.Size };
    m_pDevice->CreateBuffer(vb, &vbData, &gpu.VB);

    // IB
    BufferDesc ib{};
    ib.Name = "Mesh IB";
    ib.BindFlags = BIND_INDEX_BUFFER;
    ib.Usage = USAGE_IMMUTABLE;
    ib.Size = static_cast<Uint32>(indices.size() * sizeof(uint32_t));
    BufferData ibData{ indices.data(), ib.Size };
    m_pDevice->CreateBuffer(ib, &ibData, &gpu.IB);

    gpu.VertexCount = vertices.size();
    gpu.IndexCount = indices.size();

    auto [insIt, ok] = m_MeshCache.emplace(mesh, std::move(gpu));
    MOON_LOG_INFO("DiligentRenderer", "Mesh uploaded: %zu verts, %zu indices", insIt->second.VertexCount, insIt->second.IndexCount);
    return &insIt->second;
}

void DiligentRenderer::DrawMesh(Moon::Mesh* mesh, const Moon::Matrix4x4& world)
{
    if (!mesh || !mesh->IsValid()) return;

    auto* gpu = GetOrCreateMeshResources(mesh);

    // 更新 VS 常量
    Moon::Matrix4x4 wvp = world * m_ViewProj;
    VSConstantsCPU cbuf{}; cbuf.WorldViewProjT = Transpose(wvp);
    UpdateCB(m_pVSConstants, cbuf);

    // 设置管线状态
    m_pImmediateContext->SetPipelineState(m_pPSO);
    
    // 绑定 VB/IB
    Uint64 offset = 0;
    IBuffer* vbs[] = { gpu->VB };
    m_pImmediateContext->SetVertexBuffers(0, 1, vbs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(gpu->IB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // 提交着色器资源
    m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs da{};
    da.IndexType = VT_UINT32;
    da.NumIndices = static_cast<Uint32>(gpu->IndexCount);
    da.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(da);
}

void DiligentRenderer::DrawCube(const Moon::Matrix4x4& world)
{
    // Legacy method - not used with scene system
}

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
cbuffer VSConstants { float4x4 g_WorldViewProj; };
struct VSInput { 
    float3 Position:ATTRIB0; 
    float4 Color:ATTRIB1;      // 必须声明但不使用，以匹配 VB 的 stride
};
struct PSInput { float4 Position:SV_Position; };
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

    // 使用统一的 Vertex Layout（避免手写重复声明）
    // 注意：即使 picking shader 只使用 position，也必须声明完整的 layout 来保证 stride 正确
    LayoutElement layout[2];
    Uint32 numElements;
    GetVertexLayout(layout, numElements);
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
    // 🔥 必须先释放旧的纹理，否则 CreateTexture 会触发断言
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

    // Readback 1x1（只创建一次，或复用；分辨率与屏幕无关）
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
void DiligentRenderer::RenderSceneForPicking(Moon::Scene* scene)
{
    if (!scene || !m_pPickingRTV || !m_pPickingDSV) return;

    // 解绑之前的 RT
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

    // 绑定 Picking RT/DS
    ITextureView* rtvs[] = { m_pPickingRTV };
    m_pImmediateContext->SetRenderTargets(1, rtvs, m_pPickingDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // 清理 RT/DS
    const Uint32 Zero[4] = { 0,0,0,0 };
    m_pImmediateContext->ClearRenderTarget(m_pPickingRTV, reinterpret_cast<const float*>(Zero), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(m_pPickingDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    m_pImmediateContext->SetPipelineState(m_pPickingPSO);

    scene->Traverse([&](Moon::SceneNode* node) {
        auto* mr = node->GetComponent<Moon::MeshRenderer>();
        if (!mr || !mr->IsEnabled() || !mr->IsVisible()) return;

        auto sp = mr->GetMesh();
        auto* mesh = sp ? sp.get() : nullptr;
        if (!mesh) return;

        auto* gpu = GetOrCreateMeshResources(mesh);
        if (!gpu || !gpu->VB) return;

        // VS 常量（注意 row-major → 列主序 需转置）
        Moon::Matrix4x4 world = node->GetTransform()->GetWorldMatrix();
        Moon::Matrix4x4 wvp = world * m_ViewProj;
        VSConstantsCPU vsc{}; vsc.WorldViewProjT = Transpose(wvp);
        UpdateCB(m_pVSConstants, vsc);

        // PS 常量：ObjectID
        PSConstantsCPU psc{}; psc.ObjectID = node->GetID();
        UpdateCB(m_pPickingPSConstants, psc);

        // 绑定 VB/IB
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
    if (x < 0 || y < 0 || x >= static_cast<int>(m_Width) || y >= static_cast<int>(m_Height)) return 0;
    if (!m_pPickingRTV || !m_pPickingReadback) return 0;

    // 复制 1 像素到 1x1 读回纹理
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

// ======= 关闭 =======
void DiligentRenderer::Shutdown()
{
    MOON_LOG_INFO("DiligentRenderer", "Shutdown");

    // 清空 Mesh 缓存（RefCntAutoPtr 自动释放）
    m_MeshCache.clear();

    // Picking（RefCntAutoPtr 自动释放）
    m_pPickingSRB.Release();
    m_pPickingPSO.Release();
    m_pPickingPSConstants.Release();
    m_pPickingReadback.Release();
    m_pPickingRTV.Release();
    m_pPickingRT.Release();
    m_pPickingDSV.Release();
    m_pPickingDS.Release();

    // 主渲染管线
    m_pSRB.Release();
    m_pPSO.Release();
    m_pVSConstants.Release();

    // Backbuffer 视图由 swapchain 拥有
    m_pRTV = nullptr;
    m_pDSV = nullptr;

    // 设备/上下文/交换链
    m_pSwapChain.Release();
    m_pImmediateContext.Release();
    m_pDevice.Release();

#ifdef _WIN32
    m_hWnd = nullptr;
#endif
}
