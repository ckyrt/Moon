// ============================================================================
// PBR Common Definitions
// 常量、cbuffer、纹理声明、输入结构
// ============================================================================

#ifndef PBR_COMMON_HLSL
#define PBR_COMMON_HLSL

static const float PI = 3.14159265359;

// 材质参数常量缓冲区
cbuffer MaterialConstants { 
    float g_Metallic;
    float g_Roughness;
    float g_TriplanarTiling;  // Triplanar纹理平铺密度（默认0.5 = 每2米重复一次）
    float g_MappingMode;      // 映射模式（0.0 = UV, 1.0 = Triplanar）
    float3 g_BaseColor;
    float g_TriplanarBlend;   // Triplanar混合锐度（默认4.0）
    float g_HasNormalMap;     // 是否加载了法线贴图（0.0 = 无，1.0 = 有）
    float g_Opacity;          // 不透明度（0.0 = 完全透明, 1.0 = 完全不透明）
    float2 g_Padding2;
    float3 g_TransmissionColor; // 透射颜色（用于玻璃）
    float g_Padding3;
};

// 场景参数常量缓冲区（相机位置、光源等）
cbuffer SceneConstants { 
    float3 g_CameraPosition;
    float g_HasEnvironmentMap;  // 是否有有效的环境贴图（0.0 = 无，1.0 = 有）
    float3 g_LightDirection;
    float g_Padding4;
    float3 g_LightColor;
    float g_LightIntensity;
};

// 阴影（Shadow Map）参数
cbuffer ShadowConstants {
    float4x4 g_WorldToShadowClip;      // world -> light clip space
    float2   g_ShadowMapTexelSize;     // 1.0 / shadowMapSize
    float    g_ShadowBias;             // depth bias
    float    g_ShadowStrength;         // 0=disable, 1=full
};

// 纹理资源和采样器
Texture2D g_AlbedoMap;
SamplerState g_AlbedoMap_sampler;

Texture2D g_AOMap;
SamplerState g_AOMap_sampler;

Texture2D g_RoughnessMap;
SamplerState g_RoughnessMap_sampler;

Texture2D g_MetalnessMap;
SamplerState g_MetalnessMap_sampler;

Texture2D g_NormalMap;
SamplerState g_NormalMap_sampler;

Texture2D g_EquirectMap;
SamplerState g_EquirectMap_sampler;

// BRDF LUT for IBL (256x256 RG16F)
Texture2D g_BRDF_LUT;
SamplerState g_BRDF_LUT_sampler;

// Shadow map (depth)
Texture2D              g_ShadowMap;
SamplerComparisonState g_ShadowMap_sampler;

struct PSInput { 
    float4 Pos      : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 NormalWS : NORMAL;
    float4 Color    : COLOR;
    float2 UV       : TEXCOORD;  // 保留用于非CSG物体
};

// 将 3D 方向转换为 equirectangular UV 坐标
float2 DirToEquirectUV(float3 dir) {
    float u = atan2(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float v = 1.0 - (asin(dir.y) / PI + 0.5);
    return float2(u, v);
}

#endif // PBR_COMMON_HLSL
