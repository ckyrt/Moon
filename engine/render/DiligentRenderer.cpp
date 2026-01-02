#include "DiligentRenderer.h"

// Moon Engine
#include "../core/Logging/Logger.h"
#include "../core/Camera/Camera.h"
#include "../core/Scene/Scene.h"
#include "../core/Scene/SceneNode.h"
#include "../core/Scene/MeshRenderer.h"
#include "../core/Scene/Light.h"
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
#include "Graphics/GraphicsEngine/interface/Sampler.h"

// stb_image for loading HDR files
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Platforms/Win32/interface/Win32NativeWindow.h"
#endif

#include <string>

using namespace Diligent;

// ======= 辅助函数：获取 exe 所在目录 =======
static std::string GetExecutableDirectory()
{
#ifdef _WIN32
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string path(exePath);
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        return path.substr(0, lastSlash + 1);
    }
    return "";
#else
    // Linux/Mac 实现可以使用 readlink("/proc/self/exe") 等
    return "";
#endif
}

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
        CreateSkyboxPass(); // Skybox 渲染管线
        
        // 加载 HDR 环境贴图
        LoadEnvironmentMap("assets/textures/environment.hdr");
        
        PrecomputeIBL();    // 预计算 IBL 资源
        
        // 绑定 IBL 纹理到主渲染管线
        if (m_pEquirectHDR_SRV) {
            m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_EquirectMap")->Set(m_pEquirectHDR_SRV);
            MOON_LOG_INFO("DiligentRenderer", "IBL equirectangular texture bound to main pipeline");
        }

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
    
    // 创建 PS 材质常量缓冲区
    BufferDesc psCB{};
    psCB.Name = "PS Material Constants";
    psCB.BindFlags = BIND_UNIFORM_BUFFER;
    psCB.Usage = USAGE_DYNAMIC;
    psCB.CPUAccessFlags = CPU_ACCESS_WRITE;
    psCB.Size = sizeof(PSMaterialCPU);
    m_pDevice->CreateBuffer(psCB, nullptr, &m_pPSMaterialConstants);
    
    // 创建 PS 场景常量缓冲区
    BufferDesc psSceneCB{};
    psSceneCB.Name = "PS Scene Constants";
    psSceneCB.BindFlags = BIND_UNIFORM_BUFFER;
    psSceneCB.Usage = USAGE_DYNAMIC;
    psSceneCB.CPUAccessFlags = CPU_ACCESS_WRITE;
    psSceneCB.Size = sizeof(PSSceneCPU);
    m_pDevice->CreateBuffer(psSceneCB, nullptr, &m_pPSSceneConstants);
}

void DiligentRenderer::CreateMainPass()
{
    // PBR 着色器（Cook-Torrance BRDF，无贴图，无 IBL）
    const char* vsCode = R"(
cbuffer Constants { 
    float4x4 g_WorldViewProj;
    float4x4 g_World;
};
struct VSInput { 
    float3 Pos    : ATTRIB0; 
    float3 Normal : ATTRIB1;
    float4 Color  : ATTRIB2;
};
struct PSInput { 
    float4 Pos      : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 NormalWS : NORMAL;
    float4 Color    : COLOR;
};
void main(in VSInput i, out PSInput o) {
    o.Pos = mul(float4(i.Pos, 1.0), g_WorldViewProj);
    o.WorldPos = mul(float4(i.Pos, 1.0), g_World).xyz;
    o.NormalWS = normalize(mul(i.Normal, (float3x3)g_World));
    o.Color = i.Color;
}
)";

    const char* psCode = R"(
static const float PI = 3.14159265359;

// 材质参数常量缓冲区
cbuffer MaterialConstants { 
    float g_Metallic;
    float g_Roughness;
    float2 g_Padding;
};

// 场景参数常量缓冲区
cbuffer SceneConstants {
    float3 g_CameraPosition;
    float g_Padding2;
};

// IBL 纹理资源
Texture2D g_EquirectMap;
SamplerState g_EquirectMap_sampler;

struct PSInput { 
    float4 Pos      : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 NormalWS : NORMAL;
    float4 Color    : COLOR;
};

// ============================================================================
// PBR 函数：Distribution (D) - GGX / Trowbridge-Reitz
// ============================================================================
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return nom / denom;
}

// ============================================================================
// PBR 函数：Geometry (G) - Smith's method with Schlick-GGX
// ============================================================================
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// ============================================================================
// PBR 函数：Fresnel (F) - Schlick approximation
// ============================================================================
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// ============================================================================
// 辅助函数：将方向向量转换为 Equirectangular UV
// ============================================================================
float2 DirToEquirectUV(float3 dir)
{
    float phi = atan2(dir.z, dir.x);
    float theta = acos(dir.y);
    return float2(phi / (2.0 * PI) + 0.5, theta / PI);
}

