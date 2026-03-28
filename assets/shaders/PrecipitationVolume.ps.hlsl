#include "include/SurfaceShared.hlsl"

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR0;
    float2 UV    : TEXCOORD0;
};

float RainMask(float2 uv)
{
    float2 p = uv - float2(0.5, 0.5);
    float width = 1.0 - smoothstep(0.05, 0.18, abs(p.x));
    float taper = 1.0 - smoothstep(0.35, 0.5, abs(p.y));
    return width * taper;
}

float SnowMask(float2 uv)
{
    float2 p = uv - float2(0.5, 0.5);
    float dist = length(p);
    float body = 1.0 - smoothstep(0.12, 0.48, dist);
    float crystal = 1.0 - smoothstep(0.02, 0.10, min(abs(p.x), abs(p.y)));
    return saturate(body * 0.75 + crystal * 0.25);
}

float4 main(in PSInput i) : SV_Target
{
    float snow = saturate(g_SnowAmount);
    float mask = lerp(RainMask(i.UV), SnowMask(i.UV), snow);
    float alpha = mask * i.Color.a * saturate(g_PrecipitationIntensity);
    if (alpha <= 0.002) {
        discard;
    }

    float3 color = i.Color.rgb;
    return float4(color, saturate(alpha));
}
