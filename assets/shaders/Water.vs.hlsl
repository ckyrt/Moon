// Water Vertex Shader
#include "include/SurfaceShared.hlsl"

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

float ComputeWaveOffset(float2 worldXZ, float timeSeconds, float windStrength)
{
    float waveA = sin(dot(worldXZ, normalize(float2(0.92, 0.38))) * 0.022 + timeSeconds * (0.70 + windStrength * 0.90));
    float waveB = cos(dot(worldXZ, normalize(float2(-0.44, 0.90))) * 0.031 - timeSeconds * (0.55 + windStrength * 0.75) + 1.4);
    float waveC = sin(dot(worldXZ, normalize(float2(0.18, -0.98))) * 0.047 + timeSeconds * (1.05 + windStrength * 1.20) + 2.1);
    return (waveA * 0.45 + waveB * 0.35 + waveC * 0.20) * (0.05 + windStrength * 0.06);
}

void main(in VSInput i, out PSInput o)
{
    float3 localPos = i.Pos;
    localPos.y += ComputeWaveOffset(i.Pos.xz, g_TimeSeconds, g_WindStrength);

    float4 worldPos4 = mul(float4(localPos, 1.0), g_World);
    o.Pos = mul(float4(localPos, 1.0), g_WorldViewProj);
    o.WorldPos = worldPos4.xyz;
    o.NormalWS = normalize(mul((float3x3)g_World, i.Normal));
    o.Color = i.Color;
    o.UV = i.UV;
}