// ============================================================================
// 主着色器：Cook-Torrance BRDF
// ============================================================================
float4 main(in PSInput i) : SV_TARGET {
    // 材质参数（从常量缓冲区读取）
    float3 albedo = i.Color.rgb;
    float metallic = g_Metallic;   // 从 CB 读取
    float roughness = g_Roughness; // 从 CB 读取
    
    // 视角方向（从相机位置计算真实方向）
    float3 viewDir = normalize(g_CameraPosition - i.WorldPos);
    
    // 固定方向光：从左上偏前方照射（更靠前，避免遮挡左上角）
    // lightDir 是光的传播方向（从光源出发），所以 Y 为负表示从上往下
    float3 lightDir = normalize(float3(-0.5, -1.0, 2.0));  // 方向：主要从前方来，从左上往下照
    float3 lightColor = float3(8.0, 8.0, 8.0);  // 强光源
    
    // 向量准备
    float3 N = normalize(i.NormalWS);
    float3 V = viewDir;
    float3 L = -lightDir;  // 指向光源（方向光需要取反）
    float3 H = normalize(V + L);  // 半角向量
    
    // F0：基础反射率（金属用 albedo，非金属用 0.04）
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);
    
    // Cook-Torrance BRDF
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    // 镜面反射项
    float3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    float3 specular = numerator / denominator;
    
    // 能量守恒：kS + kD = 1
    float3 kS = F;                          // 镜面反射比例
    float3 kD = (1.0 - kS) * (1.0 - metallic);  // 漫反射比例（金属没有漫反射）
    
    // 最终颜色：漫反射 + 镜面反射
    float NdotL = max(dot(N, L), 0.0);
    float3 Lo = (kD * albedo / PI + specular) * lightColor * NdotL;
    
    // IBL 环境反射
    float3 R = reflect(-V, N);  // 反射向量
    float2 envUV = DirToEquirectUV(normalize(R));  // 转换为 UV
    float3 envColor = g_EquirectMap.Sample(g_EquirectMap_sampler, envUV).rgb;
    
    // 环境光（简单模拟 IBL 的效果）
    float3 ambientColor = float3(0.4, 0.4, 0.45);  // 天空蓝色调
    float3 ambient = kD * albedo * ambientColor;   // 漫反射环境光
    
    // 环境镜面反射：使用采样的环境贴图颜色，受粗糙度影响
    float3 ambientSpecular = kS * envColor * (1.0 - roughness * 0.7);
    
    float3 color = ambient + ambientSpecular + Lo;
    
    // Gamma 校正（简化）
    // color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));
    
    return float4(color, i.Color.a);
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
    LayoutElement layout[3];  // ⭐ 从2改成3
    Uint32 numElements;
    GetVertexLayout(layout, numElements);

    // 定义着色器资源变量（纹理需要设置为 MUTABLE 或 DYNAMIC）
    ShaderResourceVariableDesc Vars[] = {
        {SHADER_TYPE_PIXEL, "g_EquirectMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };

    GraphicsPipelineStateCreateInfo pci{};
    pci.PSODesc.Name = "Main PSO";
    pci.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    pci.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    pci.PSODesc.ResourceLayout.Variables = Vars;
    pci.PSODesc.ResourceLayout.NumVariables = _countof(Vars);
    
    // 定义静态采样器
    SamplerDesc SamLinearClampDesc{
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    ImmutableSamplerDesc ImtblSamplers[] = {
        {SHADER_TYPE_PIXEL, "g_EquirectMap", SamLinearClampDesc}
    };
    pci.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplers;
    pci.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

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
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "MaterialConstants")->Set(m_pPSMaterialConstants);
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "SceneConstants")->Set(m_pPSSceneConstants);
    
    // 绑定环境贴图（需要在 PrecomputeIBL 后才有效，这里先创建 SRB）
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
void DiligentRenderer::SetMaterialParameters(float metallic, float roughness)
{
    PSMaterialCPU material{};
    material.metallic = metallic;
    material.roughness = roughness;
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
    VSConstantsCPU cbuf{};
    cbuf.WorldViewProjT = Transpose(wvp);
    cbuf.WorldT = Transpose(world);
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
    Moon::Matrix4x4 vpT = Transpose(m_ViewProj);
    
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
    // 构建相对于 exe 的完整路径
    std::string exeDir = GetExecutableDirectory();
    std::string fullPath = exeDir + filepath;
    
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
    texDesc.MipLevels = 1;
    texDesc.Usage = USAGE_IMMUTABLE;
    texDesc.BindFlags = BIND_SHADER_RESOURCE;
    
    // 准备初始化数据
    TextureSubResData subresData;
    subresData.pData = hdrData;
    subresData.Stride = width * 4 * sizeof(float); // RGBA32F
    
    TextureData initData;
    initData.pSubResources = &subresData;
    initData.NumSubresources = 1;
    
    m_pDevice->CreateTexture(texDesc, &initData, &m_pEquirectHDR);
    
    // 释放 stb_image 加载的数据
    stbi_image_free(hdrData);
    
    if (!m_pEquirectHDR) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create HDR texture");
        return;
    }
    
    // 创建 SRV
    m_pEquirectHDR_SRV = m_pEquirectHDR->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    
    MOON_LOG_INFO("DiligentRenderer", "HDR texture created: {}x{}, format: RGBA32F", width, height);
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
        // 绑定静态变量（常量缓冲）
        m_pSkyboxPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "SkyboxConstants")->Set(m_pSkyboxVSConstants);
        
        // 绑定静态变量（Equirectangular 环境贴图）
        m_pSkyboxPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_EquirectMap")->Set(m_pEquirectHDR_SRV);
        
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
    
    // ========== 2. 创建 BRDF LUT 预计算 Shader ==========
    // Vertex Shader：全屏三角形
    const char* brdfVS = R"(
