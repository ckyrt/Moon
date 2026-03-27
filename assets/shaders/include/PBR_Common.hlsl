// ============================================================================
// PBR Common Definitions
// Shared constant buffers, texture declarations, and common pixel inputs.
// ============================================================================

#ifndef PBR_COMMON_HLSL
#define PBR_COMMON_HLSL

#include "SurfaceShared.hlsl"

static const float PI = 3.14159265359;

cbuffer ShadowConstants {
    float4x4 g_WorldToShadowClip;
    float2 g_ShadowMapTexelSize;
    float g_ShadowBias;
    float g_ShadowStrength;
};

cbuffer PointShadowConstants {
    float3 g_PointShadowLightPos;
    float g_PointShadowInvRange;
    float g_PointShadowBias;
    float g_PointShadowStrength;
    float2 g_PointShadowPadding;
};

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

Texture2D g_BRDF_LUT;
SamplerState g_BRDF_LUT_sampler;

Texture2D g_ShadowMap;
SamplerComparisonState g_ShadowMap_sampler;

TextureCube g_PointShadowMap;
SamplerState g_PointShadowMap_sampler;

struct PSInput {
    float4 Pos : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 NormalWS : NORMAL;
    float4 Color : COLOR;
    float2 UV : TEXCOORD;
};

float2 DirToEquirectUV(float3 dir) {
    float u = atan2(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float v = 1.0 - (asin(dir.y) / PI + 0.5);
    return float2(u, v);
}

#endif // PBR_COMMON_HLSL
