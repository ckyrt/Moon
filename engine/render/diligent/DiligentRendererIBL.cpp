// IBL (Image-Based Lighting) and Skybox rendering for DiligentRenderer
// This module contains:
// - CreateSkyboxPass: Creates skybox rendering pipeline and resources
// - RenderSkybox: Renders skybox using equirectangular HDR map
// - LoadEnvironmentMap: Loads HDR environment map from file
// - ConvertEquirectangularToCubemap: Converts equirect map to cubemap
// - PrecomputeIBL: Precomputes BRDF LUT for IBL

#include "DiligentRenderer.h"
#include "DiligentRendererUtils.h"
#include "../../core/Assets/AssetPaths.h"

// Diligent includes
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/TextureView.h"

// stb_image for loading HDR files
#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

using namespace Diligent;

// ======= Skybox 渲染管线 =======
void DiligentRenderer::CreateSkyboxPass()
{
    // 1. 创建 Skybox 立方体顶点缓冲（中心在原点，边长很大以"包围"场景）
    struct SkyboxVertex {
        float x, y, z;
    };
    
    const float skyboxSize = 1000.0f; // 很大的立方体
    
    // Skybox 立方体顶点（8个顶点）
    SkyboxVertex skyboxVertices[] = {
        // 前面（+Z）
        {-skyboxSize, -skyboxSize,  skyboxSize}, // 0
        { skyboxSize, -skyboxSize,  skyboxSize}, // 1
        { skyboxSize,  skyboxSize,  skyboxSize}, // 2
        {-skyboxSize,  skyboxSize,  skyboxSize}, // 3
        // 后面（-Z）
        {-skyboxSize, -skyboxSize, -skyboxSize}, // 4
        { skyboxSize, -skyboxSize, -skyboxSize}, // 5
        { skyboxSize,  skyboxSize, -skyboxSize}, // 6
        {-skyboxSize,  skyboxSize, -skyboxSize}  // 7
    };
    
    BufferDesc vbDesc{};
    vbDesc.Name = "Skybox VB";
    vbDesc.BindFlags = BIND_VERTEX_BUFFER;
    vbDesc.Usage = USAGE_IMMUTABLE;
    vbDesc.Size = sizeof(skyboxVertices);
    
    BufferData vbData;
    vbData.pData = skyboxVertices;
    vbData.DataSize = sizeof(skyboxVertices);
    m_pDevice->CreateBuffer(vbDesc, &vbData, &m_pSkyboxVB);
    
    // 2. 创建 Skybox 立方体索引缓冲
    uint32_t skyboxIndices[] = {
        // 前面
        0, 1, 2,  0, 2, 3,
        // 右面
        1, 5, 6,  1, 6, 2,
        // 后面
        5, 4, 7,  5, 7, 6,
        // 左面
        4, 0, 3,  4, 3, 7,
        // 顶面
        3, 2, 6,  3, 6, 7,
        // 底面
        4, 5, 1,  4, 1, 0
    };
    
    BufferDesc ibDesc{};
    ibDesc.Name = "Skybox IB";
    ibDesc.BindFlags = BIND_INDEX_BUFFER;
    ibDesc.Usage = USAGE_IMMUTABLE;
    ibDesc.Size = sizeof(skyboxIndices);
    
    BufferData ibData;
    ibData.pData = skyboxIndices;
    ibData.DataSize = sizeof(skyboxIndices);
    m_pDevice->CreateBuffer(ibDesc, &ibData, &m_pSkyboxIB);
    
    // 3. 创建 Skybox VS 常量缓冲（VP 矩阵，移除平移）
    BufferDesc cbDesc{};
    cbDesc.Name = "Skybox VS Constants";
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    cbDesc.Size = 64; // 4x4 matrix
    m_pDevice->CreateBuffer(cbDesc, nullptr, &m_pSkyboxVSConstants);
    
    // 4. 创建 Skybox 着色器
    const char* skyboxVS = R"(
cbuffer SkyboxConstants {
    float4x4 g_ViewProj; // ViewProj with translation removed
};

struct VSInput {
    float3 Pos : ATTRIB0;
};

struct PSInput {
    float4 Pos : SV_POSITION;
    float3 TexCoord : TEXCOORD0; // 使用顶点位置作为 cubemap 采样方向
};

void main(in VSInput i, out PSInput o) {
    // 使用顶点位置作为 cubemap 采样坐标
    o.TexCoord = i.Pos;
    
    // 变换到裁剪空间
    o.Pos = mul(float4(i.Pos, 1.0), g_ViewProj);
    
    // 设置 z = w，确保 Skybox 在最远处（深度 = 1.0）
    o.Pos.z = o.Pos.w;
}
)";
    
    const char* skyboxPS = R"(
