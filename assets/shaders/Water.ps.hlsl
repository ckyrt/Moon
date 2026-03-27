// Water Pixel Shader
#include "include/PBR_Common.hlsl"
#include "include/PBR_Lighting.hlsl"

float HashNoise2D(float2 p)
{
    float h = dot(p, float2(127.1, 311.7));
    return frac(sin(h) * 43758.5453123);
}

float WavePhase(float2 worldXZ, float2 direction, float spatialScale, float speed, float timeSeconds, float phaseOffset)
{
    float2 dir = normalize(direction);
    return dot(worldXZ, dir) * spatialScale + timeSeconds * speed + phaseOffset;
}

float ComputeRipple(float2 worldXZ, float timeSeconds, float windStrength)
{
    float waveA = sin(WavePhase(worldXZ, float2(0.94, 0.22), 0.020, 0.70 + windStrength * 0.90, timeSeconds, 0.0));
    float waveB = cos(WavePhase(worldXZ, float2(-0.31, 0.95), 0.027, 0.54 + windStrength * 0.75, timeSeconds, 1.7));
    float waveC = sin(WavePhase(worldXZ, float2(0.58, -0.82), 0.043, 0.92 + windStrength * 1.10, timeSeconds, 3.1));
    float waveD = cos(WavePhase(worldXZ, float2(0.12, 0.99), 0.061, 1.25 + windStrength * 1.35, timeSeconds, 4.4));
    float lowFrequencyNoise = HashNoise2D(floor(worldXZ * 0.015 + timeSeconds * 0.04));
    float combined = waveA * 0.34 + waveB * 0.28 + waveC * 0.22 + waveD * 0.16;
    return saturate(0.5 + combined * 0.35 + (lowFrequencyNoise - 0.5) * 0.10);
}

float3 ComputeWaterNormal(float2 worldXZ, float timeSeconds, float windStrength)
{
    float t = timeSeconds;
    float h1 = sin(WavePhase(worldXZ, float2(0.92, 0.38), 0.018, 0.62 + windStrength * 0.85, t, 0.0));
    float h2 = cos(WavePhase(worldXZ, float2(-0.44, 0.90), 0.026, 0.48 + windStrength * 0.70, t, 1.4));
    float h3 = sin(WavePhase(worldXZ, float2(0.19, -0.98), 0.041, 0.95 + windStrength * 1.20, t, 2.6));
    float h4 = cos(WavePhase(worldXZ, float2(0.73, 0.68), 0.058, 1.34 + windStrength * 1.55, t, 4.2));

    float slopeX =
        cos(WavePhase(worldXZ, float2(0.92, 0.38), 0.018, 0.62 + windStrength * 0.85, t, 0.0)) * 0.018 * 0.92 +
        -sin(WavePhase(worldXZ, float2(-0.44, 0.90), 0.026, 0.48 + windStrength * 0.70, t, 1.4)) * 0.026 * -0.44 +
        cos(WavePhase(worldXZ, float2(0.19, -0.98), 0.041, 0.95 + windStrength * 1.20, t, 2.6)) * 0.041 * 0.19 +
        -sin(WavePhase(worldXZ, float2(0.73, 0.68), 0.058, 1.34 + windStrength * 1.55, t, 4.2)) * 0.058 * 0.73;

    float slopeZ =
        cos(WavePhase(worldXZ, float2(0.92, 0.38), 0.018, 0.62 + windStrength * 0.85, t, 0.0)) * 0.018 * 0.38 +
        -sin(WavePhase(worldXZ, float2(-0.44, 0.90), 0.026, 0.48 + windStrength * 0.70, t, 1.4)) * 0.026 * 0.90 +
        cos(WavePhase(worldXZ, float2(0.19, -0.98), 0.041, 0.95 + windStrength * 1.20, t, 2.6)) * 0.041 * -0.98 +
        -sin(WavePhase(worldXZ, float2(0.73, 0.68), 0.058, 1.34 + windStrength * 1.55, t, 4.2)) * 0.058 * 0.68;

    float waveEnergy = abs(h1) * 0.26 + abs(h2) * 0.24 + abs(h3) * 0.28 + abs(h4) * 0.22;
    float choppiness = 0.11 + windStrength * 0.20 + waveEnergy * 0.04;
    return normalize(float3(-slopeX * choppiness, 1.0, -slopeZ * choppiness));
}

float ComputeRiverFlowHighlight(float2 uv, float2 worldXZ, float timeSeconds, float windStrength)
{
    float uvBand = sin(uv.y * 37.0 - timeSeconds * (0.95 + windStrength * 1.45));
    float worldBand = cos(dot(worldXZ, normalize(float2(0.62, 0.78))) * 0.022 - timeSeconds * (0.58 + windStrength * 0.95) + 1.3);
    return saturate(0.5 + uvBand * 0.24 + worldBand * 0.18);
}

