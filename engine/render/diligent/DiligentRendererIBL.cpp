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
#include "../../core/Mesh/Mesh.h"

#include <algorithm>
#include <cmath>
#include <vector>

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

namespace {

constexpr uint32_t kPrecipitationMaxParticles = 1200;

float Hash11(uint32_t x)
{
    x ^= x >> 17;
    x *= 0xed5ad4bbU;
    x ^= x >> 11;
    x *= 0xac4c1b51U;
    x ^= x >> 15;
    x *= 0x31848babU;
    x ^= x >> 14;
    return static_cast<float>(x & 0x00ffffffU) / static_cast<float>(0x01000000U);
}

Moon::Vector3 PrecipitationTint(bool snow)
{
    return snow ? Moon::Vector3(0.96f, 0.97f, 1.00f) : Moon::Vector3(0.74f, 0.82f, 0.92f);
}

} // namespace

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
    cbDesc.Size = sizeof(SkyboxConstantsCPU);
    m_pDevice->CreateBuffer(cbDesc, nullptr, &m_pSkyboxVSConstants);
    
    // 4. 创建 Skybox 着色器
    const char* skyboxVS = R"(
cbuffer SkyboxConstants {
    float4x4 g_ViewProj; // ViewProj with translation removed
    float3 g_SkyZenithColor;
    float g_UseProceduralSky;
    float3 g_SkyHorizonColor;
    float g_SkyIntensity;
    float3 g_SunDirection;
    float g_StarIntensity;
    float3 g_SunColor;
    float g_SunDiscSize;
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

cbuffer SkyboxConstants {
    float4x4 g_ViewProj;
    float3 g_SkyZenithColor;
    float g_UseProceduralSky;
    float3 g_SkyHorizonColor;
    float g_SkyIntensity;
    float3 g_SunDirection;
    float g_StarIntensity;
    float3 g_SunColor;
    float g_SunDiscSize;
    float3 g_MoonDirection;
    float g_MoonIntensity;
    float3 g_MoonColor;
    float g_CloudCoverage;
    float g_TimeSeconds;
    float3 g_Padding0;
};

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

float Hash31(float3 p) {
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 19.19);
    return frac((p.x + p.y) * p.z);
}

float Noise3(float3 p) {
    float3 cell = floor(p);
    float3 fracPart = frac(p);
    fracPart = fracPart * fracPart * (3.0 - 2.0 * fracPart);

    float n000 = Hash31(cell + float3(0.0, 0.0, 0.0));
    float n100 = Hash31(cell + float3(1.0, 0.0, 0.0));
    float n010 = Hash31(cell + float3(0.0, 1.0, 0.0));
    float n110 = Hash31(cell + float3(1.0, 1.0, 0.0));
    float n001 = Hash31(cell + float3(0.0, 0.0, 1.0));
    float n101 = Hash31(cell + float3(1.0, 0.0, 1.0));
    float n011 = Hash31(cell + float3(0.0, 1.0, 1.0));
    float n111 = Hash31(cell + float3(1.0, 1.0, 1.0));

    float nx00 = lerp(n000, n100, fracPart.x);
    float nx10 = lerp(n010, n110, fracPart.x);
    float nx01 = lerp(n001, n101, fracPart.x);
    float nx11 = lerp(n011, n111, fracPart.x);
    float nxy0 = lerp(nx00, nx10, fracPart.y);
    float nxy1 = lerp(nx01, nx11, fracPart.y);
    return lerp(nxy0, nxy1, fracPart.z);
}

float FBM(float3 p) {
    float sum = 0.0;
    float amp = 0.55;
    sum += Noise3(p) * amp;
    p *= 2.03;
    amp *= 0.5;
    sum += Noise3(p) * amp;
    p *= 2.01;
    amp *= 0.5;
    sum += Noise3(p) * amp;
    return sum;
}

float HenyeyGreenstein(float cosineTheta, float g) {
    float g2 = g * g;
    float denom = pow(max(1.0 + g2 - 2.0 * g * cosineTheta, 1e-3), 1.5);
    return (1.0 - g2) / (4.0 * 3.14159265359 * denom);
}

