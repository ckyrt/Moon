#include "DiligentRenderer.h"
#include "DiligentRendererUtils.h"

// Moon Engine
#include "../../core/Logging/Logger.h"
#include "../../core/Scene/Scene.h"
#include "../../core/Scene/SceneNode.h"
#include "../../core/Scene/Light.h"
#include "../../core/Scene/Skybox.h"
#include "../../core/Scene/Material.h"
#include "../../core/Scene/MeshRenderer.h"
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
#include "Graphics/GraphicsEngine/interface/Texture.h"
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
template void DiligentRenderer::UpdateCB<DiligentRenderer::PointShadowConstantsCPU>(IBuffer*, const PointShadowConstantsCPU&);
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
        CreateShadowPass();  // Shadow map depth-only pipeline
        CreatePointShadowPass();
        CreateMainPass();  // 主渲染管线（用于正常场景渲染，不透明物体）
        CreateTransparentPass();  // 透明物体渲染管线（Alpha Blending）
        CreateShadowMapResources();  // Shadow map texture + bind to SRBs
        CreatePointShadowMapResources();
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
        
        // 绑定 BRDF LUT 到主渲染管线
        if (m_pBRDF_LUT_SRV) {
            auto* brdfVar = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_BRDF_LUT");
            if (brdfVar) {
                brdfVar->Set(m_pBRDF_LUT_SRV);
                MOON_LOG_INFO("DiligentRenderer", "BRDF LUT bound to main pipeline");
            } else {
                MOON_LOG_ERROR("DiligentRenderer", "Failed to get g_BRDF_LUT variable from SRB");
            }
        } else {
            MOON_LOG_WARN("DiligentRenderer", "BRDF LUT not available");
        }
        
        // 绑定 IBL 纹理到透明渲染管线
        if (m_pEquirectHDR_SRV) {
            auto* equirectVar = m_pTransparentSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_EquirectMap");
            if (equirectVar) {
                equirectVar->Set(m_pEquirectHDR_SRV);
                MOON_LOG_INFO("DiligentRenderer", "IBL equirectangular texture bound to transparent pipeline");
            }
        } else {
            if (m_pDefaultWhiteTextureSRV) {
                auto* equirectVar = m_pTransparentSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_EquirectMap");
                if (equirectVar) {
                    equirectVar->Set(m_pDefaultWhiteTextureSRV);
                }
            }
        }
        
        if (m_pBRDF_LUT_SRV) {
            auto* brdfVar = m_pTransparentSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_BRDF_LUT");
            if (brdfVar) {
                brdfVar->Set(m_pBRDF_LUT_SRV);
                MOON_LOG_INFO("DiligentRenderer", "BRDF LUT bound to transparent pipeline");
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
        {SHADER_TYPE_PIXEL, "g_AOMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_RoughnessMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_MetalnessMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_NormalMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_EquirectMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_BRDF_LUT", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_ShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_PointShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
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

    auto* psShadowConstants = m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "ShadowConstants");
    if (psShadowConstants) {
        psShadowConstants->Set(m_pShadowConstants);
    } else {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to get PS ShadowConstants variable");
    }

    auto* psPointShadowConstants = m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PointShadowConstants");
    if (psPointShadowConstants) {
        psPointShadowConstants->Set(m_pPointShadowConstants);
    } else {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to get PS PointShadowConstants variable");
    }
    
    // 创建 SRB（MUTABLE 变量会在渲染时动态绑定）
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);

    MOON_LOG_INFO("DiligentRenderer", "Main PSO created (opaque objects)");
}

