#include "DiligentRenderer.h"
#include "DiligentRendererUtils.h"

// Moon Engine
#include "../../core/Logging/Logger.h"
#include "../../core/Scene/Scene.h"
#include "../../core/Scene/SceneNode.h"
#include "../../core/Scene/Light.h"
#include "../../core/Scene/Skybox.h"
#include "../../core/Mesh/Mesh.h"

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
#include "Graphics/GraphicsEngine/interface/TextureView.h"

#include <string>

using namespace Diligent;

// ======= 工具函数模板实现 =======
template<typename T>
void DiligentRenderer::UpdateCB(IBuffer* buf, const T& data)
{
    void* p = nullptr;
    m_pImmediateContext->MapBuffer(buf, MAP_WRITE, MAP_FLAG_DISCARD, p);
    std::memcpy(p, &data, sizeof(T));
    m_pImmediateContext->UnmapBuffer(buf, MAP_WRITE);
}

// 显式实例化常用类型
template void DiligentRenderer::UpdateCB<DiligentRenderer::VSConstantsCPU>(IBuffer*, const VSConstantsCPU&);
template void DiligentRenderer::UpdateCB<DiligentRenderer::PSMaterialCPU>(IBuffer*, const PSMaterialCPU&);
template void DiligentRenderer::UpdateCB<DiligentRenderer::PSSceneCPU>(IBuffer*, const PSSceneCPU&);
template void DiligentRenderer::UpdateCB<DiligentRenderer::PSConstantsCPU>(IBuffer*, const PSConstantsCPU&);
template void DiligentRenderer::UpdateCB<Moon::Matrix4x4>(IBuffer*, const Moon::Matrix4x4&);

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
        CreateDefaultWhiteTexture();  // 创建默认白色纹理
        CreateVSConstants();
        CreateMainPass();  // 主渲染管线（用于正常场景渲染）
        CreateSkyboxPass(); // Skybox 渲染管线
        PrecomputeIBL();    // 预计算 IBL 资源
        
        // 绑定 IBL 纹理到主渲染管线
        if (m_pEquirectHDR_SRV) {
            auto* equirectVar = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_EquirectMap");
            if (equirectVar) {
                equirectVar->Set(m_pEquirectHDR_SRV);
                MOON_LOG_INFO("DiligentRenderer", "IBL equirectangular texture bound to main pipeline");
            } else {
                MOON_LOG_ERROR("DiligentRenderer", "Failed to get g_EquirectMap variable from SRB");
            }
        } else {
            // 如果 HDR 加载失败，使用默认纹理
            if (m_pDefaultWhiteTextureSRV) {
                auto* equirectVar = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_EquirectMap");
                if (equirectVar) {
                    equirectVar->Set(m_pDefaultWhiteTextureSRV);
                }
            }
        }
        
        // 注意：g_AlbedoMap 是 MUTABLE 变量，必须在每次渲染前通过 BindAlbedoTexture() 设置，不在这里初始化

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

// Forward declarations to separate compilation units
// These functions are now implemented in DiligentRendererInit.cpp
// void DiligentRenderer::CreateDeviceAndSwapchain(const RenderInitParams& params);
// void DiligentRenderer::CreateVSConstants();
// void DiligentRenderer::CreateDefaultWhiteTexture();

void DiligentRenderer::CreateMainPass()
{
    // 从文件加载 PBR 着色器
    std::string vsCode = DiligentRendererUtils::LoadShaderSource("PBR.vs.hlsl");
    std::string psCode = DiligentRendererUtils::LoadShaderSource("PBR.ps.hlsl");
    
    if (vsCode.empty() || psCode.empty()) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to load PBR shaders");
        return;
    }

    // Create Vertex Shader
    RefCntAutoPtr<IShader> vs;
    {
        ShaderCreateInfo ci{};
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name = "Main VS";
        ci.Desc.UseCombinedTextureSamplers = true;  // 关键：启用组合纹理采样器
        ci.Source = vsCode.c_str();
        m_pDevice->CreateShader(ci, &vs);
    }

    // Create Pixel Shader
    RefCntAutoPtr<IShader> ps;
    {
        ShaderCreateInfo ci{};
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ci.Desc.Name = "Main PS";
        ci.Desc.UseCombinedTextureSamplers = true;  // 关键：启用组合纹理采样器
        ci.Source = psCode.c_str();
        m_pDevice->CreateShader(ci, &ps);
    }

    // 使用统一的 Vertex Layout（避免手写重复声明）
    LayoutElement layout[4];  // ⭐ 从3改成4（添加了UV）
    Uint32 numElements;
    DiligentRendererUtils::GetVertexLayout(layout, numElements);

    // 定义着色器资源变量（纹理需要设置为 MUTABLE 或 DYNAMIC）
    ShaderResourceVariableDesc Vars[] = {
        {SHADER_TYPE_PIXEL, "g_AlbedoMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_ARMMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_NormalMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_EquirectMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };

    GraphicsPipelineStateCreateInfo pci{};
    pci.PSODesc.Name = "Main PSO";
    pci.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    pci.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    pci.PSODesc.ResourceLayout.Variables = Vars;
    pci.PSODesc.ResourceLayout.NumVariables = _countof(Vars);
    
    // 允许动态绑定纹理
    pci.Flags = PSO_CREATE_FLAG_NONE;
    
    // 暂时不使用 ImmutableSamplers，尝试动态绑定
    // (Tutorial03 使用 ImmutableSamplers，但我们的多纹理配置可能有所不同)
    
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
    
    // 检查并绑定 Vertex Shader 的常量缓冲区
    auto* vsConstants = m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
    if (vsConstants) {
        vsConstants->Set(m_pVSConstants);
    } else {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to get VS Constants variable");
    }
    
    // 检查并绑定 Pixel Shader 的常量缓冲区
    auto* psMaterialConstants = m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "MaterialConstants");
    if (psMaterialConstants) {
        psMaterialConstants->Set(m_pPSMaterialConstants);
    } else {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to get PS MaterialConstants variable");
    }
    
    auto* psSceneConstants = m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "SceneConstants");
    if (psSceneConstants) {
        psSceneConstants->Set(m_pPSSceneConstants);
    } else {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to get PS SceneConstants variable");
    }
    
    // 创建 SRB（MUTABLE 变量会在渲染时动态绑定）
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

