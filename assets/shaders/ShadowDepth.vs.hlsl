// Shadow map depth-only vertex shader

cbuffer ShadowVSConstants
{
    float4x4 g_WorldViewProj;
};

struct VSInput
{
    float3 Pos    : ATTRIB0;
    float3 Normal : ATTRIB1;
    float4 Color  : ATTRIB2;
    float2 UV     : ATTRIB3;
};

struct VSOutput
{
    float4 Pos : SV_POSITION;
};

void main(in VSInput i, out VSOutput o)
{
    o.Pos = mul(float4(i.Pos, 1.0), g_WorldViewProj);
}
