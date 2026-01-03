#include "DiligentRenderer.h"
#include "DiligentRendererUtils.h"

// Moon Engine
#include "../core/Logging/Logger.h"
#include "../core/Camera/Camera.h"
#include "../core/Scene/Scene.h"
#include "../core/Scene/SceneNode.h"
#include "../core/Scene/MeshRenderer.h"
#include "../core/Scene/Light.h"
#include "../core/Mesh/Mesh.h"
#include "../core/Texture/TextureManager.h"

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
        
        // 加载 HDR 环境贴图
        LoadEnvironmentMap("assets/textures/environment.hdr");
        
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
    // PBR 着色器（Cook-Torrance BRDF，支持 Albedo 贴图和 IBL）
    const char* vsCode = R"(
cbuffer Constants { 
    float4x4 g_WorldViewProj;
    float4x4 g_World;
};
struct VSInput { 
    float3 Pos    : ATTRIB0; 
    float3 Normal : ATTRIB1;
    float4 Color  : ATTRIB2;
    float2 UV     : ATTRIB3;
};
struct PSInput { 
    float4 Pos      : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 NormalWS : NORMAL;
    float4 Color    : COLOR;
    float2 UV       : TEXCOORD0;
};
void main(in VSInput i, out PSInput o) {
    o.Pos = mul(float4(i.Pos, 1.0), g_WorldViewProj);
    o.WorldPos = mul(float4(i.Pos, 1.0), g_World).xyz;
    // 修复：法线变换应该使用列向量形式（Matrix * Normal）
    // 对于统一缩放的情况，这是正确的（非统一缩放需要逆转置矩阵）
    o.NormalWS = normalize(mul((float3x3)g_World, i.Normal));
    o.Color = i.Color;
    o.UV = i.UV;
}
)";

    const char* psCode = R"(
static const float PI = 3.14159265359;

// 材质参数常量缓冲区
cbuffer MaterialConstants { 
    float g_Metallic;
    float g_Roughness;
    float2 g_Padding1;
    float3 g_BaseColor;
    float g_Padding2;
};

// 场景参数常量缓冲区（相机位置、光源等）
cbuffer SceneConstants { 
    float3 g_CameraPosition;
    float g_Padding3;
    float3 g_LightDirection;
    float g_Padding4;
    float3 g_LightColor;
    float g_LightIntensity;
};

// 纹理资源和采样器
Texture2D g_AlbedoMap;
SamplerState g_AlbedoMap_sampler;

Texture2D g_ARMMap;
SamplerState g_ARMMap_sampler;

Texture2D g_NormalMap;
SamplerState g_NormalMap_sampler;

Texture2D g_EquirectMap;
SamplerState g_EquirectMap_sampler;

struct PSInput { 
    float4 Pos      : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 NormalWS : NORMAL;
    float4 Color    : COLOR;
    float2 UV       : TEXCOORD0;
};

// 将 3D 方向转换为 equirectangular UV 坐标
float2 DirToEquirectUV(float3 dir) {
    float u = atan2(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float v = 1.0 - (asin(dir.y) / PI + 0.5);
    return float2(u, v);
}

// Fresnel-Schlick 近似（菲涅尔效应）
float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX/Trowbridge-Reitz 法线分布函数（NDF）
float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return nom / max(denom, 0.0001);
}

// Smith GGX 几何遮挡函数
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return nom / max(denom, 0.0001);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

