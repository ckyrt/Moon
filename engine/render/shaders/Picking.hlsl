// Picking.hlsl - 物体拾取着色器
// 输出：每像素的 ObjectID（R32_UINT）

cbuffer VSConstants
{
    float4x4 g_WorldViewProj;
};

cbuffer PSConstants
{
    uint g_ObjectID;  // 当前物体的唯一 ID
};

struct VSInput
{
    float3 Position : ATTRIB0;
};

struct PSInput
{
    float4 Position : SV_Position;
};

// Vertex Shader
PSInput main_vs(VSInput input)
{
    PSInput output;
    output.Position = mul(float4(input.Position, 1.0), g_WorldViewProj);
    return output;
}

// Pixel Shader - 直接输出 ObjectID
uint main_ps(PSInput input) : SV_Target
{
    return g_ObjectID;
}
