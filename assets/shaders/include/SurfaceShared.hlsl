#ifndef SURFACE_SHARED_HLSL
#define SURFACE_SHARED_HLSL

cbuffer MaterialConstants {
    float g_Metallic;
    float g_Roughness;
    float g_TriplanarTiling;
    float g_MappingMode;
    float3 g_BaseColor;
    float g_TriplanarBlend;
    float g_HasNormalMap;
    float g_Opacity;
    float g_UseVertexColorTint;
    float g_AlphaCutoff;
    float3 g_TransmissionColor;
    float g_Padding3;
};

cbuffer SceneConstants {
    float3 g_CameraPosition;
    float g_HasEnvironmentMap;
    float3 g_LightDirection;
    float g_Padding4;
    float3 g_LightColor;
    float g_LightIntensity;
    float3 g_PointLightPosition;
    float g_PointLightRange;
    float3 g_PointLightColor;
    float g_PointLightIntensity;
    float3 g_PointLightAttenuation;
    float g_PointLightPadding;
    float3 g_FogColor;
    float g_FogDensity;
    float3 g_SkyColor;
    float g_FogEnabled;
    float g_CloudCoverage;
    float g_Wetness;
    float g_WindStrength;
    float g_TimeSeconds;
    float g_PrecipitationIntensity;
    float g_SnowAmount;
    float g_ScenePadding0;
    float g_ScenePadding1;
};

#endif // SURFACE_SHARED_HLSL
