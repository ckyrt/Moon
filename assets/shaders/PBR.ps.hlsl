// PBR Pixel Shader
#include "include/PBR_Common.hlsl"
#include "include/PBR_Triplanar.hlsl"
#include "include/PBR_Lighting.hlsl"
#include "include/PBR_Shadow.hlsl"

float HashNoise2D(float2 p)
{
    float h = dot(p, float2(127.1, 311.7));
    return frac(sin(h) * 43758.5453123);
}

float3 SampleTerrainAlbedo(Texture2D tex, SamplerState samp, float3 worldPos, float3 normalWS, float tiling)
{
    float coarseTiling = tiling * 0.38;
    float detailTiling = tiling * 1.35;
    float noise = HashNoise2D(worldPos.xz * 0.0017);
    float blend = saturate(0.30 + noise * 0.55);
    float3 coarse = SampleTriplanar(tex, samp, worldPos, normalWS, coarseTiling).rgb;
    float3 detail = SampleTriplanar(tex, samp, worldPos + float3(17.0, 0.0, 11.0), normalWS, detailTiling).rgb;
    return lerp(coarse, detail, blend);
}

float3 SampleUVNormal(float3 normalWS, float3 worldPos, float2 uv)
{
    float3 normalMap = g_NormalMap.Sample(g_NormalMap_sampler, uv).rgb;
    if (dot(normalMap, normalMap) <= 0.001) {
        return normalWS;
    }

    normalMap = normalMap * 2.0 - 1.0;
    normalMap.y = -normalMap.y;
    normalMap.z = sqrt(saturate(1.0 - dot(normalMap.xy, normalMap.xy)));

    float3 dPosDx = ddx_coarse(worldPos);
    float3 dPosDy = ddy_coarse(worldPos);
    float2 dUVDx = ddx_coarse(uv);
    float2 dUVDy = ddy_coarse(uv);

    float3 T = dPosDx * dUVDy.y - dPosDy * dUVDx.y;
    float3 B = dPosDy * dUVDx.x - dPosDx * dUVDy.x;

    float tDot = dot(T, T);
    float bDot = dot(B, B);
    if (tDot <= 1e-6 || bDot <= 1e-6) {
        return normalWS;
    }

    float invMax = rsqrt(max(tDot, bDot));
    float3x3 TBN = float3x3(T * invMax, B * invMax, normalWS);
    return normalize(mul(normalMap, TBN));
}