float4 main(in PSInput i) : SV_Target {
    // 🔍 调试开关（取消注释查看不同的调试信息）
    // 0 = 正常渲染
    // 0 = 正常渲染
    // 1 = 显示法线贴图原始颜色
    // 2 = 显示世界空间法线（查看法线贴图是否影响光照）
    // 3 = 显示 ARM 贴图（R=AO, G=Roughness, B=Metallic）
    // 4 = 只显示 AO
    // 5 = 只显示 Roughness
    // 6 = 显示顶点法线（未应用法线贴图）
    // 7 = 显示法线贴图（切线空间，转换后）
    // 8 = 显示切线T（红色）
    // 9 = 显示副切线B（绿色）
    // 10 = 显示Albedo贴图
    // 11 = 只显示直接光照
    // 12 = 只显示环境光
    int debugMode = 0;  // 恢复正常渲染
    
    // 从贴图采样 Albedo
    float3 albedo = g_AlbedoMap.Sample(g_AlbedoMap_sampler, i.UV).rgb;
    
    // === ARM 贴图采样（必须在所有 debug 分支之前，确保 shader 不会优化掉）===
    float3 armSample = g_ARMMap.Sample(g_ARMMap_sampler, i.UV).rgb;
    
    // 🔍 调试模式 3: 显示 ARM 贴图
    if (debugMode == 3) {
        return float4(armSample, 1.0);
    }
    
    // 🔍 调试模式 4: 只显示 AO（红色通道）
    if (debugMode == 4) {
        float ao = armSample.r;
        return float4(ao, ao, ao, 1.0);
    }
    
    // 🔍 调试模式 5: 只显示 Roughness（绿色通道）
    if (debugMode == 5) {
        float roughness = armSample.g;
        return float4(roughness, roughness, roughness, 1.0);
    }
    
    // 🔍 调试模式 10: 显示Albedo贴图原始颜色
    if (debugMode == 10) {
        return float4(albedo, 1.0);
    }
    
    // Unity 标准做法：Albedo 贴图与 BaseColor 相乘（作为 tint color）
    if (dot(albedo, albedo) > 0.001) {
        // 贴图存在：贴图颜色 × BaseColor（用作色调调整）
        albedo *= g_BaseColor;
    } else {
        // 贴图不存在：直接使用 BaseColor
        albedo = g_BaseColor;
    }
    
    // 不要用顶点颜色调制纹理！这会让纹理变暗
    // albedo *= i.Color.rgb;
    
    // === 法线贴图采样和处理 ===
    float3 N = normalize(i.NormalWS);
    
    // 🔍 调试模式 6: 显示顶点法线（未应用法线贴图）
    if (debugMode == 6) {
        return float4(N * 0.5 + 0.5, 1.0);
    }
    
    // 采样法线贴图
    float3 normalMap = g_NormalMap.Sample(g_NormalMap_sampler, i.UV).rgb;
    
    // 🔍 调试模式 1: 显示法线贴图原始颜色
    if (debugMode == 1) {
        return float4(normalMap, 1.0);
    }
    
    // 检查法线贴图是否有效（非零）
    if (dot(normalMap, normalMap) > 0.001) {
        // 将法线从 [0,1] 转换到 [-1,1]
        normalMap = normalMap * 2.0 - 1.0;
        
        // ⚠️ 重要：DirectX 法线贴图（_nor_dx）需要翻转 Y 通道！
        // DirectX 使用 Y-down，我们使用 Y-up
        // 如果不翻转，凹凸方向会完全相反，导致看起来很平
        normalMap.y = -normalMap.y;
        
        // 🔍 调试：直接显示法线贴图（取消注释查看法线贴图是否工作）
        // return float4(normalMap * 0.5 + 0.5, 1.0);  // 将 [-1,1] 转回 [0,1] 显示
        
        // 🔍 调试模式 7: 显示转换到[-1,1]后的法线贴图
        if (debugMode == 7) {
            return float4(normalMap * 0.5 + 0.5, 1.0);
        }
        
        // === Unity/Unreal 标准法线强度处理 ===
        // 简单缩放XY，然后重建Z（保证法线长度为1）
        float normalStrength = 2.0;  // 业界标准范围：0.0-3.0
        normalMap.xy *= normalStrength;
        // 重建Z分量（使用saturate防止sqrt(负数)）
        normalMap.z = sqrt(saturate(1.0 - dot(normalMap.xy, normalMap.xy)));
        
        // === Unreal Engine 标准 TBN 计算（更稳定） ===
        // 使用屏幕空间导数，但加入更好的数值稳定性处理
        
        // 计算位置和 UV 的屏幕空间导数
        float3 dPos_dx = ddx_coarse(i.WorldPos);  // 使用粗粒度导数，更稳定
        float3 dPos_dy = ddy_coarse(i.WorldPos);
        float2 dUV_dx = ddx_coarse(i.UV);
        float2 dUV_dy = ddy_coarse(i.UV);
        
        // Unreal Engine 的 TBN 计算（处理数值不稳定）
        float3 T = dPos_dx * dUV_dy.y - dPos_dy * dUV_dx.y;
        float3 B = dPos_dy * dUV_dx.x - dPos_dx * dUV_dy.x;
        float invMax = rsqrt(max(dot(T, T), dot(B, B)));  // 快速归一化
        
        // 构建 TBN 矩阵（列向量形式）
        float3x3 TBN = float3x3(T * invMax, B * invMax, N);
        
        // 🔍 调试模式 8: 显示切线T
        if (debugMode == 8) {
            return float4(normalize(T) * 0.5 + 0.5, 1.0);
        }
        
        // 🔍 调试模式 9: 显示副切线B
        if (debugMode == 9) {
            return float4(normalize(B) * 0.5 + 0.5, 1.0);
        }
        
        // 将切线空间法线转换到世界空间
        N = normalize(mul(normalMap, TBN));
        // else: 如果 d == 0（UV 没有变化），保持原法线
    }
    
    // 🔍 调试模式 2: 显示世界空间法线（查看法线贴图是否影响光照）
    // 将 [-1,1] 范围的法线转换到 [0,1] 用于显示
    if (debugMode == 2) {
        return float4(N * 0.5 + 0.5, 1.0);
    }
    
    // === ARM 贴图处理（从之前采样的结果中提取）===
    float ao = armSample.r;          // R 通道 = AO (环境光遮蔽)
    float roughness = armSample.g;   // G 通道 = Roughness (粗糙度)
    float metallic = armSample.b;    // B 通道 = Metallic (金属度)
    
    // Unity 标准做法：贴图值 × 常量值 = 最终值
    // 如果 ARM 贴图有效，将贴图值与材质常量相乘
    if (dot(armSample, armSample) > 0.001) {
        // 贴图存在：贴图值作为基础，材质常量作为乘数
        metallic *= g_Metallic;
        roughness *= g_Roughness;
        // AO 不需要乘数，直接使用贴图值
    } else {
        // 贴图不存在：直接使用材质常量值
        metallic = g_Metallic;
        roughness = g_Roughness;
        ao = 1.0; // 默认无遮蔽
    }
    
    roughness = max(roughness, 0.25); // 增加最小粗糙度，让墙面更哑光、不那么光滑
    
    float3 V = normalize(g_CameraPosition - i.WorldPos);
    
    // 基础反射率（F0）
    float3 F0 = float3(0.04, 0.04, 0.04);  // 非金属默认值
    F0 = lerp(F0, albedo, metallic);       // 金属使用 albedo 作为 F0
    
    // 反射方向（用于环境光采样）
    float3 R = reflect(-V, N);
    
    // 采样环境贴图
    float2 envUV = DirToEquirectUV(R);
    float3 envColor = g_EquirectMap.Sample(g_EquirectMap_sampler, envUV).rgb;
    
    // === 直接光照（方向光）===
    float3 Lo = float3(0.0, 0.0, 0.0);
    
    if (g_LightIntensity > 0.0) {
        float3 L = normalize(-g_LightDirection);  // 光源方向（指向光源）
        float3 H = normalize(V + L);              // 半程向量
        float3 radiance = g_LightColor * g_LightIntensity;
        
        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        
        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        float3 specular = numerator / max(denominator, 0.001);
        
        float3 kS = F;
        float3 kD = float3(1.0, 1.0, 1.0) - kS;
        kD *= 1.0 - metallic;  // 金属没有漫反射
        
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    // === 环境光照（IBL - Image Based Lighting）===
    // Unity/Unreal 标准流程
    float3 F = FresnelSchlick(max(dot(N, V), 0.0), F0);
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    
    // 漫反射环境光：Unity/Unreal标准做法
    // ⚠️ 标准IBL: 漫反射使用预计算的Irradiance Map（球谐函数SH或卷积贴图）
    // 在没有预计算贴图的情况下，使用均匀的环境光颜色（避免方向性采样导致颜色不一致）
    // 
    // Unity默认：Sky Light使用均匀的天空颜色作为环境光基础
    // Unreal默认：使用SH（Spherical Harmonics）计算的平均环境光
    float3 irradiance = float3(0.8, 0.8, 0.8);  // 中性灰白色环境光（模拟天空平均颜色）
    
    float3 diffuse = kD * albedo * irradiance;
    
    // 镜面反射环境光：使用反射方向采样
    float3 prefilteredColor = envColor;  
    
    // Unreal Engine 标准：环境镜面反射BRDF
    float3 specular = prefilteredColor * F;
    
    // 粗糙度衰减：粗糙材质几乎没有环境镜面反射
    float smoothness = 1.0 - roughness;
    specular *= smoothness * smoothness;
    
    // ⚠️ 关键修复：大幅降低环境镜面反射，避免灰色天空覆盖漫反射颜色
    // 非金属材质的环境镜面反射应该非常弱
    specular *= lerp(0.05, 0.3, metallic);  // 非金属5%，金属30%
    
    // 组合环境光（间接光）
    float3 ambient = (diffuse + specular) * ao;  // AO只影响环境光
    
    // 🔍 调试模式 11: 只显示直接光照（不含环境光）
    if (debugMode == 11) {
        return float4(Lo, 1.0);
    }
    
    // 🔍 调试模式 12: 只显示环境光（不含直接光）
    if (debugMode == 12) {
        return float4(ambient, 1.0);
    }
    
    // 最终颜色 = 直接光 + 环境光
    float3 color = Lo + ambient;
    
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
        ci.Desc.UseCombinedTextureSamplers = true;  // 关键：启用组合纹理采样器
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
        ci.Desc.UseCombinedTextureSamplers = true;  // 关键：启用组合纹理采样器
        ci.Source = psCode;
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

// Texture binding functions are now in DiligentRendererResources.cpp

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
    // 构建相对于 exe 的完整路径
    std::string exeDir = DiligentRendererUtils::GetExecutableDirectory();
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
