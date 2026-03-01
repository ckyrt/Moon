// Point light shadow cubemap - vertex shader
// Renders geometry from point light view (per cubemap face)

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

struct VSOutput {
    float4 Pos      : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
};

void main(in VSInput i, out VSOutput o)
{
    float3 worldPos = mul(float4(i.Pos, 1.0), g_World).xyz;
    o.WorldPos = worldPos;
    o.Pos = mul(float4(i.Pos, 1.0), g_WorldViewProj);
}
