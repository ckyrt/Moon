// UI Pixel Shader - 渲染 CEF UI 纹理
Texture2D g_UITexture;
SamplerState g_Sampler;

struct PSInput {
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET {
    float4 color = g_UITexture.Sample(g_Sampler, input.UV);
    
    // Alpha blending - 透明区域显示 3D 场景
    // CEF 已经预乘了 alpha，所以直接返回即可
    return color;
}
