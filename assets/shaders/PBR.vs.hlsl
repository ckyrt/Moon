// PBR Vertex Shader
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
    o.Pos = mul(float4(i.Pos, 1.0), g_WorldViewProj);
    o.WorldPos = mul(float4(i.Pos, 1.0), g_World).xyz;
    // 修复：法线变换应该使用列向量形式（Matrix * Normal）
    // 对于统一缩放的情况，这是正确的（非统一缩放需要逆转置矩阵）
    o.NormalWS = normalize(mul((float3x3)g_World, i.Normal));
    o.Color = i.Color;
    o.UV = i.UV;
}