Texture2D g_EquirectMap;
SamplerState g_EquirectMap_sampler;

struct PSInput {
    float4 Pos : SV_POSITION;
    float3 TexCoord : TEXCOORD0;
};

// 将 3D 方向转换为 equirectangular UV 坐标
float2 DirToEquirectUV(float3 dir) {
    const float PI = 3.14159265359;
    // atan2 返回 [-PI, PI]，asin 返回 [-PI/2, PI/2]
    float u = atan2(dir.z, dir.x) / (2.0 * PI) + 0.5;
    // 注意：V 坐标需要反转（1.0 - v）因为纹理坐标从上到下
    float v = 1.0 - (asin(dir.y) / PI + 0.5);
    return float2(u, v);
}

float4 main(in PSInput i) : SV_Target {
    // 从方向向量计算 equirectangular UV 坐标
    float3 dir = normalize(i.TexCoord);
    float2 uv = DirToEquirectUV(dir);
    
    // 从 equirectangular 贴图采样
    float3 color = g_EquirectMap.Sample(g_EquirectMap_sampler, uv).rgb;
    
    // 简单的 tone mapping
    color = color / (color + 1.0); // Reinhard tone mapping
    
    return float4(color, 1.0);
}
)";
    
    ShaderCreateInfo vsInfo;
    vsInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    vsInfo.Desc.ShaderType = SHADER_TYPE_VERTEX;
    vsInfo.Desc.Name = "Skybox VS";
    vsInfo.Source = skyboxVS;
    vsInfo.EntryPoint = "main";
    vsInfo.Desc.UseCombinedTextureSamplers = true;
    
    RefCntAutoPtr<IShader> pSkyboxVS;
    m_pDevice->CreateShader(vsInfo, &pSkyboxVS);
    
    ShaderCreateInfo psInfo;
    psInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    psInfo.Desc.ShaderType = SHADER_TYPE_PIXEL;
    psInfo.Desc.Name = "Skybox PS";
    psInfo.Source = skyboxPS;
    psInfo.EntryPoint = "main";
    psInfo.Desc.UseCombinedTextureSamplers = true;
    
    RefCntAutoPtr<IShader> pSkyboxPS;
    m_pDevice->CreateShader(psInfo, &pSkyboxPS);
    
    // 5. 创建 Skybox PSO
    GraphicsPipelineStateCreateInfo psoInfo;
    psoInfo.PSODesc.Name = "Skybox PSO";
    psoInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    
    psoInfo.pVS = pSkyboxVS;
    psoInfo.pPS = pSkyboxPS;
    
    // 输入布局（只有位置）
    LayoutElement skyboxLayout[] = {
        {0, 0, 3, VT_FLOAT32, False}
    };
    psoInfo.GraphicsPipeline.InputLayout.LayoutElements = skyboxLayout;
    psoInfo.GraphicsPipeline.InputLayout.NumElements = 1;
    
    psoInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    
    // 光栅化状态
    psoInfo.GraphicsPipeline.RasterizerDesc.FillMode = FILL_MODE_SOLID;
    psoInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE; // 不剔除（从内部看天空盒）
    
    // 深度测试：LESS_EQUAL（因为 Skybox z=w，深度为1.0）
    psoInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    psoInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False; // 不写入深度
    psoInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS_EQUAL;
    
    // 混合状态
    psoInfo.GraphicsPipeline.BlendDesc.RenderTargets[0].BlendEnable = False;
    
    // RT 格式
    psoInfo.GraphicsPipeline.NumRenderTargets = 1;
    psoInfo.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    psoInfo.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    
    // 资源签名（常量缓冲）
    psoInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    
    m_pDevice->CreateGraphicsPipelineState(psoInfo, &m_pSkyboxPSO);
    
    // 注意：SRB 将在加载环境贴图后创建，因为需要绑定 cubemap 纹理
    
    MOON_LOG_INFO("DiligentRenderer", "Skybox pass created successfully!");
}

