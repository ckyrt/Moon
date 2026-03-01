// Point light shadow cubemap - pixel shader
// Outputs normalized radial distance to the point light into R32_FLOAT cubemap face

cbuffer PointShadowConstants {
    float3 g_PointShadowLightPos;
    float  g_PointShadowInvRange;
    float  g_PointShadowBias;
    float  g_PointShadowStrength;
    float2 g_PointShadowPadding;
};

struct VSOutput {
    float4 Pos      : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
};

float main(in VSOutput i) : SV_Target
{
    float dist = length(i.WorldPos - g_PointShadowLightPos);
    float d = dist * g_PointShadowInvRange;
    return saturate(d);
}