struct VSOutput {
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

void main(uint VertexID : SV_VertexID, out VSOutput output) {
    // 全屏三角形技巧
    float2 uv = float2((VertexID << 1) & 2, VertexID & 2);
    output.UV = uv;
    output.Pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
)";
    
    // Pixel Shader：BRDF 积分计算（参考 Epic Games 的实现）
    const char* brdfPS = R"(
#define PI 3.14159265359
#define NUM_SAMPLES 512u

// Hammersley 2D 序列（低差异序列）
float2 Hammersley2D(uint i, uint N) {
    // Van der Corput sequence
    uint bits = i;
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float radicalInverse = float(bits) * 2.3283064365386963e-10; // / 0x100000000
    return float2(float(i) / float(N), radicalInverse);
}

// GGX 重要性采样
float3 ImportanceSampleGGX(float2 Xi, float roughness, float3 N) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    // 球面坐标转笛卡尔坐标
    float3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;
    
    // 切线空间 -> 世界空间
    float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangentX = normalize(cross(up, N));
    float3 tangentY = cross(N, tangentX);
    
    return tangentX * H.x + tangentY * H.y + N * H.z;
}

// Smith GGX Visibility（几何遮挡项）
float GeometrySmith_GGX(float NdotV, float NdotL, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    
    float ggxV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float ggxL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    
    return 0.5 / max(ggxV + ggxL, 0.001);
}

// BRDF 积分（Split-Sum Approximation 的第二部分）
float2 IntegrateBRDF(float roughness, float NdotV) {
    float3 V;
    V.x = sqrt(1.0 - NdotV * NdotV); // sin
    V.y = 0.0;
    V.z = NdotV; // cos
    
    float3 N = float3(0, 0, 1);
    
    float A = 0.0;
    float B = 0.0;
    
    for (uint i = 0u; i < NUM_SAMPLES; ++i) {
        float2 Xi = Hammersley2D(i, NUM_SAMPLES);
        float3 H = ImportanceSampleGGX(Xi, roughness, N);
        float3 L = 2.0 * dot(V, H) * H - V;
        
        float NdotL = saturate(L.z);
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));
        
        if (NdotL > 0.0) {
            float G = GeometrySmith_GGX(NdotV, NdotL, roughness);
            float G_Vis = G * VdotH / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);
            
            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    
    return float2(A, B) / float(NUM_SAMPLES);
}

struct PSInput {
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

float2 main(PSInput input) : SV_Target {
    float NdotV = input.UV.x;
    float roughness = input.UV.y;
    return IntegrateBRDF(roughness, NdotV);
}
)";
    
    ShaderCreateInfo vsInfo;
    vsInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    vsInfo.Desc.ShaderType = SHADER_TYPE_VERTEX;
    vsInfo.Desc.Name = "BRDF LUT VS";
    vsInfo.Source = brdfVS;
    vsInfo.EntryPoint = "main";
    
    RefCntAutoPtr<IShader> pBRDF_VS;
    m_pDevice->CreateShader(vsInfo, &pBRDF_VS);
    
    ShaderCreateInfo psInfo;
    psInfo.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    psInfo.Desc.ShaderType = SHADER_TYPE_PIXEL;
    psInfo.Desc.Name = "BRDF LUT PS";
    psInfo.Source = brdfPS;
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

    // 使用统一的 Vertex Layout（避免手写重复声明）
    // 注意：即使 picking shader 只使用 position，也必须声明完整的 layout 来保证 stride 正确
    LayoutElement layout[3];  // ⭐ 从2改成3
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
        VSConstantsCPU vsc{};
        vsc.WorldViewProjT = Transpose(wvp);
        vsc.WorldT = Transpose(world);
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