void DiligentRenderer::CreateTransparentPass()
{
    // 透明物体使用与不透明物体相同的shader，只是PSO配置不同
    std::string vsCode = DiligentRendererUtils::LoadShaderSource("PBR.vs.hlsl");
    std::string psCode = DiligentRendererUtils::LoadShaderSource("PBR.ps.hlsl");
    
    if (vsCode.empty() || psCode.empty()) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to load PBR shaders for transparent pass");
        return;
    }

    // Create Vertex Shader
    RefCntAutoPtr<IShader> vs;
    {
        ShaderCreateInfo ci{};
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name = "Transparent VS";
        ci.Desc.UseCombinedTextureSamplers = true;
        ci.Source = vsCode.c_str();
        m_pDevice->CreateShader(ci, &vs);
    }

    // Create Pixel Shader
    RefCntAutoPtr<IShader> ps;
    {
        ShaderCreateInfo ci{};
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ci.Desc.Name = "Transparent PS";
        ci.Desc.UseCombinedTextureSamplers = true;
        ci.Source = psCode.c_str();
        m_pDevice->CreateShader(ci, &ps);
    }

    LayoutElement layout[4];
    Uint32 numElements;
    DiligentRendererUtils::GetVertexLayout(layout, numElements);

    ShaderResourceVariableDesc Vars[] = {
        {SHADER_TYPE_PIXEL, "g_AlbedoMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_AOMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_RoughnessMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_MetalnessMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_NormalMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_EquirectMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_BRDF_LUT", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_ShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL, "g_PointShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };

    GraphicsPipelineStateCreateInfo pci{};
    pci.PSODesc.Name = "Transparent PSO";
    pci.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    pci.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    pci.PSODesc.ResourceLayout.Variables = Vars;
    pci.PSODesc.ResourceLayout.NumVariables = _countof(Vars);
    pci.Flags = PSO_CREATE_FLAG_NONE;
    
    pci.GraphicsPipeline.NumRenderTargets = 1;
    pci.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    pci.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    pci.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pci.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    
    // ✅ 业界标准透明渲染配置：深度测试开启，深度写入关闭
    pci.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    pci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;  // 关键：不写入深度
    
    // ✅ 启用 Alpha 混合（标准混合公式）
    pci.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = True;
    pci.GraphicsPipeline.BlendDesc.RenderTargets[0].SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    pci.GraphicsPipeline.BlendDesc.RenderTargets[0].DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    pci.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendOp = BLEND_OPERATION_ADD;
    pci.GraphicsPipeline.BlendDesc.RenderTargets[0].SrcBlendAlpha = BLEND_FACTOR_ONE;
    pci.GraphicsPipeline.BlendDesc.RenderTargets[0].DestBlendAlpha = BLEND_FACTOR_ZERO;
    pci.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendOpAlpha = BLEND_OPERATION_ADD;
    
    pci.GraphicsPipeline.InputLayout.LayoutElements = layout;
    pci.GraphicsPipeline.InputLayout.NumElements = numElements;

    pci.pVS = vs;
    pci.pPS = ps;

    m_pDevice->CreateGraphicsPipelineState(pci, &m_pTransparentPSO);
    
    // 绑定常量缓冲区（与不透明物体共享）
    auto* vsConstants = m_pTransparentPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants");
    if (vsConstants) {
        vsConstants->Set(m_pVSConstants);
    }
    
    auto* psMaterialConstants = m_pTransparentPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "MaterialConstants");
    if (psMaterialConstants) {
        psMaterialConstants->Set(m_pPSMaterialConstants);
    }
    
    auto* psSceneConstants = m_pTransparentPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "SceneConstants");
    if (psSceneConstants) {
        psSceneConstants->Set(m_pPSSceneConstants);
    }

    auto* psShadowConstants = m_pTransparentPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "ShadowConstants");
    if (psShadowConstants) {
        psShadowConstants->Set(m_pShadowConstants);
    }

    auto* psPointShadowConstants = m_pTransparentPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PointShadowConstants");
    if (psPointShadowConstants) {
        psPointShadowConstants->Set(m_pPointShadowConstants);
    }
    
    m_pTransparentPSO->CreateShaderResourceBinding(&m_pTransparentSRB, true);

    MOON_LOG_INFO("DiligentRenderer", "Transparent PSO created (depth write disabled, alpha blending enabled)");
}

void DiligentRenderer::CreateShadowPass()
{
    std::string vsCode = DiligentRendererUtils::LoadShaderSource("ShadowDepth.vs.hlsl");
    if (vsCode.empty()) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to load ShadowDepth.vs.hlsl");
        return;
    }

    RefCntAutoPtr<IShader> vs;
    {
        ShaderCreateInfo ci{};
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name = "Shadow Depth VS";
        ci.Desc.UseCombinedTextureSamplers = true;
        ci.Source = vsCode.c_str();
        m_pDevice->CreateShader(ci, &vs);
    }

    GraphicsPipelineStateCreateInfo pci{};
    pci.PSODesc.Name = "Shadow PSO";
    pci.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // Depth-only
    pci.GraphicsPipeline.NumRenderTargets = 0;
    pci.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_UNKNOWN;
    pci.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
    pci.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pci.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    pci.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    LayoutElement layout[4];
    Uint32 numElements;
    DiligentRendererUtils::GetVertexLayout(layout, numElements);
    pci.GraphicsPipeline.InputLayout.LayoutElements = layout;
    pci.GraphicsPipeline.InputLayout.NumElements = numElements;

    pci.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    pci.pVS = vs;
    pci.pPS = nullptr;

    m_pDevice->CreateGraphicsPipelineState(pci, &m_pShadowPSO);

    if (m_pShadowPSO) {
        auto* shadowCB = m_pShadowPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "ShadowVSConstants");
        if (shadowCB) {
            shadowCB->Set(m_pShadowVSConstants);
        } else {
            MOON_LOG_ERROR("DiligentRenderer", "Failed to get ShadowVSConstants variable");
        }
    }

    MOON_LOG_INFO("DiligentRenderer", "Shadow PSO created (depth-only)");

    // Create SRB even if we only use static resources.
    // Diligent debug layer expects resources to be committed before draw calls.
    if (m_pShadowPSO) {
        m_pShadowSRB.Release();
        m_pShadowPSO->CreateShaderResourceBinding(&m_pShadowSRB, true);
    }
}