float3 AcesApprox(float3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float4 main(in PSInput i) : SV_Target {
    if (g_UseProceduralSky > 0.5) {
        float3 dir = normalize(i.TexCoord);
        float horizonFactor = saturate(dir.y * 0.5 + 0.5);
        float cloudiness = saturate(g_CloudCoverage);
        float clearSky = 1.0 - cloudiness;
        float overcast = saturate((cloudiness - 0.35) / 0.55);
        float zenithCurve = pow(horizonFactor, lerp(0.62, 0.48, clearSky));
        float3 gradient = lerp(g_SkyHorizonColor, g_SkyZenithColor, zenithCurve);
        float horizonGlow = pow(1.0 - saturate(abs(dir.y - 0.05)), 6.0);
        float horizonHaze = pow(1.0 - saturate(abs(dir.y) * 1.6), 3.2);
        float sunAltitude = saturate(g_SunDirection.y * 0.5 + 0.5);
        float sunsetWeight = 1.0 - sunAltitude;
        float3 aerialHazeColor = lerp(float3(0.82, 0.90, 1.00), float3(1.00, 0.72, 0.46), sunsetWeight);
        float3 clearZenithBoost = float3(0.06, 0.10, 0.18) * clearSky;
        float3 clearHorizonBoost = float3(0.04, 0.05, 0.06) * clearSky;
        float3 overcastTint = float3(0.90, 0.93, 0.98);
        gradient = lerp(gradient + clearHorizonBoost, gradient * overcastTint, overcast * 0.28);
        gradient += clearZenithBoost * pow(horizonFactor, 1.3);
        gradient = lerp(gradient, aerialHazeColor, horizonHaze * lerp(0.10, 0.22, overcast));
        gradient += g_SunColor * horizonGlow * (0.09 + sunsetWeight * 0.08) * (0.65 + clearSky * 0.45);

        float sunAmount = saturate(dot(dir, normalize(g_SunDirection)));
        float forwardScatter = HenyeyGreenstein(sunAmount, 0.72) * (0.65 + sunAltitude * 0.35);
        float backScatter = HenyeyGreenstein(sunAmount, -0.15) * 0.35;
        float3 rayleighColor = float3(0.34, 0.55, 1.00);
        float3 mieColor = lerp(float3(1.00, 0.92, 0.82), float3(1.00, 0.68, 0.48), sunsetWeight);
        float aerialPerspective = pow(1.0 - horizonFactor, 1.7);
        gradient += rayleighColor * backScatter * (0.08 + clearSky * 0.10 + sunAltitude * 0.04);
        gradient += mieColor * forwardScatter * (0.42 + clearSky * 0.52 + sunsetWeight * 0.65) * (0.22 + aerialPerspective * 0.68);

        float sunDisc = smoothstep(g_SunDiscSize, 1.0, sunAmount);
        float sunGlow = pow(sunAmount, lerp(88.0, 128.0, clearSky));
        float sunHalo = pow(sunAmount, lerp(14.0, 22.0, clearSky));
        float sunBloom = pow(sunAmount, 8.0) * (0.08 + sunsetWeight * 0.14 + clearSky * 0.10);

        float moonAmount = saturate(dot(dir, normalize(g_MoonDirection)));
        float moonDisc = smoothstep(0.99935, 1.0, moonAmount);
        float moonGlow = pow(moonAmount, 64.0) * g_MoonIntensity * (0.55 + clearSky * 0.45);
        float moonHalo = pow(moonAmount, 18.0) * g_MoonIntensity * (0.45 + clearSky * 0.40);

        float3 starCell = floor(dir * 240.0);
        float starNoise = Hash31(starCell);
        float starMask = step(0.9975, starNoise);
        float starTwinkle = 0.65 + 0.35 * Hash31(starCell + 13.37);
        float stars = starMask * starTwinkle * g_StarIntensity * (0.35 + clearSky * 0.65);

        float2 highWind = float2(0.0035, -0.0018) * g_TimeSeconds;
        float2 lowWind = float2(0.0065, 0.0022) * g_TimeSeconds;
        float2 streakWind = float2(0.0110, -0.0012) * g_TimeSeconds;
        float highLayer = FBM(float3(dir.x * 1.6 + highWind.x + 3.0, dir.z * 1.6 + highWind.y - 6.0, dir.y * 0.9 + 8.0));
        float upperLayer = FBM(float3(dir.x * 2.3 + lowWind.x, dir.z * 2.3 + lowWind.y, dir.y * 1.4 + 4.0));
        float midLayer = FBM(float3(dir.x * 4.2 + lowWind.x * 1.7 + 11.0, dir.z * 4.2 + lowWind.y * 1.7 - 5.0, dir.y * 2.2));
        float streakLayer = Noise3(float3(dir.x * 10.0 + streakWind.x, dir.z * 3.0 + streakWind.y, dir.y * 0.8 + 19.0));
        float lowLayerShape = upperLayer * 0.50 + midLayer * 0.35 + streakLayer * 0.15;
        float highLayerShape = highLayer * 0.78 + Noise3(float3(dir.x * 5.8 + highWind.x * 1.2 - 4.0, dir.z * 5.8 + highWind.y * 1.2 + 9.0, dir.y * 0.6 + 23.0)) * 0.22;
        float sunCloudProbe = FBM(float3(
            (dir.x + g_SunDirection.x * 0.08) * 5.8 + 7.0,
            (dir.z + g_SunDirection.z * 0.08) * 5.8 - 9.0,
            dir.y * 1.4 + g_SunDirection.y * 0.6));

        float lowCloudHeightMask = smoothstep(-0.12, 0.28, dir.y) * (1.0 - smoothstep(0.58, 0.92, dir.y));
        float highCloudHeightMask = smoothstep(0.10, 0.42, dir.y) * (1.0 - smoothstep(0.76, 0.98, dir.y));
        float cloudThreshold = lerp(0.74, 0.38, cloudiness);
        float lowCloudMask = smoothstep(cloudThreshold, cloudThreshold + 0.16, lowLayerShape) * lowCloudHeightMask;
        float lowCloudSoftShadow = smoothstep(cloudThreshold - 0.10, cloudThreshold + 0.02, lowLayerShape) * lowCloudHeightMask;
        float lowCloudCore = smoothstep(cloudThreshold + 0.02, cloudThreshold + 0.22, lowLayerShape) * lowCloudHeightMask;
        float lowCloudRim = saturate(lowCloudMask - lowCloudCore) * (0.65 + 0.35 * horizonHaze);
        float lowCloudDepth = saturate((lowLayerShape - sunCloudProbe) * 1.8 + 0.5) * lowCloudMask;
        float highCoverage = saturate(cloudiness * 0.78 + 0.10);
        float highThreshold = lerp(0.70, 0.46, highCoverage);
        float highCloudMask = smoothstep(highThreshold, highThreshold + 0.16, highLayerShape) * highCloudHeightMask * 0.72;
        float highCloudRim = saturate(highCloudMask - smoothstep(highThreshold + 0.03, highThreshold + 0.20, highLayerShape) * highCloudHeightMask) * 0.75;
        float cloudMask = saturate(lowCloudMask + highCloudMask * (1.0 - lowCloudMask * 0.35));
        float cloudSoftShadow = saturate(lowCloudSoftShadow + highCloudMask * 0.18);

        float sunForward = saturate(dot(normalize(g_SunDirection), dir) * 0.5 + 0.5);
        float warmScatter = pow(sunForward, 3.0);
        float coolScatter = pow(1.0 - sunForward, 2.0);
        float3 cloudLitColor = lerp(float3(0.94, 0.96, 0.99), float3(1.00, 0.76, 0.58), warmScatter);
        float3 cloudShadowColor = lerp(g_SkyZenithColor, g_SkyHorizonColor, 0.32) * lerp(0.82, 0.72, sunsetWeight);
        float3 lowCloudColor = lerp(cloudShadowColor, cloudLitColor, saturate(0.30 + warmScatter * 0.95 - coolScatter * 0.15));
        lowCloudColor = lerp(lowCloudColor, float3(1.00, 0.70, 0.52), horizonHaze * sunsetWeight * 0.22);
        float3 highCloudColor = lerp(float3(0.86, 0.90, 0.95), float3(1.00, 0.82, 0.70), warmScatter * 0.7 + sunsetWeight * 0.2);
        float3 cloudRimColor = lerp(float3(1.00, 0.98, 0.94), float3(1.00, 0.74, 0.58), sunsetWeight);
        lowCloudColor = lerp(lowCloudColor * 0.82, lowCloudColor, lowCloudDepth);
        float3 cloudColor = lerp(lowCloudColor, highCloudColor, saturate(highCloudMask * 0.8));
        cloudColor = lerp(cloudColor, float3(0.85, 0.88, 0.93), overcast * 0.38);

        float3 color = gradient * g_SkyIntensity;
        color += g_SunColor * (sunDisc * (5.8 + clearSky * 4.2) + sunGlow * (0.24 + clearSky * 0.34) + sunHalo * (0.08 + clearSky * 0.10) + sunBloom * 0.18) * (1.0 - overcast * 0.70);
        color += g_MoonColor * (moonDisc * 2.5 + moonGlow * 0.20 + moonHalo * 0.08);
        color += float3(stars, stars, stars);
        color = lerp(color, color * 0.97, cloudMask * 0.08);
        color = lerp(color, cloudColor, cloudMask * lerp(0.84, 1.0, overcast));
        color = lerp(color, color * 0.94, cloudSoftShadow * 0.18);
        color += cloudRimColor * lowCloudRim * (0.05 + warmScatter * 0.12 + sunsetWeight * 0.08);
        color += cloudRimColor * highCloudRim * (0.03 + warmScatter * 0.06 + sunsetWeight * 0.05);
        color = lerp(color, aerialHazeColor, horizonHaze * lerp(0.03, 0.12, sunsetWeight + overcast * 0.35));
        float exposure = lerp(1.16, 0.94, sunAltitude) + clearSky * 0.05;
        color = AcesApprox(color * exposure);
        return float4(color, 1.0);
    }

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
    if (!pSkyboxVS || !pSkyboxPS) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create skybox shaders");
        return;
    }
    
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
    if (!m_pSkyboxPSO) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create skybox PSO");
        return;
    }
    
    // 注意：SRB 将在加载环境贴图后创建，因为需要绑定 cubemap 纹理
    
    if (auto* vsConstants = m_pSkyboxPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "SkyboxConstants")) {
        vsConstants->Set(m_pSkyboxVSConstants, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
    } else {
        MOON_LOG_ERROR("DiligentRenderer", "Skybox VS constant buffer binding not found");
    }
    if (auto* psConstants = m_pSkyboxPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "SkyboxConstants")) {
        psConstants->Set(m_pSkyboxVSConstants, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
    } else {
        MOON_LOG_ERROR("DiligentRenderer", "Skybox PS constant buffer binding not found");
    }
    if (auto* equirectMap = m_pSkyboxPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_EquirectMap")) {
        equirectMap->Set(m_pDefaultWhiteTextureSRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
    } else {
        MOON_LOG_ERROR("DiligentRenderer", "Skybox environment texture binding not found");
    }
    m_pSkyboxPSO->CreateShaderResourceBinding(&m_pSkyboxSRB, true);

    MOON_LOG_INFO("DiligentRenderer", "Skybox pass created successfully!");
}

void DiligentRenderer::CreatePrecipitationVolumePass()
{
    RefCntAutoPtr<IShader> vs = CreateShaderFromFile(
        "PrecipitationVolume.vs.hlsl",
        SHADER_TYPE_VERTEX,
        "Precipitation Volume VS");
    RefCntAutoPtr<IShader> ps = CreateShaderFromFile(
        "PrecipitationVolume.ps.hlsl",
        SHADER_TYPE_PIXEL,
        "Precipitation Volume PS");

    if (!vs || !ps) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create precipitation volume shaders");
        return;
    }

    LayoutElement layout[4];
    Uint32 numElements = 0;
    DiligentRendererUtils::GetVertexLayout(layout, numElements);

    GraphicsPipelineStateCreateInfo pci{};
    pci.PSODesc.Name = "Precipitation Volume PSO";
    pci.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
    pci.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    pci.GraphicsPipeline.NumRenderTargets = 1;
    pci.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    pci.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    pci.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pci.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    pci.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;
    pci.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    pci.GraphicsPipeline.InputLayout.LayoutElements = layout;
    pci.GraphicsPipeline.InputLayout.NumElements = numElements;
    auto& blend = pci.GraphicsPipeline.BlendDesc.RenderTargets[0];
    blend.BlendEnable = True;
    blend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    blend.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    blend.BlendOp = BLEND_OPERATION_ADD;
    blend.SrcBlendAlpha = BLEND_FACTOR_ONE;
    blend.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
    blend.BlendOpAlpha = BLEND_OPERATION_ADD;
    pci.pVS = vs;
    pci.pPS = ps;

    m_pPrecipitationVolumePSO.Release();
    m_pDevice->CreateGraphicsPipelineState(pci, &m_pPrecipitationVolumePSO);
    if (!m_pPrecipitationVolumePSO) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create precipitation volume PSO");
        return;
    }

    BindStaticBuffer(
        m_pPrecipitationVolumePSO,
        SHADER_TYPE_VERTEX,
        "Constants",
        m_pVSConstants,
        "Precipitation Volume PSO");
    BindStaticBuffer(
        m_pPrecipitationVolumePSO,
        SHADER_TYPE_PIXEL,
        "SceneConstants",
        m_pPSSceneConstants,
        "Precipitation Volume PSO");

    m_pPrecipitationVolumeSRB.Release();
    m_pPrecipitationVolumePSO->CreateShaderResourceBinding(&m_pPrecipitationVolumeSRB, true);
    if (!m_pPrecipitationVolumeSRB) {
        MOON_LOG_ERROR("DiligentRenderer", "Failed to create precipitation volume SRB");
        return;
    }

    MOON_LOG_INFO("DiligentRenderer", "Precipitation volume pass created");
}