void DiligentRenderer::RenderSkybox()
{
    if (!m_pSkyboxPSO || !m_pSkyboxSRB) return;
    
    // 1. 使用当前的 ViewProj 矩阵（Skybox 足够大，不需要移除平移）
    Moon::Matrix4x4 vpT = DiligentRendererUtils::Transpose(m_ViewProj);
    
    // 更新常量缓冲
    UpdateCB(m_pSkyboxVSConstants, vpT);
    
    // 2. 设置 PSO 和 SRB
    m_pImmediateContext->SetPipelineState(m_pSkyboxPSO);
    m_pImmediateContext->CommitShaderResources(m_pSkyboxSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // 3. 绑定顶点和索引缓冲
    IBuffer* pVBs[] = { m_pSkyboxVB };
    m_pImmediateContext->SetVertexBuffers(0, 1, pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_pSkyboxIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // 4. 绘制
    DrawIndexedAttribs drawAttrs;
    drawAttrs.IndexType = VT_UINT32;
    drawAttrs.NumIndices = 36; // 6 faces * 2 triangles * 3 vertices
    drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    
    m_pImmediateContext->DrawIndexed(drawAttrs);
}

// ======= 加载 HDR 环境贴图 =======
void DiligentRenderer::LoadEnvironmentMap(const char* filepath)
{
    std::string fullPath = Moon::Assets::BuildAssetPath(filepath);
    
    MOON_LOG_INFO("DiligentRenderer", "Loading environment map from: %s", fullPath.c_str());
    
    // 使用 stb_image 加载 HDR 文件
    int width, height, channels;
    float* hdrData = stbi_loadf(fullPath.c_str(), &width, &height, &channels, 4); // 强制 4 通道 RGBA
    
    if (!hdrData) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to load HDR file: %s", fullPath.c_str());
        return;
    }
    
    MOON_LOG_INFO("DiligentRenderer", "Loaded HDR: %dx%d, %d channels", width, height, channels);
    
    // 创建 2D 纹理用于 equirectangular HDR
    TextureDesc texDesc;
    texDesc.Name = "HDR Environment Map (Equirectangular)";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    texDesc.MipLevels = 0;  // ✅ 0 = 自动生成完整 mipmap 链（log2(max(w,h)) + 1）
    texDesc.Usage = USAGE_DEFAULT;  // ✅ DEFAULT 允许 GPU 写入（生成 mipmap）
    texDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;  // ✅ RT 用于生成 mipmap
    texDesc.MiscFlags = MISC_TEXTURE_FLAG_GENERATE_MIPS;  // ✅ 请求自动生成 mipmap
    
    // ⚠️ 关键：创建纹理时不提供初始数据（nullptr），稍后通过 UpdateTexture 上传
    m_pDevice->CreateTexture(texDesc, nullptr, &m_pEquirectHDR);
    
    if (!m_pEquirectHDR) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create HDR texture");
        stbi_image_free(hdrData);
        return;
    }
    
    // 准备上传数据到 mipmap level 0
    TextureSubResData subresData;
    subresData.pData = hdrData;
    subresData.Stride = width * 4 * sizeof(float); // RGBA32F
    
    // 上传数据到 level 0
    Uint32 mipLevel = 0;
    Uint32 arraySlice = 0;
    Box updateBox;
    updateBox.MinX = 0;
    updateBox.MaxX = width;
    updateBox.MinY = 0;
    updateBox.MaxY = height;
    updateBox.MinZ = 0;
    updateBox.MaxZ = 1;
    
    m_pImmediateContext->UpdateTexture(m_pEquirectHDR, mipLevel, arraySlice, 
        updateBox, subresData, 
        RESOURCE_STATE_TRANSITION_MODE_TRANSITION, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    // 释放 stb_image 加载的数据
    stbi_image_free(hdrData);
    
    // 创建 SRV
    m_pEquirectHDR_SRV = m_pEquirectHDR->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    
    // ✅ 生成 mipmap（Unity/Unreal 标准流程）
    // 只有 level 0 有数据，GPU 自动生成剩余 level
    m_pImmediateContext->GenerateMips(m_pEquirectHDR_SRV);
    
    MOON_LOG_INFO("DiligentRenderer", "HDR texture created: {}x{}, format: RGBA32F with mipmaps", width, height);
    MOON_LOG_INFO("DiligentRenderer", "HDR SRV created successfully");
    
    // 转换 equirectangular 到 cubemap（暂时跳过，直接使用 equirect）
    ConvertEquirectangularToCubemap(m_pEquirectHDR);
}

// ======= 将 Equirectangular HDR 转换为 Cubemap =======
void DiligentRenderer::ConvertEquirectangularToCubemap(ITexture* pEquirectangularMap)
{
    if (!pEquirectangularMap) return;
    
    MOON_LOG_INFO("DiligentRenderer", "Converting equirectangular map to cubemap...");
    
    // 创建目标 Cubemap 纹理（512x512 per face, RGBA16F）
    TextureDesc cubemapDesc;
    cubemapDesc.Name = "Environment Cubemap";
    cubemapDesc.Type = RESOURCE_DIM_TEX_CUBE;
    cubemapDesc.Width = 512;
    cubemapDesc.Height = 512;
    cubemapDesc.ArraySize = 6; // Cubemap 有 6 个面
    cubemapDesc.Format = TEX_FORMAT_RGBA16_FLOAT;
    cubemapDesc.MipLevels = 1;
    cubemapDesc.Usage = USAGE_DEFAULT;
    cubemapDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    
    m_pDevice->CreateTexture(cubemapDesc, nullptr, &m_pEnvironmentMap);
    if (!m_pEnvironmentMap) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create environment cubemap texture");
        return;
    }
    
    m_pEnvironmentMapSRV = m_pEnvironmentMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    
    // 简化版：暂时跳过实际的转换渲染，直接创建空的 cubemap
    // 后续步骤会实现完整的 equirect -> cubemap 转换
    // TODO: 实现使用 6 个 view matrices 渲染到 cubemap 6 个面
    
    MOON_LOG_INFO("DiligentRenderer", "Environment cubemap created (512x512 per face)");
    
    // 创建 Skybox SRB 并绑定环境贴图和常量缓冲
    if (m_pSkyboxPSO && m_pEquirectHDR_SRV) {
        // 绑定静态变量（常量缓冲）- 允许覆盖以支持重新加载
        m_pSkyboxPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "SkyboxConstants")->Set(m_pSkyboxVSConstants, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        
        // 绑定静态变量（Equirectangular 环境贴图）- 允许覆盖以支持重新加载
        m_pSkyboxPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_EquirectMap")->Set(m_pEquirectHDR_SRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        
        // 创建 SRB
        m_pSkyboxPSO->CreateShaderResourceBinding(&m_pSkyboxSRB, true);
        
        MOON_LOG_INFO("DiligentRenderer", "Skybox SRB created with equirectangular HDR map");
        MOON_LOG_INFO("DiligentRenderer", "Equirect HDR SRV pointer: {}", (void*)m_pEquirectHDR_SRV.RawPtr());
    } else {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create Skybox SRB - PSO: {}, SRV: {}", 
                      (void*)m_pSkyboxPSO.RawPtr(), (void*)m_pEquirectHDR_SRV.RawPtr());
    }
}

