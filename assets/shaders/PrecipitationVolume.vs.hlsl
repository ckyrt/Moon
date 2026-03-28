#include "include/SurfaceShared.hlsl"

cbuffer Constants
{
    float4x4 g_WorldViewProj;
    float4x4 g_World;
};

struct VSInput
{
    float3 Pos   : ATTRIB0;
    float3 Normal: ATTRIB1;
    float4 Color : ATTRIB2;
    float2 UV    : ATTRIB3;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV    : TEXCOORD0;
};

void main(in VSInput i, out PSInput o)
{
    o.Pos = mul(float4(i.Pos, 1.0), g_WorldViewProj);
    o.Color = i.Color;
    o.UV = i.UV;
}
