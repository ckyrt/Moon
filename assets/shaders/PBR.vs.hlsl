// PBR Vertex Shader
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

void main(in VSInput i, out PSInput o) {
    float3 localPos = i.Pos;
    float vegetationMask = (g_MappingMode < 0.5 && g_UseVertexColorTint > 0.5) ? saturate(i.Color.a) : 0.0;

    if (vegetationMask > 0.001) {
        float phase = g_TimeSeconds * (0.9 + g_WindStrength * 1.7) + i.Pos.x * 0.18 + i.Pos.z * 0.11;
        float sway = sin(phase) * 0.08 + cos(phase * 0.63 + i.Pos.y * 0.7) * 0.05;
        float gust = sin(g_TimeSeconds * (2.4 + g_WindStrength * 2.8) + i.Pos.x * 0.05) * 0.04;
        float bend = (sway + gust) * vegetationMask * (0.45 + g_WindStrength * 1.25);
        localPos.x += bend;
        localPos.z += bend * 0.42;
    }

    float4 worldPos4 = mul(float4(localPos, 1.0), g_World);
    o.Pos = mul(float4(localPos, 1.0), g_WorldViewProj);
    o.WorldPos = worldPos4.xyz;
    o.NormalWS = normalize(mul((float3x3)g_World, i.Normal));
    o.Color = i.Color;
    o.UV = i.UV;
}