float4 main(in PSInput i) : SV_Target
{
    float3 N = normalize(i.NormalWS);
    float4 albedoSample;
    float3 albedo;
    float ao;
    float roughness;
    float metallic;

    if (g_MappingMode > 0.5) {
        albedoSample = float4(SampleTerrainAlbedo(g_AlbedoMap, g_AlbedoMap_sampler, i.WorldPos, N, g_TriplanarTiling), 1.0);
        albedo = albedoSample.rgb;
        ao = SampleTriplanar(g_AOMap, g_AOMap_sampler, i.WorldPos, N, g_TriplanarTiling).r;
        roughness = max(SampleTriplanar(g_RoughnessMap, g_RoughnessMap_sampler, i.WorldPos, N, g_TriplanarTiling).r * g_Roughness, 0.04);
        metallic = saturate(SampleTriplanar(g_MetalnessMap, g_MetalnessMap_sampler, i.WorldPos, N, g_TriplanarTiling).r * g_Metallic);

        if (g_HasNormalMap > 0.5) {
            float3 triplanarNormal = SampleTriplanarNormal(g_NormalMap, g_NormalMap_sampler, i.WorldPos, N, g_TriplanarTiling);
            N = normalize(lerp(N, triplanarNormal, 0.5));
        }
    } else {
        albedoSample = g_AlbedoMap.Sample(g_AlbedoMap_sampler, i.UV);
        albedo = albedoSample.rgb;
        ao = g_AOMap.Sample(g_AOMap_sampler, i.UV).r;
        roughness = max(g_RoughnessMap.Sample(g_RoughnessMap_sampler, i.UV).r * g_Roughness, 0.04);
        metallic = saturate(g_MetalnessMap.Sample(g_MetalnessMap_sampler, i.UV).r * g_Metallic);

        if (g_HasNormalMap > 0.5) {
            N = SampleUVNormal(N, i.WorldPos, i.UV);
        }
    }

    if (g_MappingMode < 0.5 && g_AlphaCutoff > 0.001) {
        clip(albedoSample.a - g_AlphaCutoff);
    }

    if (dot(albedo, albedo) > 0.001) {
        albedo *= g_BaseColor;
    } else {
        albedo = g_BaseColor;
    }

    if (g_UseVertexColorTint > 0.5) {
        if (g_MappingMode > 0.5) {
            float3 terrainTint = saturate(i.Color.rgb * 1.18);
            albedo = lerp(albedo, albedo * terrainTint, 0.82);
        } else {
            albedo *= i.Color.rgb;
        }
    }

    if (g_MappingMode > 0.5) {
        float localWetness = g_UseVertexColorTint > 0.5 ? saturate(i.Color.a) : 0.0;
        float flatness = saturate((N.y - 0.55) / 0.35);
        float terrainWetness = saturate(g_Wetness * flatness + localWetness * 0.85);
        float saturatedMud = localWetness * (0.35 + g_Wetness * 0.45);
        albedo *= 1.0 - terrainWetness * 0.22;
        albedo = lerp(albedo, albedo * float3(0.92, 0.97, 1.02), terrainWetness * 0.06);
        albedo = lerp(albedo, albedo * float3(0.78, 0.77, 0.76), saturatedMud * 0.24);
        roughness = lerp(roughness, max(0.06, roughness * 0.45), terrainWetness);
        roughness = lerp(roughness, max(0.045, roughness * 0.72), saturatedMud);
        ao = lerp(ao, min(1.0, ao + 0.08), terrainWetness * 0.6);
    }

    ao = saturate(ao);
    float3 V = normalize(g_CameraPosition - i.WorldPos);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float3 envDiffuseColor = float3(0.0, 0.0, 0.0);
    if (g_HasEnvironmentMap > 0.5) {
        float3 irradianceSum = float3(0.0, 0.0, 0.0);
        float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
        float3 tangent = normalize(cross(up, N));
        float3 bitangent = cross(N, tangent);

        [unroll]
        for (int sampleIndex = 0; sampleIndex < 8; ++sampleIndex) {
            float phi = (float(sampleIndex) + 0.5) / 8.0 * 2.0 * PI;
            float cosTheta = sqrt((float(sampleIndex) + 0.5) / 8.0);
            float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
            float3 sampleDir = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
            float3 worldDir = tangent * sampleDir.x + bitangent * sampleDir.y + N * sampleDir.z;
            irradianceSum += g_EquirectMap.SampleLevel(g_EquirectMap_sampler, DirToEquirectUV(worldDir), 6.0).rgb * cosTheta;
        }

        envDiffuseColor = (irradianceSum / 8.0) * 0.5;
    }

    float3 Lo = float3(0.0, 0.0, 0.0);
    if (g_LightIntensity > 0.0) {
        float3 L = normalize(-g_LightDirection);
        float3 radiance = g_LightColor * g_LightIntensity;
        float shadow = ComputeShadowFactor(i.WorldPos, N, L);
        Lo += CalculateDirectionalLight(N, V, L, albedo, roughness, metallic, F0, radiance) * shadow;
    }

    if (g_PointLightIntensity > 0.0 && g_PointLightRange > 0.0) {
        float3 Lvec = g_PointLightPosition - i.WorldPos;
        float dist = length(Lvec);
        float3 L = dist > 1e-4 ? (Lvec / dist) : float3(0.0, 1.0, 0.0);
        float att = 1.0 / (g_PointLightAttenuation.x + g_PointLightAttenuation.y * dist + g_PointLightAttenuation.z * dist * dist);
        float rangeAtt = saturate(1.0 - dist / g_PointLightRange);
        rangeAtt *= rangeAtt;
        float3 radiance = g_PointLightColor * g_PointLightIntensity * att * rangeAtt;
        float pointShadow = ComputePointShadowFactor(i.WorldPos, N, L);
        Lo += CalculateDirectionalLight(N, V, L, albedo, roughness, metallic, F0, radiance) * pointShadow;
    }

    float3 diffuse;
    float3 specular = float3(0.0, 0.0, 0.0);
    if (g_HasEnvironmentMap > 0.5) {
        float NdotV = max(dot(N, V), 0.0);
        float3 F = FresnelSchlick(NdotV, F0);
        float3 kS = F;
        float3 kD = (1.0 - kS) * (1.0 - metallic);
        diffuse = kD * albedo * envDiffuseColor;

        float mipLevel = roughness * 8.0;
        float2 specularUV = DirToEquirectUV(reflect(-V, N));
        float3 prefilteredColor = g_EquirectMap.SampleLevel(g_EquirectMap_sampler, specularUV, mipLevel).rgb;
        float2 brdf = g_BRDF_LUT.Sample(g_BRDF_LUT_sampler, float2(NdotV, roughness)).rg;
        specular = prefilteredColor * (F0 * brdf.r + brdf.g) * 0.7;
    } else {
        diffuse = albedo * float3(0.03, 0.03, 0.03);
    }

    float3 color = Lo + (diffuse + specular) * ao * 0.3;

    if (g_FogEnabled > 0.5) {
        float fogDistance = length(g_CameraPosition - i.WorldPos);
        float fogFactor = saturate(1.0 - exp(-g_FogDensity * fogDistance));
        color = lerp(color, g_FogColor, fogFactor);
    }

    float alpha = g_Opacity;
    if (alpha < 0.99) {
        color *= g_TransmissionColor;
        color *= (0.96 + g_Wetness * 0.04);
    }

    return float4(color, alpha);
}