void DiligentRenderer::CreatePrecipitationVolumeBuffers()
{
    BufferDesc vbDesc{};
    vbDesc.Name = "Precipitation VB";
    vbDesc.BindFlags = BIND_VERTEX_BUFFER;
    vbDesc.Usage = USAGE_DYNAMIC;
    vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    vbDesc.Size = static_cast<Uint32>(kPrecipitationMaxParticles * 4 * sizeof(Moon::Vertex));
    m_pDevice->CreateBuffer(vbDesc, nullptr, &m_pPrecipitationVB);

    BufferDesc ibDesc{};
    ibDesc.Name = "Precipitation IB";
    ibDesc.BindFlags = BIND_INDEX_BUFFER;
    ibDesc.Usage = USAGE_DYNAMIC;
    ibDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    ibDesc.Size = static_cast<Uint32>(kPrecipitationMaxParticles * 6 * sizeof(uint32_t));
    m_pDevice->CreateBuffer(ibDesc, nullptr, &m_pPrecipitationIB);
}

void DiligentRenderer::RenderSkybox()
{
    if (!m_pSkyboxPSO || !m_pSkyboxSRB) return;
    if (!m_RenderProceduralSky && m_SceneDataCache.hasEnvironmentMap <= 0.5f) return;
    
    // Keep the sky centered on the camera so the user can never "reach" the cube.
    SkyboxConstantsCPU constants{};
    Moon::Matrix4x4 skyViewProj = m_ViewProj;
    skyViewProj.m[3][0] = 0.0f;
    skyViewProj.m[3][1] = 0.0f;
    skyViewProj.m[3][2] = 0.0f;
    constants.ViewProjT = DiligentRendererUtils::Transpose(skyViewProj);
    constants.UseProceduralSky = (m_RenderProceduralSky && m_SceneDataCache.hasEnvironmentMap <= 0.5f) ? 1.0f : 0.0f;
    constants.SkyHorizonColor = m_SceneDataCache.fogColor;
    constants.SkyZenithColor = m_SceneDataCache.skyColor;
    constants.SkyIntensity = 1.0f;
    constants.SunDirection = (m_SceneDataCache.lightDirection * -1.0f).Normalized();
    constants.SunColor = m_SceneDataCache.lightColor;
    constants.SunDiscSize = 0.9992f;
    constants.StarIntensity = constants.SunDirection.y > 0.0f ? 0.0f : std::min(1.0f, -constants.SunDirection.y * 1.5f);
    constants.MoonDirection = constants.SunDirection * -1.0f;
    constants.MoonIntensity = constants.SunDirection.y > 0.0f ? 0.0f : std::min(1.0f, -constants.SunDirection.y * 1.2f);
    constants.MoonColor = Moon::Vector3(0.70f, 0.78f, 0.92f);
    constants.CloudCoverage = std::min(1.0f, std::max(0.0f, m_SceneDataCache.cloudCoverage));
    constants.TimeSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now() - m_SkyStartTime).count();
    
    // 更新常量缓冲
    UpdateCB(m_pSkyboxVSConstants, constants);
    
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