// ======= PBR 材质参数 =======
void DiligentRenderer::SetMaterialParameters(float metallic, float roughness, const Moon::Vector3& baseColor)
{
    PSMaterialCPU material{};
    material.metallic = metallic;
    material.roughness = roughness;
    material.baseColor = baseColor;
    UpdateCB(m_pPSMaterialConstants, material);
}

// ======= 场景参数更新 =======
void DiligentRenderer::SetCameraPosition(const Moon::Vector3& position)
{
    // 更新缓存中的相机位置
    m_SceneDataCache.cameraPosition = position;
    
    // 上传到 GPU
    UpdateCB(m_pPSSceneConstants, m_SceneDataCache);
}

// ======= 更新场景光源 =======
void DiligentRenderer::UpdateSceneLights(Moon::Scene* scene)
{
    if (!scene) {
        // 无场景，清空光源
        m_SceneDataCache.lightDirection = Moon::Vector3(0.0f, -1.0f, 0.0f);
        m_SceneDataCache.lightColor = Moon::Vector3(1.0f, 1.0f, 1.0f);
        m_SceneDataCache.lightIntensity = 0.0f;
        UpdateCB(m_pPSSceneConstants, m_SceneDataCache);
        return;
    }
    
    // 默认无光源
    m_SceneDataCache.lightDirection = Moon::Vector3(0.0f, -1.0f, 0.0f);
    m_SceneDataCache.lightColor = Moon::Vector3(1.0f, 1.0f, 1.0f);
    m_SceneDataCache.lightIntensity = 0.0f;  // 0 = 无光源，shader 中会检测这个值
    
    // 查找第一个启用的方向光
    scene->Traverse([&](Moon::SceneNode* node) {
        if (m_SceneDataCache.lightIntensity > 0.0f) return;  // 已找到光源
        
        Moon::Light* light = node->GetComponent<Moon::Light>();
        if (light && light->IsEnabled() && 
            light->GetType() == Moon::Light::Type::Directional) {
            
            m_SceneDataCache.lightDirection = light->GetDirection();
            m_SceneDataCache.lightColor = light->GetColor();
            m_SceneDataCache.lightIntensity = light->GetIntensity();
        }
    });
    
    // 上传到 GPU（保留了 cameraPosition）
    UpdateCB(m_pPSSceneConstants, m_SceneDataCache);
}

// ======= 更新场景 Skybox =======
void DiligentRenderer::UpdateSceneSkybox(Moon::Scene* scene)
{
    if (!scene) return;
    
    // 查找第一个启用的 Skybox 组件
    Moon::Skybox* activeSkybox = nullptr;
    scene->Traverse([&](Moon::SceneNode* node) {
        if (activeSkybox) return;  // 已找到
        
        Moon::Skybox* skybox = node->GetComponent<Moon::Skybox>();
        if (skybox && skybox->IsEnabled()) {
            activeSkybox = skybox;
        }
    });
    
    // 如果找到 Skybox 且需要重新加载
    if (activeSkybox && activeSkybox->NeedsReload()) {
        const std::string& path = activeSkybox->GetEnvironmentMapPath();
        if (!path.empty()) {
            LoadEnvironmentMap(path.c_str());
            activeSkybox->ClearReloadFlag();
            MOON_LOG_INFO("DiligentRenderer", "Loaded environment map from Skybox component: {}", path);
        }
    }
}

// Resource management functions are now in DiligentRendererResources.cpp

void DiligentRenderer::DrawMesh(Moon::Mesh* mesh, const Moon::Matrix4x4& world)
{
    if (!mesh || !mesh->IsValid()) return;

    auto* gpu = GetOrCreateMeshResources(mesh);

    // 更新 VS 常量
    Moon::Matrix4x4 wvp = world * m_ViewProj;
    VSConstantsCPU cbuf{};
    cbuf.WorldViewProjT = DiligentRendererUtils::Transpose(wvp);
    cbuf.WorldT = DiligentRendererUtils::Transpose(world);
    UpdateCB(m_pVSConstants, cbuf);

    // 设置管线状态
    m_pImmediateContext->SetPipelineState(m_pPSO);
    
    // 绑定 VB/IB
    Uint64 offset = 0;
    IBuffer* vbs[] = { gpu->VB };
    m_pImmediateContext->SetVertexBuffers(0, 1, vbs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(gpu->IB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // 提交着色器资源（包括纹理）
    m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs da{};
    da.IndexType = VT_UINT32;
    da.NumIndices = static_cast<Uint32>(gpu->IndexCount);
    da.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(da);
}

// IBL/Skybox functions are now in DiligentRendererIBL.cpp

void DiligentRenderer::DrawCube(const Moon::Matrix4x4& world)
{
    // Legacy method - not used with scene system
}

// Picking functions are now in DiligentRendererPicking.cpp

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