void DiligentRenderer::CreateShadowMapResources()
{
    if (!m_pDevice) return;

    TextureDesc sm{};
    sm.Name = "Shadow Map";
    sm.Type = RESOURCE_DIM_TEX_2D;
    sm.Width = m_ShadowMapSize;
    sm.Height = m_ShadowMapSize;
    sm.Format = TEX_FORMAT_D32_FLOAT;
    sm.MipLevels = 1;
    sm.Usage = USAGE_DEFAULT;
    sm.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;

    m_pShadowMap.Release();
    m_pShadowMapSRV.Release();
    m_pShadowMapDSV.Release();

    m_pDevice->CreateTexture(sm, nullptr, &m_pShadowMap);
    if (!m_pShadowMap) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create shadow map texture");
        return;
    }

    m_pShadowMapSRV = m_pShadowMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    m_pShadowMapDSV = m_pShadowMap->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

    // Comparison sampler (see Diligent Tutorial13_ShadowMap)
    SamplerDesc cmpSampler{};
    cmpSampler.ComparisonFunc = COMPARISON_FUNC_LESS_EQUAL;
    cmpSampler.MinFilter = FILTER_TYPE_COMPARISON_LINEAR;
    cmpSampler.MagFilter = FILTER_TYPE_COMPARISON_LINEAR;
    cmpSampler.MipFilter = FILTER_TYPE_COMPARISON_LINEAR;
    cmpSampler.AddressU = TEXTURE_ADDRESS_CLAMP;
    cmpSampler.AddressV = TEXTURE_ADDRESS_CLAMP;
    cmpSampler.AddressW = TEXTURE_ADDRESS_CLAMP;

    RefCntAutoPtr<ISampler> pSampler;
    m_pDevice->CreateSampler(cmpSampler, &pSampler);
    if (m_pShadowMapSRV && pSampler) {
        m_pShadowMapSRV->SetSampler(pSampler);
    }

    // Bind to SRBs for main/transparent passes
    if (m_pSRB && m_pShadowMapSRV) {
        if (auto* var = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap")) {
            var->Set(m_pShadowMapSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        }
    }
    if (m_pTransparentSRB && m_pShadowMapSRV) {
        if (auto* var = m_pTransparentSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap")) {
            var->Set(m_pShadowMapSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        }
    }

    // Initialize shadow constants
    ShadowConstantsCPU sc{};
    sc.WorldToShadowClipT = DiligentRendererUtils::Transpose(Moon::Matrix4x4{});
    sc.shadowMapTexelSize[0] = 1.0f / float(m_ShadowMapSize);
    sc.shadowMapTexelSize[1] = 1.0f / float(m_ShadowMapSize);
    sc.shadowBias = 0.0015f;
    sc.shadowStrength = 1.0f;
    UpdateCB(m_pShadowConstants, sc);

    MOON_LOG_INFO("DiligentRenderer", "Shadow map created: %ux%u", m_ShadowMapSize, m_ShadowMapSize);
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
void DiligentRenderer::SetMaterialParameters(Moon::Material* material)
{
    if (!material) {
        // 无材质，使用默认值
        PSMaterialCPU mat{};
        mat.metallic = 0.0f;
        mat.roughness = 0.5f;
        mat.triplanarTiling = 0.5f;
        mat.mappingMode = 0.0f;  // UV模式
        mat.baseColor = Moon::Vector3(1.0f, 1.0f, 1.0f);
        mat.triplanarBlend = 4.0f;
        mat.hasNormalMap = 0.0f;
        mat.opacity = 1.0f;  // 不透明
        mat.transmissionColor = Moon::Vector3(1.0f, 1.0f, 1.0f);
        UpdateCB(m_pPSMaterialConstants, mat);
        return;
    }
    
    PSMaterialCPU mat{};
    mat.metallic = material->GetMetallic();
    mat.roughness = material->GetRoughness();
    mat.triplanarTiling = material->GetTriplanarTiling();
    
    // ✅ 设置mapping mode（0.0 = UV, 1.0 = Triplanar）
    mat.mappingMode = (material->GetMappingMode() == Moon::MappingMode::Triplanar) ? 1.0f : 0.0f;
    
    mat.baseColor = material->GetBaseColor();
    mat.triplanarBlend = material->GetTriplanarBlend();
    
    // ✅ 设置hasNormalMap flag
    mat.hasNormalMap = material->HasNormalMap() ? 1.0f : 0.0f;
    
    // ✅ 设置透明度参数
    mat.opacity = material->GetOpacity();
    mat.transmissionColor = material->GetTransmissionColor();
    
    UpdateCB(m_pPSMaterialConstants, mat);
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

        m_SceneDataCache.pointLightPosition = Moon::Vector3(0.0f, 0.0f, 0.0f);
        m_SceneDataCache.pointLightColor = Moon::Vector3(1.0f, 1.0f, 1.0f);
        m_SceneDataCache.pointLightIntensity = 0.0f;
        m_SceneDataCache.pointLightRange = 0.0f;
        m_SceneDataCache.pointLightAttenuation = Moon::Vector3(1.0f, 0.0f, 0.0f);

        UpdateCB(m_pPSSceneConstants, m_SceneDataCache);
        return;
    }
    
    // 默认无光源
    m_SceneDataCache.lightDirection = Moon::Vector3(0.0f, -1.0f, 0.0f);
    m_SceneDataCache.lightColor = Moon::Vector3(1.0f, 1.0f, 1.0f);
    m_SceneDataCache.lightIntensity = 0.0f;  // 0 = 无光源，shader 中会检测这个值

    m_SceneDataCache.pointLightPosition = Moon::Vector3(0.0f, 0.0f, 0.0f);
    m_SceneDataCache.pointLightColor = Moon::Vector3(1.0f, 1.0f, 1.0f);
    m_SceneDataCache.pointLightIntensity = 0.0f;
    m_SceneDataCache.pointLightRange = 0.0f;
    m_SceneDataCache.pointLightAttenuation = Moon::Vector3(1.0f, 0.0f, 0.0f);

    m_PointLightCastsShadows = false;
    
    bool foundDirectional = false;
    bool foundPoint = false;

    // 查找第一个启用的方向光 + 点光源
    scene->Traverse([&](Moon::SceneNode* node) {
        if (foundDirectional && foundPoint) return;

        Moon::Light* light = node->GetComponent<Moon::Light>();
        if (!light || !light->IsEnabled()) {
            return;
        }

        if (!foundDirectional && light->GetType() == Moon::Light::Type::Directional) {
            m_SceneDataCache.lightDirection = light->GetDirection();
            m_SceneDataCache.lightColor = light->GetColor();
            m_SceneDataCache.lightIntensity = light->GetIntensity();
            foundDirectional = (m_SceneDataCache.lightIntensity > 0.0f);
        }

        if (!foundPoint && light->GetType() == Moon::Light::Type::Point) {
            m_SceneDataCache.pointLightPosition = node->GetTransform()->GetWorldPosition();
            m_SceneDataCache.pointLightColor = light->GetColor();
            m_SceneDataCache.pointLightIntensity = light->GetIntensity();
            m_SceneDataCache.pointLightRange = light->GetRange();
            float c = 1.0f, l = 0.0f, q = 0.0f;
            light->GetAttenuation(c, l, q);
            m_SceneDataCache.pointLightAttenuation = Moon::Vector3(c, l, q);
            m_PointLightCastsShadows = light->GetCastShadows();
            foundPoint = (m_SceneDataCache.pointLightIntensity > 0.0f);
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
    
    // 更新 SceneConstants 中的环境贴图标志（标准引擎做法）
    if (activeSkybox && m_pEquirectHDR_SRV) {
        m_SceneDataCache.hasEnvironmentMap = 1.0f;  // 有天空盒
    } else {
        m_SceneDataCache.hasEnvironmentMap = 0.0f;  // 无天空盒
    }
    UpdateCB(m_pPSSceneConstants, m_SceneDataCache);
    
    // 如果没有找到激活的 Skybox，清除所有 skybox 和 IBL 资源以停止渲染
    if (!activeSkybox) {
        if (m_pSkyboxSRB || m_pEquirectHDR) {
            // Skybox 渲染资源
            m_pSkyboxSRB.Release();
            
            // HDR 环境贴图
            m_pEquirectHDR_SRV.Release();
            m_pEquirectHDR.Release();
            
            // Cubemap 环境贴图
            m_pEnvironmentMapSRV.Release();
            m_pEnvironmentMap.Release();
            
            // IBL 预计算贴图
            m_pIrradianceMapSRV.Release();
            m_pIrradianceMap.Release();
            m_pPrefilteredEnvMapSRV.Release();
            m_pPrefilteredEnvMap.Release();
            
            MOON_LOG_INFO("DiligentRenderer", "Cleared skybox and IBL resources (no active skybox in scene)");
        }
    }
}

// Resource management functions are now in DiligentRendererResources.cpp

void DiligentRenderer::DrawMesh(Moon::Mesh* mesh, const Moon::Matrix4x4& world)
{
    if (!mesh || !mesh->IsValid()) return;

    auto* gpu = GetOrCreateMeshResources(mesh);

    if (m_IsRenderingShadow) {
        // Shadow pass: world * lightVP
        Moon::Matrix4x4 wvp = world * m_LightViewProj;
        ShadowVSConstantsCPU cbuf{};
        cbuf.WorldViewProjT = DiligentRendererUtils::Transpose(wvp);
        UpdateCB(m_pShadowVSConstants, cbuf);

        m_pImmediateContext->SetPipelineState(m_pShadowPSO);

        if (m_pShadowSRB) {
            m_pImmediateContext->CommitShaderResources(m_pShadowSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    } else if (m_IsRenderingPointShadow) {
        // Point shadow pass: world * faceVP
        Moon::Matrix4x4 wvp = world * m_PointLightFaceViewProj;
        VSConstantsCPU cbuf{};
        cbuf.WorldViewProjT = DiligentRendererUtils::Transpose(wvp);
        cbuf.WorldT = DiligentRendererUtils::Transpose(world);
        UpdateCB(m_pVSConstants, cbuf);

        m_pImmediateContext->SetPipelineState(m_pPointShadowPSO);
        if (m_pPointShadowSRB) {
            m_pImmediateContext->CommitShaderResources(m_pPointShadowSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    } else {
        // Main pass constants
        Moon::Matrix4x4 wvp = world * m_ViewProj;
        VSConstantsCPU cbuf{};
        cbuf.WorldViewProjT = DiligentRendererUtils::Transpose(wvp);
        cbuf.WorldT = DiligentRendererUtils::Transpose(world);
        UpdateCB(m_pVSConstants, cbuf);

        // ✅ 根据当前渲染状态选择PSO和SRB
        auto* pso = m_IsRenderingTransparent ? m_pTransparentPSO.RawPtr() : m_pPSO.RawPtr();
        auto* srb = m_IsRenderingTransparent ? m_pTransparentSRB.RawPtr() : m_pSRB.RawPtr();

        // 设置管线状态
        m_pImmediateContext->SetPipelineState(pso);

        // 提交着色器资源（包括纹理）
        m_pImmediateContext->CommitShaderResources(srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    
    // 绑定 VB/IB
    Uint64 offset = 0;
    IBuffer* vbs[] = { gpu->VB };
    m_pImmediateContext->SetVertexBuffers(0, 1, vbs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(gpu->IB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs da{};
    da.IndexType = VT_UINT32;
    da.NumIndices = static_cast<Uint32>(gpu->IndexCount);
    da.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(da);
}

void DiligentRenderer::CreatePointShadowPass()
{
    std::string vsCode = DiligentRendererUtils::LoadShaderSource("PointShadowDepth.vs.hlsl");
    std::string psCode = DiligentRendererUtils::LoadShaderSource("PointShadowDepth.ps.hlsl");
    if (vsCode.empty() || psCode.empty()) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to load point shadow depth shaders");
        return;
    }

    RefCntAutoPtr<IShader> vs;
    {
        ShaderCreateInfo ci{};
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ci.Desc.Name = "Point Shadow Depth VS";
        ci.Desc.UseCombinedTextureSamplers = true;
        ci.Source = vsCode.c_str();
        m_pDevice->CreateShader(ci, &vs);
    }

    RefCntAutoPtr<IShader> ps;
    {
        ShaderCreateInfo ci{};
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ci.Desc.Name = "Point Shadow Depth PS";
        ci.Desc.UseCombinedTextureSamplers = true;
        ci.Source = psCode.c_str();
        m_pDevice->CreateShader(ci, &ps);
    }

    GraphicsPipelineStateCreateInfo pci{};
    pci.PSODesc.Name = "Point Shadow PSO";
    pci.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    pci.GraphicsPipeline.NumRenderTargets = 1;
    pci.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_R32_FLOAT;
    pci.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
    pci.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pci.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    pci.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    LayoutElement layout[4];
    Uint32 numElements;
    DiligentRendererUtils::GetVertexLayout(layout, numElements);
    pci.GraphicsPipeline.InputLayout.LayoutElements = layout;
    pci.GraphicsPipeline.InputLayout.NumElements = numElements;

    pci.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    pci.pVS = vs;
    pci.pPS = ps;

    m_pDevice->CreateGraphicsPipelineState(pci, &m_pPointShadowPSO);
    if (!m_pPointShadowPSO) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create point shadow PSO");
        return;
    }

    if (auto* vsCB = m_pPointShadowPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")) {
        vsCB->Set(m_pVSConstants);
    } else {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to get point shadow VS Constants variable");
    }

    if (auto* psCB = m_pPointShadowPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PointShadowConstants")) {
        psCB->Set(m_pPointShadowConstants);
    } else {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to get point shadow PS PointShadowConstants variable");
    }

    m_pPointShadowPSO->CreateShaderResourceBinding(&m_pPointShadowSRB, true);
    MOON_LOG_INFO("DiligentRenderer", "Point shadow PSO created");
}

void DiligentRenderer::CreatePointShadowMapResources()
{
    if (!m_pDevice) return;

    TextureDesc cube{};
    cube.Name = "Point Shadow Cubemap";
    cube.Type = RESOURCE_DIM_TEX_CUBE;
    cube.Width = m_PointShadowMapSize;
    cube.Height = m_PointShadowMapSize;
    cube.ArraySize = 6;
    cube.Format = TEX_FORMAT_R32_FLOAT;
    cube.MipLevels = 1;
    cube.Usage = USAGE_DEFAULT;
    cube.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;

    m_pPointShadowCube.Release();
    m_pPointShadowCubeSRV.Release();
    for (auto& rtv : m_pPointShadowCubeRTV)
        rtv.Release();

    m_pDevice->CreateTexture(cube, nullptr, &m_pPointShadowCube);
    if (!m_pPointShadowCube) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create point shadow cubemap texture");
        return;
    }

    m_pPointShadowCubeSRV = m_pPointShadowCube->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // Sampler for cubemap distance lookup
    SamplerDesc samp{};
    samp.MinFilter = FILTER_TYPE_LINEAR;
    samp.MagFilter = FILTER_TYPE_LINEAR;
    samp.MipFilter = FILTER_TYPE_LINEAR;
    samp.AddressU = TEXTURE_ADDRESS_CLAMP;
    samp.AddressV = TEXTURE_ADDRESS_CLAMP;
    samp.AddressW = TEXTURE_ADDRESS_CLAMP;
    RefCntAutoPtr<ISampler> pSampler;
    m_pDevice->CreateSampler(samp, &pSampler);
    if (m_pPointShadowCubeSRV && pSampler) {
        m_pPointShadowCubeSRV->SetSampler(pSampler);
    }

    // RTV per face
    for (Uint32 face = 0; face < 6; ++face) {
        TextureViewDesc rtvDesc{};
        rtvDesc.Name = "Point Shadow Cubemap Face RTV";
        rtvDesc.ViewType = TEXTURE_VIEW_RENDER_TARGET;
        // Cubemap is a 2D texture array with 6 slices in D3D11.
        // RTV for a single face must be a 2D-array view with one slice.
        rtvDesc.TextureDim = RESOURCE_DIM_TEX_2D_ARRAY;
        rtvDesc.FirstArraySlice = face;
        rtvDesc.NumArraySlices = 1;
        rtvDesc.MostDetailedMip = 0;
        rtvDesc.NumMipLevels = 1;
        rtvDesc.Format = cube.Format;
        m_pPointShadowCube->CreateView(rtvDesc, &m_pPointShadowCubeRTV[face]);
    }

    // Depth buffer shared by all faces
    TextureDesc depth{};
    depth.Name = "Point Shadow Depth";
    depth.Type = RESOURCE_DIM_TEX_2D;
    depth.Width = m_PointShadowMapSize;
    depth.Height = m_PointShadowMapSize;
    depth.MipLevels = 1;
    depth.ArraySize = 1;
    depth.Format = TEX_FORMAT_D32_FLOAT;
    depth.Usage = USAGE_DEFAULT;
    depth.BindFlags = BIND_DEPTH_STENCIL;

    m_pPointShadowDepth.Release();
    m_pPointShadowDepthDSV.Release();
    m_pDevice->CreateTexture(depth, nullptr, &m_pPointShadowDepth);
    if (!m_pPointShadowDepth) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create point shadow depth texture");
        return;
    }
    m_pPointShadowDepthDSV = m_pPointShadowDepth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

    // Bind cubemap SRV to SRBs for main/transparent passes
    if (m_pSRB && m_pPointShadowCubeSRV) {
        if (auto* var = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_PointShadowMap")) {
            var->Set(m_pPointShadowCubeSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        }
    }
    if (m_pTransparentSRB && m_pPointShadowCubeSRV) {
        if (auto* var = m_pTransparentSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_PointShadowMap")) {
            var->Set(m_pPointShadowCubeSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        }
    }

    // Initialize point shadow constants (disabled by default)
    PointShadowConstantsCPU pc{};
    pc.lightPosition = Moon::Vector3(0.0f, 0.0f, 0.0f);
    pc.invRange = 0.0f;
    pc.bias = 0.002f;
    pc.strength = 0.0f;
    UpdateCB(m_pPointShadowConstants, pc);

    MOON_LOG_INFO("DiligentRenderer", "Point shadow cubemap created: %ux%u", m_PointShadowMapSize, m_PointShadowMapSize);
}

void DiligentRenderer::RenderPointShadowMap(Moon::Scene* scene)
{
    if (!scene) return;
    if (!m_pPointShadowPSO || !m_pPointShadowCubeSRV || !m_pPointShadowDepthDSV) return;

    // Require an enabled point light
    if (m_SceneDataCache.pointLightIntensity <= 0.0f || m_SceneDataCache.pointLightRange <= 0.0f || !m_PointLightCastsShadows) {
        PointShadowConstantsCPU pc{};
        pc.lightPosition = m_SceneDataCache.pointLightPosition;
        pc.invRange = (m_SceneDataCache.pointLightRange > 1e-4f) ? (1.0f / m_SceneDataCache.pointLightRange) : 0.0f;
        pc.bias = 0.002f;
        pc.strength = 0.0f;
        UpdateCB(m_pPointShadowConstants, pc);
        return;
    }

    const Moon::Vector3 lightPos = m_SceneDataCache.pointLightPosition;
    const float range = m_SceneDataCache.pointLightRange;

    static bool s_PointShadowActiveLogged = false;
    if (!s_PointShadowActiveLogged) {
        MOON_LOG_INFO("DiligentRenderer", "Point shadow active: lightPos=(%.2f, %.2f, %.2f), range=%.2f", lightPos.x, lightPos.y, lightPos.z, range);
        s_PointShadowActiveLogged = true;
    }

    PointShadowConstantsCPU pc{};
    pc.lightPosition = lightPos;
    pc.invRange = (range > 1e-4f) ? (1.0f / range) : 0.0f;
    pc.bias = 0.002f;
    pc.strength = 1.0f;
    UpdateCB(m_pPointShadowConstants, pc);

    constexpr float OPACITY_THRESHOLD = 0.99f;
    const float nearZ = 0.1f;
    const float farZ = std::max(range, 0.2f);
    const float fov = 3.14159265359f * 0.5f; // 90 deg

    struct FaceDesc { Moon::Vector3 Dir; Moon::Vector3 Up; };
    const FaceDesc Faces[6] = {
        {Moon::Vector3( 1, 0, 0), Moon::Vector3(0, 1, 0)},
        {Moon::Vector3(-1, 0, 0), Moon::Vector3(0, 1, 0)},
        {Moon::Vector3( 0, 1, 0), Moon::Vector3(0, 0,-1)},
        {Moon::Vector3( 0,-1, 0), Moon::Vector3(0, 0, 1)},
        {Moon::Vector3( 0, 0, 1), Moon::Vector3(0, 1, 0)},
        {Moon::Vector3( 0, 0,-1), Moon::Vector3(0, 1, 0)},
    };

    Viewport vp{};
    vp.Width = static_cast<float>(m_PointShadowMapSize);
    vp.Height = static_cast<float>(m_PointShadowMapSize);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;

    Moon::Matrix4x4 proj = Moon::Matrix4x4::PerspectiveFovLH(fov, 1.0f, nearZ, farZ);

    m_IsRenderingPointShadow = true;
    for (Uint32 face = 0; face < 6; ++face) {
        if (!m_pPointShadowCubeRTV[face]) {
            MOON_LOG_ERROR("DiligentRenderer", "Point shadow RTV for face %u is null", face);
            continue;
        }

        ITextureView* rtvs[] = { m_pPointShadowCubeRTV[face] };
        m_pImmediateContext->SetRenderTargets(1, rtvs, m_pPointShadowDepthDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const float ClearValue[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        m_pImmediateContext->ClearRenderTarget(rtvs[0], ClearValue, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(m_pPointShadowDepthDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetViewports(1, &vp, 0, 0);

        Moon::Matrix4x4 view = Moon::Matrix4x4::LookAtLH(lightPos, lightPos + Faces[face].Dir, Faces[face].Up);
        m_PointLightFaceViewProj = view * proj;

        scene->Traverse([&](Moon::SceneNode* node) {
            auto* meshRenderer = node->GetComponent<Moon::MeshRenderer>();
            if (!meshRenderer || !meshRenderer->IsEnabled() || !meshRenderer->IsVisible()) {
                return;
            }
            auto* material = node->GetComponent<Moon::Material>();
            if (material && material->IsEnabled() && material->GetOpacity() < OPACITY_THRESHOLD) {
                return;
            }
            meshRenderer->Render(this);
        });
    }
    m_IsRenderingPointShadow = false;

    // Restore main framebuffer targets & viewport (no clear)
    ITextureView* rtvs[] = { m_pRTV };
    m_pImmediateContext->SetRenderTargets(1, rtvs, m_pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Viewport mainVp{};
    mainVp.Width = static_cast<float>(m_Width);
    mainVp.Height = static_cast<float>(m_Height);
    mainVp.MinDepth = 0.f;
    mainVp.MaxDepth = 1.f;
    m_pImmediateContext->SetViewports(1, &mainVp, 0, 0);
}

void DiligentRenderer::RenderShadowMap(Moon::Scene* scene, Moon::Camera* camera)
{
    if (!scene || !camera) return;
    if (!m_pShadowPSO || !m_pShadowMapDSV || !m_pShadowMapSRV) return;

    // If there is no directional light, disable shadows
    if (m_SceneDataCache.lightIntensity <= 0.0f) {
        ShadowConstantsCPU sc{};
        sc.WorldToShadowClipT = DiligentRendererUtils::Transpose(Moon::Matrix4x4{});
        sc.shadowMapTexelSize[0] = 1.0f / float(m_ShadowMapSize);
        sc.shadowMapTexelSize[1] = 1.0f / float(m_ShadowMapSize);
        sc.shadowBias = 0.0015f;
        sc.shadowStrength = 0.0f;
        UpdateCB(m_pShadowConstants, sc);
        return;
    }

    // Build light view-projection (simple stable ortho around camera)
    Moon::Vector3 lightDir = m_SceneDataCache.lightDirection.Normalized();
    Moon::Vector3 center = camera->GetPosition();

    float shadowDistance = 60.0f;
    float orthoSize = 80.0f; // covers an 80x80 region
    float nearZ = 0.1f;
    float farZ = 200.0f;

    Moon::Vector3 eye = center - lightDir * shadowDistance;
    Moon::Vector3 up(0.0f, 1.0f, 0.0f);
    if (std::abs(Moon::Vector3::Dot(up, lightDir)) > 0.99f) {
        up = Moon::Vector3(0.0f, 0.0f, 1.0f);
    }

    Moon::Matrix4x4 lightView = Moon::Matrix4x4::LookAtLH(eye, center, up);
    Moon::Matrix4x4 lightProj = Moon::Matrix4x4::OrthoLH(orthoSize, orthoSize, nearZ, farZ);
    m_LightViewProj = lightView * lightProj;

    // Update shadow sampling constants for main pass
    ShadowConstantsCPU sc{};
    sc.WorldToShadowClipT = DiligentRendererUtils::Transpose(m_LightViewProj);
    sc.shadowMapTexelSize[0] = 1.0f / float(m_ShadowMapSize);
    sc.shadowMapTexelSize[1] = 1.0f / float(m_ShadowMapSize);
    sc.shadowBias = 0.0015f;
    sc.shadowStrength = 1.0f;
    UpdateCB(m_pShadowConstants, sc);

    // Render into shadow map depth
    m_pImmediateContext->SetRenderTargets(0, nullptr, m_pShadowMapDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(m_pShadowMapDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Viewport vp{};
    vp.Width = static_cast<float>(m_ShadowMapSize);
    vp.Height = static_cast<float>(m_ShadowMapSize);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    m_pImmediateContext->SetViewports(1, &vp, 0, 0);

    // Draw opaque meshes only
    m_IsRenderingShadow = true;
    constexpr float OPACITY_THRESHOLD = 0.99f;
    scene->Traverse([&](Moon::SceneNode* node) {
        auto* meshRenderer = node->GetComponent<Moon::MeshRenderer>();
        if (!meshRenderer || !meshRenderer->IsEnabled() || !meshRenderer->IsVisible()) {
            return;
        }
        auto* material = node->GetComponent<Moon::Material>();
        if (material && material->IsEnabled() && material->GetOpacity() < OPACITY_THRESHOLD) {
            return; // skip transparent
        }
        meshRenderer->Render(this);
    });
    m_IsRenderingShadow = false;

    // Restore main framebuffer targets & viewport (no clear)
    ITextureView* rtvs[] = { m_pRTV };
    m_pImmediateContext->SetRenderTargets(1, rtvs, m_pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Viewport mainVp{};
    mainVp.Width = static_cast<float>(m_Width);
    mainVp.Height = static_cast<float>(m_Height);
    mainVp.MinDepth = 0.f;
    mainVp.MaxDepth = 1.f;
    m_pImmediateContext->SetViewports(1, &mainVp, 0, 0);
}

void DiligentRenderer::SetRenderingTransparent(bool transparent)
{
    m_IsRenderingTransparent = transparent;
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
    m_pPSMaterialConstants.Release();
    m_pPSSceneConstants.Release();
    m_pShadowConstants.Release();
    m_pPointShadowConstants.Release();

    // Shadow pass
    m_pShadowMapDSV.Release();
    m_pShadowMapSRV.Release();
    m_pShadowMap.Release();
    m_pShadowPSO.Release();
    m_pShadowSRB.Release();
    m_pShadowVSConstants.Release();

    // Point shadow pass
    m_pPointShadowDepthDSV.Release();
    m_pPointShadowDepth.Release();
    for (auto& rtv : m_pPointShadowCubeRTV)
        rtv.Release();
    m_pPointShadowCubeSRV.Release();
    m_pPointShadowCube.Release();
    m_pPointShadowPSO.Release();
    m_pPointShadowSRB.Release();
    
    // Skybox 渲染管线
    m_pSkyboxSRB.Release();
    m_pSkyboxPSO.Release();
    m_pSkyboxVSConstants.Release();
    m_pSkyboxIB.Release();
    m_pSkyboxVB.Release();
    
    // IBL 资源
    m_pBRDF_LUT_SRV.Release();
    m_pBRDF_LUT_RTV.Release();
    m_pBRDF_LUT.Release();
    m_pEquirectHDR_SRV.Release();
    m_pEquirectHDR.Release();
    m_pEnvironmentMapSRV.Release();
    m_pEnvironmentMap.Release();
    m_pIrradianceMapSRV.Release();
    m_pIrradianceMap.Release();
    m_pPrefilteredEnvMapSRV.Release();
    m_pPrefilteredEnvMap.Release();
    
    // 纹理缓存
    m_TextureCache.clear();

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