void DiligentRenderer::RenderPrecipitationVolume()
{
    if (!m_pPrecipitationVolumePSO || !m_pPrecipitationVolumeSRB || !m_pPrecipitationVB || !m_pPrecipitationIB) {
        return;
    }
    if (m_SceneDataCache.precipitationIntensity <= 0.02f) {
        return;
    }

    const bool snow = m_SceneDataCache.snowAmount >= 0.5f;
    const uint32_t particleCount = snow ? 520u : 900u;
    const float intensity = std::max(0.35f, m_SceneDataCache.precipitationIntensity);
    const float timeSeconds = m_SceneDataCache.timeSeconds;
    const Moon::Vector3 cameraPos = m_SceneDataCache.cameraPosition;
    const Moon::Vector3 right = m_CameraRight.Normalized();
    const Moon::Vector3 up = m_CameraUp.Normalized();
    const Moon::Vector3 tint = PrecipitationTint(snow);

    std::vector<Moon::Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(particleCount * 4);
    indices.reserve(particleCount * 6);

    const int cellsPerAxis = snow ? 18 : 24;
    const float cellSize = snow ? 6.0f : 4.5f;
    const float speed = snow ? (3.5f + intensity * 2.0f) : (24.0f + intensity * 8.0f);
    const float topHeight = snow ? 26.0f : 34.0f;
    const float bottomHeight = snow ? -10.0f : -6.0f;
    const float fallHeight = topHeight - bottomHeight;
    const Moon::Vector3 streakDir = snow
        ? Moon::Vector3(0.18f, -1.0f, 0.10f).Normalized()
        : Moon::Vector3(0.22f + m_SceneDataCache.windStrength * 0.22f, -1.0f, 0.08f).Normalized();

    uint32_t particleIndex = 0;
    const int baseCellX = static_cast<int>(std::floor(cameraPos.x / cellSize)) - cellsPerAxis / 2;
    const int baseCellZ = static_cast<int>(std::floor(cameraPos.z / cellSize)) - cellsPerAxis / 2;

    for (int z = 0; z < cellsPerAxis && particleIndex < particleCount; ++z) {
        for (int x = 0; x < cellsPerAxis && particleIndex < particleCount; ++x) {
            const uint32_t seed = static_cast<uint32_t>((baseCellX + x) * 73856093 ^ (baseCellZ + z) * 19349663);
            const float occupancy = Hash11(seed + 17u);
            if ((!snow && occupancy < 0.38f) || (snow && occupancy < 0.52f)) {
                continue;
            }

            const float randX = Hash11(seed + 101u);
            const float randZ = Hash11(seed + 211u);
            const float randY = Hash11(seed + 307u);
            const float randSize = Hash11(seed + 401u);
            const float randAlpha = Hash11(seed + 503u);

            Moon::Vector3 center;
            center.x = (static_cast<float>(baseCellX + x) + randX) * cellSize;
            center.z = (static_cast<float>(baseCellZ + z) + randZ) * cellSize;

            const float cycle = std::fmod(timeSeconds * speed * (0.75f + randSize * 0.6f) + randY * fallHeight, fallHeight);
            center.y = cameraPos.y + topHeight - cycle;

            const float alpha = snow ? (0.26f + randAlpha * 0.24f) : (0.16f + randAlpha * 0.12f);
            const float width = snow ? (0.20f + randSize * 0.18f) : (0.018f + randSize * 0.020f);
            const float height = snow ? (0.20f + randSize * 0.24f) : (0.75f + randSize * 1.20f);

            Moon::Vector3 axisA = right * width;
            Moon::Vector3 axisB = snow ? (up * height) : (streakDir * height);
            const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
            vertices.emplace_back(center - axisA - axisB, Moon::Vector3(0.0f, 1.0f, 0.0f), tint, alpha, Moon::Vector2(0.0f, 1.0f));
            vertices.emplace_back(center + axisA - axisB, Moon::Vector3(0.0f, 1.0f, 0.0f), tint, alpha, Moon::Vector2(1.0f, 1.0f));
            vertices.emplace_back(center + axisA + axisB, Moon::Vector3(0.0f, 1.0f, 0.0f), tint, alpha, Moon::Vector2(1.0f, 0.0f));
            vertices.emplace_back(center - axisA + axisB, Moon::Vector3(0.0f, 1.0f, 0.0f), tint, alpha, Moon::Vector2(0.0f, 0.0f));

            indices.push_back(baseVertex + 0);
            indices.push_back(baseVertex + 1);
            indices.push_back(baseVertex + 2);
            indices.push_back(baseVertex + 0);
            indices.push_back(baseVertex + 2);
            indices.push_back(baseVertex + 3);
            ++particleIndex;
        }
    }

    if (indices.empty()) {
        return;
    }

    {
        void* data = nullptr;
        m_pImmediateContext->MapBuffer(m_pPrecipitationVB, MAP_WRITE, MAP_FLAG_DISCARD, data);
        std::memcpy(data, vertices.data(), vertices.size() * sizeof(Moon::Vertex));
        m_pImmediateContext->UnmapBuffer(m_pPrecipitationVB, MAP_WRITE);
    }
    {
        void* data = nullptr;
        m_pImmediateContext->MapBuffer(m_pPrecipitationIB, MAP_WRITE, MAP_FLAG_DISCARD, data);
        std::memcpy(data, indices.data(), indices.size() * sizeof(uint32_t));
        m_pImmediateContext->UnmapBuffer(m_pPrecipitationIB, MAP_WRITE);
    }

    VSConstantsCPU cbuf{};
    cbuf.WorldViewProjT = DiligentRendererUtils::Transpose(m_ViewProj);
    cbuf.WorldT = DiligentRendererUtils::Transpose(Moon::Matrix4x4());
    UpdateCB(m_pVSConstants, cbuf);

    Uint64 offset = 0;
    IBuffer* vbs[] = { m_pPrecipitationVB };
    m_pImmediateContext->SetPipelineState(m_pPrecipitationVolumePSO);
    m_pImmediateContext->CommitShaderResources(m_pPrecipitationVolumeSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetVertexBuffers(0, 1, vbs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_pPrecipitationIB, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs drawAttrs{};
    drawAttrs.IndexType = VT_UINT32;
    drawAttrs.NumIndices = static_cast<Uint32>(indices.size());
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
        if (auto* equirectMap = m_pSkyboxPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "g_EquirectMap")) {
            equirectMap->Set(m_pEquirectHDR_SRV, SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE);
        } else {
            MOON_LOG_ERROR("DiligentRenderer", "Skybox environment texture binding not found during map load");
        }
        
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
