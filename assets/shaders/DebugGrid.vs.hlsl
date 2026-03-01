// Debug grid line vertex shader
cbuffer Constants {
    float4x4 g_ViewProj;
};

struct VSInput {
    float3 Pos   : ATTRIB0;
    float4 Color : ATTRIB1;
};

struct PSInput {
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR;
};

void main(in VSInput i, out PSInput o)
{
    o.Pos = mul(float4(i.Pos, 1.0), g_ViewProj);
    o.Color = i.Color;
}