float4 main(in PSInput i) : SV_Target
{
    float surfaceAlpha = saturate(g_Opacity);
    float channelCenter = 1.0 - saturate(abs(i.UV.x - 0.5) * 2.0);
    float shallowWater = 1.0 - channelCenter;
    float deepWater = channelCenter;
    float waterRipple = ComputeRipple(i.WorldPos.xz, g_TimeSeconds, g_WindStrength);
    float alongFlowHighlight = ComputeRiverFlowHighlight(i.UV, i.WorldPos.xz, g_TimeSeconds, g_WindStrength);
    float edgeFoam = pow(saturate(1.0 - abs(i.UV.x - 0.5) * 2.0), 6.0);

    float3 N = ComputeWaterNormal(i.WorldPos.xz, g_TimeSeconds, g_WindStrength);
    float3 V = normalize(g_CameraPosition - i.WorldPos);

    float3 shallowTint = float3(0.38, 0.62, 0.64);
    float3 deepTint = float3(0.05, 0.16, 0.26);
    float3 baseTint = g_BaseColor;
    float3 albedo = lerp(baseTint, shallowTint, shallowWater * (0.34 + edgeFoam * 0.12));
    albedo = lerp(albedo, deepTint, deepWater * 0.18);
    albedo += float3(0.03, 0.06, 0.07) * alongFlowHighlight * deepWater * 0.55;
    albedo *= 0.93 + waterRipple * (0.08 + g_WindStrength * 0.05);

    float roughness = lerp(max(g_Roughness, 0.03), 0.040 + (1.0 - waterRipple) * 0.05, 0.72);
    float metallic = max(g_Metallic, 0.02);
    float ao = 1.0;
    float3 F0 = lerp(float3(0.02, 0.035, 0.05), float3(0.06, 0.09, 0.13), waterRipple);

    float3 envDiffuseColor = float3(0.0, 0.0, 0.0);
    float3 diffuse = float3(0.0, 0.0, 0.0);
    float3 specular = float3(0.0, 0.0, 0.0);
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

        float NdotV = saturate(dot(N, V));
        float3 F = FresnelSchlick(NdotV, F0);
        float3 kS = F;
        float3 kD = (1.0 - kS) * (1.0 - metallic);
        diffuse = kD * albedo * envDiffuseColor;

        float mipLevel = roughness * 8.0;
        float2 specularUV = DirToEquirectUV(reflect(-V, N));
        float3 prefilteredColor = g_EquirectMap.SampleLevel(g_EquirectMap_sampler, specularUV, mipLevel).rgb;
        float2 brdf = g_BRDF_LUT.Sample(g_BRDF_LUT_sampler, float2(NdotV, roughness)).rg;
        specular = prefilteredColor * (F0 * brdf.r + brdf.g) * 0.8;
    }

    float3 Lo = float3(0.0, 0.0, 0.0);
    if (g_LightIntensity > 0.0) {
        float3 L = normalize(-g_LightDirection);
        float3 radiance = g_LightColor * g_LightIntensity;
        Lo += CalculateDirectionalLight(N, V, L, albedo, roughness, metallic, F0, radiance);
    }

    if (g_PointLightIntensity > 0.0 && g_PointLightRange > 0.0) {
        float3 Lvec = g_PointLightPosition - i.WorldPos;
        float dist = length(Lvec);
        float3 L = dist > 1e-4 ? (Lvec / dist) : float3(0.0, 1.0, 0.0);
        float att = 1.0 / (g_PointLightAttenuation.x + g_PointLightAttenuation.y * dist + g_PointLightAttenuation.z * dist * dist);
        float rangeAtt = saturate(1.0 - dist / g_PointLightRange);
        rangeAtt *= rangeAtt;
        float3 radiance = g_PointLightColor * g_PointLightIntensity * att * rangeAtt;
        Lo += CalculateDirectionalLight(N, V, L, albedo, roughness, metallic, F0, radiance);
    }

    float NdotVWater = saturate(dot(N, V));
    float waterFresnel = pow(1.0 - NdotVWater, 5.0);
    float flowSheen = alongFlowHighlight * (0.08 + deepWater * 0.14);
    float3 refractedTint = lerp(float3(0.16, 0.30, 0.34), g_SkyColor, 0.28 + waterRipple * 0.18);
    refractedTint = lerp(refractedTint, float3(0.45, 0.62, 0.58), shallowWater * 0.42);
    float3 waterTransmission = refractedTint * (0.16 + (1.0 - waterFresnel) * 0.28 + shallowWater * 0.12);
    specular += lerp(float3(0.04, 0.06, 0.09), float3(0.14, 0.20, 0.25), waterFresnel) *
        (0.24 + waterRipple * 0.16 + flowSheen);

    float3 ambient = (diffuse + specular) * ao * 0.3;
    ambient += waterTransmission;
    ambient += float3(0.02, 0.04, 0.05) * alongFlowHighlight * (0.22 + deepWater * 0.16);

    float3 color = Lo + ambient;
    float foamPulse = 0.55 + 0.45 * sin(g_TimeSeconds * 1.8 + i.WorldPos.x * 0.04 + i.WorldPos.z * 0.06);
    float foam = edgeFoam * (0.08 + g_WindStrength * 0.14 + waterRipple * 0.05) * foamPulse;
    float shallowFoam = shallowWater * (0.03 + alongFlowHighlight * 0.04);
    color += float3(0.82, 0.88, 0.92) * (foam + shallowFoam);
    color = lerp(color * 0.95, color + specular * 0.10, waterFresnel);
    color *= g_TransmissionColor;
    color *= (0.97 + g_Wetness * 0.03);

    if (g_FogEnabled > 0.5) {
        float fogDistance = length(g_CameraPosition - i.WorldPos);
        float fogFactor = saturate(1.0 - exp(-g_FogDensity * fogDistance));
        color = lerp(color, g_FogColor, fogFactor);
    }

    return float4(color, surfaceAlpha);
}