// ======= IBL 预计算 =======
void DiligentRenderer::PrecomputeIBL()
{
    MOON_LOG_INFO("DiligentRenderer", "Precomputing IBL resources...");
    
    // ========== 1. 创建 BRDF LUT 纹理（256x256, RG16F） ==========
    TextureDesc texDesc;
    texDesc.Name = "BRDF LUT";
    texDesc.Type = RESOURCE_DIM_TEX_2D;
    texDesc.Width = 256;
    texDesc.Height = 256;
    texDesc.Format = TEX_FORMAT_RG16_FLOAT;
    texDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    texDesc.Usage = USAGE_DEFAULT;
    texDesc.MipLevels = 1;
    
    m_pDevice->CreateTexture(texDesc, nullptr, &m_pBRDF_LUT);
    m_pBRDF_LUT_SRV = m_pBRDF_LUT->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    m_pBRDF_LUT_RTV = m_pBRDF_LUT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    
    // ========== 2. 加载 BRDF LUT Shader ==========
    // 从文件加载shader而不是内嵌
    std::string brdfVS = DiligentRendererUtils::LoadShaderSource("BRDFIntegration.vs.hlsl");
    std::string brdfPS = DiligentRendererUtils::LoadShaderSource("BRDFIntegration.ps.hlsl");
    
    if (brdfVS.empty() || brdfPS.empty()) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to load BRDF Integration shaders");
        return;
    }
    
    ShaderCreateInfo vsInfo;
    vsInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    vsInfo.Desc.ShaderType = SHADER_TYPE_VERTEX;
    vsInfo.Desc.Name = "BRDF LUT VS";
    vsInfo.Source = brdfVS.c_str();
    vsInfo.EntryPoint = "main";
    
    RefCntAutoPtr<IShader> pBRDF_VS;
    m_pDevice->CreateShader(vsInfo, &pBRDF_VS);
    
    ShaderCreateInfo psInfo;
    psInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    psInfo.Desc.ShaderType = SHADER_TYPE_PIXEL;
    psInfo.Desc.Name = "BRDF LUT PS";
    psInfo.Source = brdfPS.c_str();
    psInfo.EntryPoint = "main";
    
    RefCntAutoPtr<IShader> pBRDF_PS;
    m_pDevice->CreateShader(psInfo, &pBRDF_PS);
    
    // ========== 3. 创建 PSO ==========
    GraphicsPipelineStateCreateInfo psoInfo;
    psoInfo.PSODesc.Name = "BRDF LUT PSO";
    psoInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    psoInfo.pVS = pBRDF_VS;
    psoInfo.pPS = pBRDF_PS;
    
    psoInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    psoInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    
    psoInfo.GraphicsPipeline.NumRenderTargets = 1;
    psoInfo.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RG16_FLOAT;
    psoInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_UNKNOWN;
    
    RefCntAutoPtr<IPipelineState> pBRDF_PSO;
    m_pDevice->CreateGraphicsPipelineState(psoInfo, &pBRDF_PSO);
    
    // ========== 4. 渲染 BRDF LUT ==========
    ITextureView* pRTVs[] = { m_pBRDF_LUT_RTV };
    m_pImmediateContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    Viewport vp;
    vp.Width = 256.0f;
    vp.Height = 256.0f;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_pImmediateContext->SetViewports(1, &vp, 0, 0);
    
    m_pImmediateContext->SetPipelineState(pBRDF_PSO);
    
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 3; // 全屏三角形
    drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->Draw(drawAttrs);
    
    MOON_LOG_INFO("DiligentRenderer", "BRDF LUT precomputed successfully!");
}
