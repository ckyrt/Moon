// ============================================================================
// PBR Pixel Shader - Modular Version
// Cook-Torrance BRDF with IBL
// 
// 模块化结构：
// - PBR_Common.hlsl     : 常量、cbuffer、纹理声明、输入结构
// - PBR_Triplanar.hlsl  : Triplanar映射函数（用于CSG）
// - PBR_Lighting.hlsl   : BRDF、光照计算
// ============================================================================

#include "include/PBR_Common.hlsl"
#include "include/PBR_Triplanar.hlsl"
#include "include/PBR_Lighting.hlsl"
#include "include/PBR_Shadow.hlsl"

// ============================================================================
// Main Pixel Shader
// ============================================================================

float4 main(in PSInput i) : SV_Target {
    // 🔍 调试开关（取消注释查看不同的调试信息）
    // 0 = 正常渲染
    // 1 = 显示法线贴图原始颜色
    // 2 = 显示世界空间法线（查看法线贴图是否影响光照）
    // 3 = 显示 ARM 贴图（R=AO, G=Roughness, B=Metallic）
    // 4 = 只显示 AO
    // 5 = 只显示 Roughness
    // 6 = 显示顶点法线（未应用法线贴图）
    // 7 = 显示法线贴图（切线空间，转换后）
    // 8 = 显示切线T（红色）
    // 9 = 显示副切线B（绿色）
    // 10 = 显示Albedo贴图
    // 11 = 只显示直接光照
    // 12 = 只显示环境光
    int debugMode = 0;  // 恢复正常渲染
    
    // ✅ 重要：先定义N，然后再使用！
    float3 N = normalize(i.NormalWS);
    
    // 🔍 调试模式 6: 显示顶点法线（未应用法线贴图）
    if (debugMode == 6) {
        return float4(N * 0.5 + 0.5, 1.0);
    }
    
    // ===== ⚠️ 显式分支：Triplanar vs UV =====
    // 不要试图统一！CSG和普通模型是两种完全不同的场景
    float3 albedo;
    float ao;
    float roughness;
    float metallic;
    
    if (g_MappingMode > 0.5) {
        // ===== Triplanar模式（CSG/程序化几何）=====
        albedo = SampleTriplanar(g_AlbedoMap, g_AlbedoMap_sampler, i.WorldPos, N, g_TriplanarTiling).rgb;
        ao = SampleTriplanar(g_AOMap, g_AOMap_sampler, i.WorldPos, N, g_TriplanarTiling).r;
        roughness = max(SampleTriplanar(g_RoughnessMap, g_RoughnessMap_sampler, i.WorldPos, N, g_TriplanarTiling).r * g_Roughness, 0.04);
        metallic = saturate(SampleTriplanar(g_MetalnessMap, g_MetalnessMap_sampler, i.WorldPos, N, g_TriplanarTiling).r * g_Metallic);
        
        // Triplanar法线
        if (g_HasNormalMap > 0.5) {
            float3 triplanarNormal = SampleTriplanarNormal(
                g_NormalMap,
                g_NormalMap_sampler,
                i.WorldPos,
                N,
                g_TriplanarTiling
            );
            
            // 🔍 调试模式 1: 显示triplanar法线
            if (debugMode == 1) {
                return float4(triplanarNormal * 0.5 + 0.5, 1.0);
            }
            
            float normalStrength = 1.0;  // 与UV模式保持一致
            N = normalize(lerp(N, triplanarNormal, normalStrength));
        }
    } else {
        // ===== UV模式（普通模型）=====
        albedo = g_AlbedoMap.Sample(g_AlbedoMap_sampler, i.UV).rgb;
        ao = g_AOMap.Sample(g_AOMap_sampler, i.UV).r;
        roughness = max(g_RoughnessMap.Sample(g_RoughnessMap_sampler, i.UV).r * g_Roughness, 0.04);
        metallic = saturate(g_MetalnessMap.Sample(g_MetalnessMap_sampler, i.UV).r * g_Metallic);
        
        // UV模式的法线：使用TBN
        if (g_HasNormalMap > 0.5) {
            float3 normalMap = g_NormalMap.Sample(g_NormalMap_sampler, i.UV).rgb;
            
            // 🔍 调试模式 1: 显示法线贴图原始颜色
            if (debugMode == 1) {
                return float4(normalMap, 1.0);
            }
            
            // 检查法线贴图是否有效（非零）
            if (dot(normalMap, normalMap) > 0.001) {
                // 将法线从 [0,1] 转换到 [-1,1]
                normalMap = normalMap * 2.0 - 1.0;
                normalMap.y = -normalMap.y;  // DirectX翻转Y
                
                // 法线强度
                float normalStrength = 1.0;
                normalMap.xy *= normalStrength;
                normalMap.z = sqrt(saturate(1.0 - dot(normalMap.xy, normalMap.xy)));
                
                // TBN计算（屏幕空间导数）
                float3 dPos_dx = ddx_coarse(i.WorldPos);
                float3 dPos_dy = ddy_coarse(i.WorldPos);
                float2 dUV_dx = ddx_coarse(i.UV);
                float2 dUV_dy = ddy_coarse(i.UV);
                
                float3 T = dPos_dx * dUV_dy.y - dPos_dy * dUV_dx.y;
                float3 B = dPos_dy * dUV_dx.x - dPos_dx * dUV_dy.x;
                
                // TBN退化保护
                float TdotT = dot(T, T);
                float BdotB = dot(B, B);
                if (TdotT > 1e-6 && BdotB > 1e-6) {
                    float invMax = rsqrt(max(TdotT, BdotB));
                    float3x3 TBN = float3x3(T * invMax, B * invMax, N);
                    N = normalize(mul(normalMap, TBN));
                }
            }
        }
    }
    
    // 🔍 调试模式 3: 显示材质贴图 (R=AO, G=Roughness, B=Metallic)
    if (debugMode == 3) {
        return float4(ao, roughness, metallic, 1.0);
    }
    
    // 🔍 调试模式 4: 只显示 AO
    if (debugMode == 4) {
        return float4(ao, ao, ao, 1.0);
    }
    
    // 🔍 调试模式 5: 只显示 Roughness
    if (debugMode == 5) {
        return float4(roughness, roughness, roughness, 1.0);
    }
    
    // 🔍 调试模式 10: 显示Albedo贴图原始颜色
    if (debugMode == 10) {
        return float4(albedo, 1.0);
    }
    
    // Unity 标准做法：Albedo 贴图与 BaseColor 相乘（作为 tint color）
    if (dot(albedo, albedo) > 0.001) {
        // 贴图存在：贴图颜色 × BaseColor（用作色调调整）
        albedo *= g_BaseColor;
    } else {
        // 贴图不存在：直接使用 BaseColor
        albedo = g_BaseColor;
    }
    
    // 不要用顶点颜色调制纹理！这会让纹理变暗
    // albedo *= i.Color.rgb;
    
    // ===== Triplanar Normal Map（CSG专用，不依赖UV）=====
    // ⚠️ CSG的UV不连续，不能用TBN！必须用世界空间triplanar采样
    float normalStrength = 0.5;  // 控制法线强度（0=平滑，1=完全使用贴图）
    
    // ✅ 使用C++端的flag判断（Unity/Unreal标准做法）
    if (g_HasNormalMap > 0.5) {
        // 采样triplanar normal（世界空间）
        float3 triplanarNormal = SampleTriplanarNormal(
            g_NormalMap,
            g_NormalMap_sampler,
            i.WorldPos,
            N,
            g_TriplanarTiling
        );
        
        // 🔍 调试模式 1: 显示triplanar法线
        if (debugMode == 1) {
            return float4(triplanarNormal * 0.5 + 0.5, 1.0);
        }
        
        // 混合顶点法线和贴图法线（lerp控制强度）
        N = normalize(lerp(N, triplanarNormal, normalStrength));
    }
    
    // 🔍 调试模式 2: 显示世界空间法线（查看法线贴图是否影响光照）
    if (debugMode == 2) {
        return float4(N * 0.5 + 0.5, 1.0);
    }
    
    // AO已经在前面处理，直接saturate确保范围
    ao = saturate(ao);
    
    float3 V = normalize(g_CameraPosition - i.WorldPos);
    
    // 基础反射率（F0）
    float3 F0 = float3(0.04, 0.04, 0.04);  // 非金属默认值
    F0 = lerp(F0, albedo, metallic);       // 金属使用 albedo 作为 F0
    
    // ⚠️ Unity/Unreal 标准：IBL 需要两个独立的采样
    // 1) 镜面反射方向（用于高光）
    // 2) 法线方向（用于漫反射 Irradiance）
    float3 R = reflect(-V, N);  // 镜面反射方向
    
    // 采样环境贴图（如果可用）
    float3 envDiffuseColor = float3(0.0, 0.0, 0.0);
    
    if (g_HasEnvironmentMap > 0.5) {
        // ⚠️ TODO: 最佳实践是预计算 32x32 的 irradiance cubemap
        // 当前方案：改进的半球采样（法线为中心的Monte Carlo近似）
        // 这比6方向采样更符合物理，因为它考虑了法线方向的半球分布
        
        float3 irradianceSum = float3(0.0, 0.0, 0.0);
        
        // 构建以法线N为Z轴的正交基（Frisvad方法，数值稳定）
        float3 up = abs(N.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
        float3 tangent = normalize(cross(up, N));
        float3 bitangent = cross(N, tangent);
        
        // 在法线周围的半球采样（固定采样模式，避免噪点）
        // 使用分层采样保证覆盖均匀，减少采样数提高性能
        const int numSamples = 8;
        for (int i = 0; i < numSamples; i++) {
            // 使用低差异序列（Hammersley）
            float phi = (float(i) + 0.5) / float(numSamples) * 2.0 * PI;
            float cosTheta = sqrt((float(i) + 0.5) / float(numSamples)); // 余弦加权
            float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
            
            // 半球方向（局部坐标）
            float3 sampleDir = float3(
                cos(phi) * sinTheta,
                sin(phi) * sinTheta,
                cosTheta
            );
            
            // 转换到世界空间
            float3 worldDir = tangent * sampleDir.x + bitangent * sampleDir.y + N * sampleDir.z;
            
            // 采样环境贴图（使用较高的mip level确保模糊）
            irradianceSum += g_EquirectMap.SampleLevel(g_EquirectMap_sampler, DirToEquirectUV(worldDir), 6.0).rgb * cosTheta;
        }
        
        // ✅ 修正：简化的漫反射近似，使用平均值并降低强度
        // 完整的Monte Carlo积分应该是 (2*PI/N) * sum，但这里我们用更保守的系数
        // 避免过度曝光，同时保持合理的环境光亮度
        envDiffuseColor = (irradianceSum / float(numSamples)) * 0.5;
    }
    
    // === 直接光照（方向光）===
    // ✅ 使用封装的函数，未来可以轻松扩展到多光源
    // TODO: 支持光源数组 / Clustered Shading
    float3 Lo = float3(0.0, 0.0, 0.0);
    
    if (g_LightIntensity > 0.0) {
        float3 L = normalize(-g_LightDirection);  // 光源方向（指向光源）
        float3 radiance = g_LightColor * g_LightIntensity;

        // Shadow factor only affects direct lighting
        float shadow = ComputeShadowFactor(i.WorldPos, N, L);
        Lo += CalculateDirectionalLight(N, V, L, albedo, roughness, metallic, F0, radiance) * shadow;
    }

    // === 直接光照（点光源，单个）===
    if (g_PointLightIntensity > 0.0 && g_PointLightRange > 0.0) {
        float3 Lvec = g_PointLightPosition - i.WorldPos;
        float dist = length(Lvec);
        float invDist = (dist > 1e-4) ? (1.0 / dist) : 0.0;
        float3 L = Lvec * invDist;

        // 距离衰减 + range 软裁剪
        float att = 1.0 / (g_PointLightAttenuation.x + g_PointLightAttenuation.y * dist + g_PointLightAttenuation.z * dist * dist);
        float t = saturate(1.0 - dist / g_PointLightRange);
        float rangeAtt = t * t;
        float3 radiance = g_PointLightColor * g_PointLightIntensity * att * rangeAtt;

        float pointShadow = ComputePointShadowFactor(i.WorldPos, N, L);
        Lo += CalculateDirectionalLight(N, V, L, albedo, roughness, metallic, F0, radiance) * pointShadow;
    }
    
    // === 环境光照（IBL - Image Based Lighting）===
    // ✅ 移除人工baseAmbient，让IBL亮度完全来自HDR环境本身
    // 这避免了室内/室外/HDR场景的过曝或灰雾问题
    
    bool hasIBL = (g_HasEnvironmentMap > 0.5);
    
    // === Unity/Unreal 标准 IBL 实现 ===
    float3 diffuse;
    
    if (hasIBL) {
        // Unity 标准：环境漫反射使用标准的 BRDF kD 权重
        float3 F = FresnelSchlick(max(dot(N, V), 0.0), F0);
        float3 kS = F;
        float3 kD = 1.0 - kS;  // 标准能量守恒，不做人工限制
        kD *= 1.0 - metallic;  // 金属没有漫反射
        
        // 使用从环境贴图采样的 Irradiance（法线方向）
        diffuse = kD * albedo * envDiffuseColor;
    } else {
        // 没有IBL：使用微弱的默认环境光（避免完全黑屏）
        // 注意：这里不应该是主要的光照来源
        float3 irradiance = float3(0.03, 0.03, 0.03);
        diffuse = albedo * irradiance;
    }
    
    // 镜面反射环境光：只在有有效环境贴图时启用
    float3 specular = float3(0.0, 0.0, 0.0);
    if (hasIBL) {
        // Unity/Unreal 标准：环境镜面反射 BRDF with Split-Sum Approximation
        float NdotV = max(dot(N, V), 0.0);
        
        // ⚠️ TODO: 当前使用equirect直接采样，存在以下问题：
        // 1. Equirect的mip不是按solid angle分布，高roughness时能量不对
        // 2. 应该预计算 Equirect → Cubemap → Prefiltered Cubemap
        //
        // 标准流程（Unreal/Unity）：
        // prefilteredColor = PrefilteredCubemap.SampleLevel(R, mipLevel)
        // brdf = BRDF_LUT.Sample(float2(NdotV, roughness))
        // specular = prefilteredColor * (F0 * brdf.r + brdf.g)
        
        // ✅ Unreal Engine 标准：根据粗糙度采样不同 mipmap level
        // Roughness 0 → mipmap 0（清晰反射）
        // Roughness 1 → mipmap 8+（完全模糊）
        float mipLevel = roughness * 8.0;
        
        // 使用 SampleLevel 手动指定 mipmap（这会模糊粗糙表面的反射）
        float2 specularUV = DirToEquirectUV(R);
        float3 prefilteredColor = g_EquirectMap.SampleLevel(g_EquirectMap_sampler, specularUV, mipLevel).rgb;
        
        // ✅ Split-Sum Approximation with BRDF LUT
        // BRDF LUT存储了 ∫(F * G * V) dω 的预计算结果
        // .r = scale (F0 multiplier)
        // .g = bias (constant offset)
        float2 brdf = g_BRDF_LUT.Sample(g_BRDF_LUT_sampler, float2(NdotV, roughness)).rg;
        
        // Epic Games标准公式：L = prefilteredColor * (F0 * brdf.x + brdf.y)
        // 这正确地处理了Fresnel和几何遮挡
        specular = prefilteredColor * (F0 * brdf.r + brdf.g);
        
        // 临时降低强度防止过曝（后续使用prefiltered cubemap后可移除）
        specular *= 0.7;
    }
    
    // 组合环境光（间接光）- 降低强度保留法线凹凸细节
    float3 ambient = (diffuse + specular) * ao * 0.3;  // AO + 环境光强度系数
    
    // 🔍 调试模式 11: 只显示直接光照（不含环境光）
    if (debugMode == 11) {
        return float4(Lo, 1.0);
    }
    
    // 🔍 调试模式 12: 只显示环境光（不含直接光）
    if (debugMode == 12) {
        return float4(ambient, 1.0);
    }
    
    // 最终颜色 = 直接光 + 环境光
    float3 color = Lo + ambient;
    
    // Gamma 校正（简化）
    // color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));
    
    // 透明度处理：
    // - 对于不透明物体（opacity >= OPACITY_THRESHOLD），alpha=1.0，正常渲染
    // - 对于透明物体（如玻璃），alpha=opacity，GPU自动混合
    // - transmissionColor影响透明材质的基础颜色（用于有色玻璃）
    // 注意：OPACITY_THRESHOLD 在 C++ 端定义为 0.99f
    float alpha = g_Opacity;
    if (alpha < 0.99) {
        // 透明材质：transmissionColor作为透射光的色调（轻微影响）
        color *= g_TransmissionColor;

    }
    
    return float4(color, alpha);
}

