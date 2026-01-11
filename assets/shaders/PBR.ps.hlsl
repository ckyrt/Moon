// PBR Pixel Shader - Cook-Torrance BRDF with IBL
static const float PI = 3.14159265359;

// 材质参数常量缓冲区
cbuffer MaterialConstants { 
    float g_Metallic;
    float g_Roughness;
    float g_TriplanarTiling;  // Triplanar纹理平铺密度（默认0.5 = 每2米重复一次）
    float g_HasNormalMap;     // 是否加载了法线贴图（0.0 = 无，1.0 = 有）
    float3 g_BaseColor;
    float g_TriplanarBlend;   // Triplanar混合锐度（默认4.0）
};

// 场景参数常量缓冲区（相机位置、光源等）
cbuffer SceneConstants { 
    float3 g_CameraPosition;
    float g_HasEnvironmentMap;  // 是否有有效的环境贴图（0.0 = 无，1.0 = 有）
    float3 g_LightDirection;
    float g_Padding4;
    float3 g_LightColor;
    float g_LightIntensity;
};

// 纹理资源和采样器
Texture2D g_AlbedoMap;
SamplerState g_AlbedoMap_sampler;

Texture2D g_AOMap;
SamplerState g_AOMap_sampler;

Texture2D g_RoughnessMap;
SamplerState g_RoughnessMap_sampler;

Texture2D g_MetalnessMap;
SamplerState g_MetalnessMap_sampler;

Texture2D g_NormalMap;
SamplerState g_NormalMap_sampler;

Texture2D g_EquirectMap;
SamplerState g_EquirectMap_sampler;

// BRDF LUT for IBL (256x256 RG16F)
Texture2D g_BRDF_LUT;
SamplerState g_BRDF_LUT_sampler;

struct PSInput { 
    float4 Pos      : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 NormalWS : NORMAL;
    float4 Color    : COLOR;
    float2 UV       : TEXCOORD;  // 保留用于非CSG物体
};

// ===== Triplanar Mapping 函数 =====
// Unity/Unreal标准实现，基于世界坐标采样
// 优点：Transform无关、布尔切割后纹理连续、缩放不变形
float3 GetTriplanarWeights(float3 normal, float sharpness) {
    float3 weights = abs(normal);
    weights = pow(weights, sharpness);  // 锐化过渡
    weights /= (weights.x + weights.y + weights.z);  // 归一化
    return weights;
}

// 法线专用：柔和的权重计算（避免45°面突然变硬）
float3 GetTriplanarWeightsForNormals(float3 normal) {
    float3 weights = abs(normal);
    // 法线不使用pow，保持平滑过渡
    weights /= (weights.x + weights.y + weights.z);
    return weights;
}

float4 SampleTriplanar(Texture2D tex, SamplerState samp, float3 worldPos, float3 normal, float tiling) {
    float3 weights = GetTriplanarWeights(normal, g_TriplanarBlend);
    
    // 三个平面采样（使用世界坐标）
    float4 sampleX = tex.Sample(samp, worldPos.yz * tiling);
    float4 sampleY = tex.Sample(samp, worldPos.xz * tiling);
    float4 sampleZ = tex.Sample(samp, worldPos.xy * tiling);
    
    // 加权混合
    return sampleX * weights.x + sampleY * weights.y + sampleZ * weights.z;
}

// ===== Triplanar Normal Map 采样（业界标准）=====
// Unity/Unreal CSG标准做法：世界空间法线贴图混合
// 优点：不依赖UV，不需要TBN，布尔切割后法线连续
float3 SampleTriplanarNormal(
    Texture2D normalMap,
    SamplerState samp,
    float3 worldPos,
    float3 normalWS,
    float tiling
) {
    // ✅ 法线使用柔和权重（不pow），避免45°面突然变硬
    float3 weights = GetTriplanarWeightsForNormals(normalWS);
    
    // 三个平面采样法线贴图
    float3 nX = normalMap.Sample(samp, worldPos.yz * tiling).xyz * 2.0 - 1.0;
    float3 nY = normalMap.Sample(samp, worldPos.xz * tiling).xyz * 2.0 - 1.0;
    float3 nZ = normalMap.Sample(samp, worldPos.xy * tiling).xyz * 2.0 - 1.0;
    
    // DirectX normal map: flip Y（与单次采样保持一致）
    nX.y = -nX.y;
    nY.y = -nY.y;
    nZ.y = -nZ.y;
    
    // ✅ 将各投影面的切线空间法线转换到世界空间，带符号修正
    // X投影（YZ平面）：tangent=Y, bitangent=Z, normal=X
    float3 worldNX = float3(
        normalWS.x > 0 ? nX.z : -nX.z,  // 符号修正，防止法线翻转
        nX.x,
        nX.y
    );
    
    // Y投影（XZ平面）：tangent=X, bitangent=Z, normal=Y
    float3 worldNY = float3(
        nY.x,
        normalWS.y > 0 ? nY.z : -nY.z,  // 符号修正
        nY.y
    );
    
    // Z投影（XY平面）：tangent=X, bitangent=Y, normal=Z
    float3 worldNZ = float3(
        nZ.x,
        nZ.y,
        normalWS.z > 0 ? nZ.z : -nZ.z  // 符号修正
    );
    
    // 加权混合并归一化
    float3 blended = 
        worldNX * weights.x +
        worldNY * weights.y +
        worldNZ * weights.z;
    
    return normalize(blended);
}

// 将 3D 方向转换为 equirectangular UV 坐标
float2 DirToEquirectUV(float3 dir) {
    float u = atan2(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float v = 1.0 - (asin(dir.y) / PI + 0.5);
    return float2(u, v);
}

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
    
    // ===== Triplanar采样（世界空间，Transform无关）=====
    float3 albedo = SampleTriplanar(g_AlbedoMap, g_AlbedoMap_sampler, i.WorldPos, N, g_TriplanarTiling).rgb;
    
    // === 材质贴图采样（Unity/Unreal标准：贴图×constant）===
    float ao = SampleTriplanar(g_AOMap, g_AOMap_sampler, i.WorldPos, N, g_TriplanarTiling).r;
    float roughness = max(SampleTriplanar(g_RoughnessMap, g_RoughnessMap_sampler, i.WorldPos, N, g_TriplanarTiling).r * g_Roughness, 0.04);
    float metallic = saturate(SampleTriplanar(g_MetalnessMap, g_MetalnessMap_sampler, i.WorldPos, N, g_TriplanarTiling).r * g_Metallic);
    
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
        
        Lo += CalculateDirectionalLight(N, V, L, albedo, roughness, metallic, F0, radiance);
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
    
    // 组合环境光（间接光）
    float3 ambient = (diffuse + specular) * ao;  // AO只影响环境光
    
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
    
    return float4(color, i.Color.a);
}
