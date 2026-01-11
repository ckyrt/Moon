// ============================================================================
// PBR Lighting Functions
// BRDF、光照计算（Cook-Torrance + IBL）
// ============================================================================

#ifndef PBR_LIGHTING_HLSL
#define PBR_LIGHTING_HLSL

// Fresnel-Schlick 近似（菲涅尔效应）
float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// GGX/Trowbridge-Reitz 法线分布函数（NDF）
float DistributionGGX(float3 N, float3 H, float roughness) {
    // ✅ 正确做法：在这里防止除0，而不是修改材质的roughness值
    float a = roughness * roughness;
    float a2 = max(a * a, 0.001); // 防止完全光滑表面导致的数值问题
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return nom / max(denom, 0.0001);
}

// Smith GGX 几何遮挡函数
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return nom / max(denom, 0.0001);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// ✅ 封装直接光照计算（支持未来多光源扩展）
// 计算单个方向光的贡献
float3 CalculateDirectionalLight(
    float3 N,           // 表面法线
    float3 V,           // 视线方向
    float3 L,           // 光源方向
    float3 albedo,      // 漫反射颜色
    float roughness,    // 粗糙度
    float metallic,     // 金属度
    float3 F0,          // 基础反射率
    float3 radiance     // 光源辐射度
) {
    float3 H = normalize(V + L);
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    float3 specular = numerator / max(denominator, 0.001);
    
    // 能量守恒
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallic;  // 金属没有漫反射
    
    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

#endif // PBR_LIGHTING_HLSL
