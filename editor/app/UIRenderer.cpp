#include "UIRenderer.h"
#include "../engine/core/Logging/Logger.h"
#include "../engine/render/diligent/DiligentRendererUtils.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceVariable.h"
#include <vector>

using namespace Diligent;

// 全屏四边形顶点数据
struct UIVertex {
    float x, y;    // Position
    float u, v;    // UV
};

UIRenderer::UIRenderer(IRenderDevice* device, IDeviceContext* context)
    : m_pDevice(device), m_pContext(context), m_Initialized(false)
{
}

bool UIRenderer::Initialize()
{
    if (m_Initialized) {
        return true;
    }

    CreateFullscreenQuad();
    CreatePipelineState();

    if (m_pPSO && m_pVertexBuffer) {
        m_Initialized = true;
        MOON_LOG_INFO("UIRenderer", "Initialized successfully");
        return true;
    }

    MOON_LOG_ERROR("UIRenderer", "Failed to initialize");
    return false;
}

void UIRenderer::CreateFullscreenQuad()
{
    // 全屏四边形顶点（两个三角形）
    // NDC 坐标：左上(-1, 1), 右下(1, -1)
    UIVertex vertices[] = {
        // Position      UV
        { -1.0f,  1.0f,  0.0f, 0.0f },  // 左上
        {  1.0f,  1.0f,  1.0f, 0.0f },  // 右上
        { -1.0f, -1.0f,  0.0f, 1.0f },  // 左下
        
        {  1.0f,  1.0f,  1.0f, 0.0f },  // 右上
        {  1.0f, -1.0f,  1.0f, 1.0f },  // 右下
        { -1.0f, -1.0f,  0.0f, 1.0f }   // 左下
    };

    BufferDesc bufDesc;
    bufDesc.Name = "UI Quad VB";
    bufDesc.Usage = USAGE_IMMUTABLE;
    bufDesc.BindFlags = BIND_VERTEX_BUFFER;
    bufDesc.Size = sizeof(vertices);

    BufferData bufData;
    bufData.pData = vertices;
    bufData.DataSize = sizeof(vertices);

    m_pDevice->CreateBuffer(bufDesc, &bufData, &m_pVertexBuffer);

    if (!m_pVertexBuffer) {
        MOON_LOG_ERROR("UIRenderer", "Failed to create vertex buffer");
    }
}

void UIRenderer::CreatePipelineState()
{
    // 加载 Shader 源码
    std::string vsCode = DiligentRendererUtils::LoadShaderSource("UI.vs.hlsl");
    std::string psCode = DiligentRendererUtils::LoadShaderSource("UI.ps.hlsl");

    // 创建 Shader
    ShaderCreateInfo shaderCI;
    shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    RefCntAutoPtr<IShader> pVS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shaderCI.Desc.Name = "UI VS";
        shaderCI.Source = vsCode.c_str();
        shaderCI.EntryPoint = "main";
        m_pDevice->CreateShader(shaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        shaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shaderCI.Desc.Name = "UI PS";
        shaderCI.Source = psCode.c_str();
        shaderCI.EntryPoint = "main";
        m_pDevice->CreateShader(shaderCI, &pPS);
    }

    if (!pVS || !pPS) {
        MOON_LOG_ERROR("UIRenderer", "Failed to create shaders");
        return;
    }

    // 创建 Pipeline State
    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "UI PSO";
    psoCI.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    psoCI.GraphicsPipeline.NumRenderTargets = 1;
    psoCI.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM_SRGB;
    psoCI.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Blend State - Alpha Blending
    psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = true;
    psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0].SrcBlend = BLEND_FACTOR_ONE;
    psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0].DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendOp = BLEND_OPERATION_ADD;
    psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0].SrcBlendAlpha = BLEND_FACTOR_ONE;
    psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0].DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    psoCI.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendOpAlpha = BLEND_OPERATION_ADD;

    // Depth State - 不使用深度测试
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;

    // Rasterizer State
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;

    // Vertex Layout
    LayoutElement layoutElems[] = {
        { 0, 0, 2, VT_FLOAT32, false },  // Position
        { 1, 0, 2, VT_FLOAT32, false }   // UV
    };
    psoCI.GraphicsPipeline.InputLayout.LayoutElements = layoutElems;
    psoCI.GraphicsPipeline.InputLayout.NumElements = _countof(layoutElems);

    psoCI.pVS = pVS;
    psoCI.pPS = pPS;

    // 纹理变量
    ShaderResourceVariableDesc variables[] = {
        { SHADER_TYPE_PIXEL, "g_UITexture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
    };
    psoCI.PSODesc.ResourceLayout.Variables = variables;
    psoCI.PSODesc.ResourceLayout.NumVariables = _countof(variables);

    // 采样器（使用静态采样器）
    SamplerDesc samplerDesc;
    samplerDesc.MinFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MagFilter = FILTER_TYPE_LINEAR;
    samplerDesc.MipFilter = FILTER_TYPE_LINEAR;
    samplerDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = TEXTURE_ADDRESS_CLAMP;

    ImmutableSamplerDesc immutableSamplers[] = {
        { SHADER_TYPE_PIXEL, "g_Sampler", samplerDesc }
    };
    psoCI.PSODesc.ResourceLayout.ImmutableSamplers = immutableSamplers;
    psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(immutableSamplers);

    m_pDevice->CreateGraphicsPipelineState(psoCI, &m_pPSO);

    if (!m_pPSO) {
        MOON_LOG_ERROR("UIRenderer", "Failed to create PSO");
        return;
    }

    // 创建 SRB
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);

    MOON_LOG_INFO("UIRenderer", "Pipeline state created");
}

void UIRenderer::RenderUI(UITextureManager* textureManager, ITextureView* renderTargetView)
{
    if (!m_Initialized || !textureManager || !textureManager->IsInitialized()) {
        return;
    }

    // 设置纹理
    if (auto* pVar = m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_UITexture")) {
        pVar->Set(textureManager->GetTextureSRV());
    }

    // 设置 PSO 和 SRB
    m_pContext->SetPipelineState(m_pPSO);
    m_pContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // 设置顶点缓冲
    IBuffer* pBuffers[] = { m_pVertexBuffer };
    m_pContext->SetVertexBuffers(0, 1, pBuffers, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // 绘制全屏四边形
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 6;
    drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pContext->Draw(drawAttrs);
}
