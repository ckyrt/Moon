// ============================================================================
// PBR Shadow Mapping
// Minimal directional shadow map sampling (3x3 PCF)
// ============================================================================

#ifndef PBR_SHADOW_HLSL
#define PBR_SHADOW_HLSL

float ComputeShadowFactor(float3 worldPos, float3 N, float3 L)
{
    // Disabled or no need
    if (g_ShadowStrength <= 0.001)
        return 1.0;

    // Transform to light clip space
    float4 shadowClip = mul(float4(worldPos, 1.0), g_WorldToShadowClip);

    // Avoid division by zero
    if (abs(shadowClip.w) < 1e-6)
        return 1.0;

    float3 ndc = shadowClip.xyz / shadowClip.w;

    // D3D NDC: x,y in [-1,1], z in [0,1]
    // Convert to texture UV with Y flipped (NDC +1 is top, UV 0 is top)
    float2 uv;
    uv.x = ndc.x * 0.5 + 0.5;
    uv.y = -ndc.y * 0.5 + 0.5;

    float depth = ndc.z;

    // Outside of shadow map
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;

    // Outside of valid depth
    if (depth < 0.0 || depth > 1.0)
        return 1.0;

    // Slope-scaled bias (very lightweight)
    float ndotl = saturate(dot(N, L));
    float bias = max(g_ShadowBias * (1.0 - ndotl), g_ShadowBias * 0.5);
    float cmpDepth = max(depth - bias, 1e-7);

    // 3x3 PCF
    float shadow = 0.0;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2(x, y) * g_ShadowMapTexelSize;
            shadow += g_ShadowMap.SampleCmpLevelZero(g_ShadowMap_sampler, uv + offset, cmpDepth);
        }
    }
    shadow *= (1.0 / 9.0);

    // Strength blending
    return lerp(1.0, shadow, saturate(g_ShadowStrength));
}

float ComputePointShadowFactor(float3 worldPos, float3 N, float3 L)
{
    if (g_PointShadowStrength <= 0.001)
        return 1.0;

    float3 toLight = g_PointShadowLightPos - worldPos; // fragment -> light
    float dist = length(toLight);
    if (dist <= 1e-4)
        return 1.0;

    float normDist = dist * g_PointShadowInvRange;
    if (normDist <= 0.0 || normDist >= 1.0)
        return 1.0;

    float3 dir = -toLight / dist; // light -> fragment (cubemap direction)
    float closest = g_PointShadowMap.Sample(g_PointShadowMap_sampler, dir).r;

    float ndotl = saturate(dot(N, L));
    float bias = max(g_PointShadowBias * (1.0 - ndotl), g_PointShadowBias * 0.5);

    float shadow = (normDist - bias <= closest + 1e-5) ? 1.0 : 0.0;
    return lerp(1.0, shadow, saturate(g_PointShadowStrength));
}

#endif // PBR_SHADOW_HLSL
