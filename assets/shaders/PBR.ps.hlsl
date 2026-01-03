// PBR Pixel Shader - Cook-Torrance BRDF with IBL
static const float PI = 3.14159265359;

// 材质参数常量缓冲区
cbuffer MaterialConstants { 
    float g_Metallic;
    float g_Roughness;
    float2 g_Padding1;
    float3 g_BaseColor;
    float g_Padding2;
};

// 场景参数常量缓冲区（相机位置、光源等）
cbuffer SceneConstants { 
    float3 g_CameraPosition;
    float g_Padding3;
    float3 g_LightDirection;
    float g_Padding4;
    float3 g_LightColor;
    float g_LightIntensity;
};

// 纹理资源和采样器
Texture2D g_AlbedoMap;
SamplerState g_AlbedoMap_sampler;

Texture2D g_ARMMap;
SamplerState g_ARMMap_sampler;

Texture2D g_NormalMap;
SamplerState g_NormalMap_sampler;

Texture2D g_EquirectMap;
SamplerState g_EquirectMap_sampler;

struct PSInput { 
    float4 Pos      : SV_POSITION;
    float3 WorldPos : POSITION;
    float3 NormalWS : NORMAL;
    float4 Color    : COLOR;
    float2 UV       : TEXCOORD0;
};

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
    float a = roughness * roughness;
    float a2 = a * a;
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
    
    // 从贴图采样 Albedo
    float3 albedo = g_AlbedoMap.Sample(g_AlbedoMap_sampler, i.UV).rgb;
    
    // === ARM 贴图采样（必须在所有 debug 分支之前，确保 shader 不会优化掉）===
    float3 armSample = g_ARMMap.Sample(g_ARMMap_sampler, i.UV).rgb;
    
    // 🔍 调试模式 3: 显示 ARM 贴图
    if (debugMode == 3) {
        return float4(armSample, 1.0);
    }
    
    // 🔍 调试模式 4: 只显示 AO（红色通道）
    if (debugMode == 4) {
        float ao = armSample.r;
        return float4(ao, ao, ao, 1.0);
    }
    
    // 🔍 调试模式 5: 只显示 Roughness（绿色通道）
    if (debugMode == 5) {
        float roughness = armSample.g;
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
    
    // === 法线贴图采样和处理 ===
    float3 N = normalize(i.NormalWS);
    
    // 🔍 调试模式 6: 显示顶点法线（未应用法线贴图）
    if (debugMode == 6) {
        return float4(N * 0.5 + 0.5, 1.0);
    }
    
    // 采样法线贴图
    float3 normalMap = g_NormalMap.Sample(g_NormalMap_sampler, i.UV).rgb;
    
    // 🔍 调试模式 1: 显示法线贴图原始颜色
    if (debugMode == 1) {
        return float4(normalMap, 1.0);
    }
    
    // 检查法线贴图是否有效（非零）
    if (dot(normalMap, normalMap) > 0.001) {
        // 将法线从 [0,1] 转换到 [-1,1]
        normalMap = normalMap * 2.0 - 1.0;
        
        // ⚠️ 重要：DirectX 法线贴图（_nor_dx）需要翻转 Y 通道！
        // DirectX 使用 Y-down，我们使用 Y-up
        // 如果不翻转，凹凸方向会完全相反，导致看起来很平
        normalMap.y = -normalMap.y;
        
        // 🔍 调试模式 7: 显示转换到[-1,1]后的法线贴图
        if (debugMode == 7) {
            return float4(normalMap * 0.5 + 0.5, 1.0);
        }
        
        // === Unity/Unreal 标准法线强度处理 ===
        // 简单缩放XY，然后重建Z（保证法线长度为1）
        float normalStrength = 2.0;  // 业界标准范围：0.0-3.0
        normalMap.xy *= normalStrength;
        // 重建Z分量（使用saturate防止sqrt(负数)）
        normalMap.z = sqrt(saturate(1.0 - dot(normalMap.xy, normalMap.xy)));
        
        // === Unreal Engine 标准 TBN 计算（更稳定） ===
        // 使用屏幕空间导数，但加入更好的数值稳定性处理
        
        // 计算位置和 UV 的屏幕空间导数
        float3 dPos_dx = ddx_coarse(i.WorldPos);  // 使用粗粒度导数，更稳定
        float3 dPos_dy = ddy_coarse(i.WorldPos);
        float2 dUV_dx = ddx_coarse(i.UV);
        float2 dUV_dy = ddy_coarse(i.UV);
        
        // Unreal Engine 的 TBN 计算（处理数值不稳定）
        float3 T = dPos_dx * dUV_dy.y - dPos_dy * dUV_dx.y;
        float3 B = dPos_dy * dUV_dx.x - dPos_dx * dUV_dy.x;
        float invMax = rsqrt(max(dot(T, T), dot(B, B)));  // 快速归一化
        
        // 构建 TBN 矩阵（列向量形式）
        float3x3 TBN = float3x3(T * invMax, B * invMax, N);
        
        // 🔍 调试模式 8: 显示切线T
        if (debugMode == 8) {
            return float4(normalize(T) * 0.5 + 0.5, 1.0);
        }
        
        // 🔍 调试模式 9: 显示副切线B
        if (debugMode == 9) {
            return float4(normalize(B) * 0.5 + 0.5, 1.0);
        }
        
        // 将切线空间法线转换到世界空间
        N = normalize(mul(normalMap, TBN));
    }
    
    // 🔍 调试模式 2: 显示世界空间法线（查看法线贴图是否影响光照）
    if (debugMode == 2) {
        return float4(N * 0.5 + 0.5, 1.0);
    }
    
    // === ARM 贴图处理（从之前采样的结果中提取）===
    float ao = armSample.r;          // R 通道 = AO (环境光遮蔽)
    float roughness = armSample.g;   // G 通道 = Roughness (粗糙度)
    float metallic = armSample.b;    // B 通道 = Metallic (金属度)
    
    // Unity 标准做法：贴图值 × 常量值 = 最终值
    if (dot(armSample, armSample) > 0.001) {
        // 贴图存在：贴图值作为基础，材质常量作为乘数
        metallic *= g_Metallic;
        roughness *= g_Roughness;
        // AO 不需要乘数，直接使用贴图值
    } else {
        // 贴图不存在：直接使用材质常量值
        metallic = g_Metallic;
        roughness = g_Roughness;
        ao = 1.0; // 默认无遮蔽
    }
    
    roughness = max(roughness, 0.25); // 增加最小粗糙度，让墙面更哑光、不那么光滑
    
    float3 V = normalize(g_CameraPosition - i.WorldPos);
    
    // 基础反射率（F0）
    float3 F0 = float3(0.04, 0.04, 0.04);  // 非金属默认值
    F0 = lerp(F0, albedo, metallic);       // 金属使用 albedo 作为 F0
    
    // 反射方向（用于环境光采样）
    float3 R = reflect(-V, N);
    
    // 采样环境贴图
    float2 envUV = DirToEquirectUV(R);
    float3 envColor = g_EquirectMap.Sample(g_EquirectMap_sampler, envUV).rgb;
    
    // === 直接光照（方向光）===
    float3 Lo = float3(0.0, 0.0, 0.0);
    
    if (g_LightIntensity > 0.0) {
        float3 L = normalize(-g_LightDirection);  // 光源方向（指向光源）
        float3 H = normalize(V + L);              // 半程向量
        float3 radiance = g_LightColor * g_LightIntensity;
        
        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        
        float3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        float3 specular = numerator / max(denominator, 0.001);
        
        float3 kS = F;
        float3 kD = float3(1.0, 1.0, 1.0) - kS;
        kD *= 1.0 - metallic;  // 金属没有漫反射
        
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    // === 环境光照（IBL - Image Based Lighting）===
    // Unity/Unreal 标准流程
    float3 F = FresnelSchlick(max(dot(N, V), 0.0), F0);
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;
    
    // 漫反射环境光：Unity/Unreal标准做法
    // ⚠️ 标准IBL: 漫反射使用预计算的Irradiance Map（球谐函数SH或卷积贴图）
    // 在没有预计算贴图的情况下，使用均匀的环境光颜色（避免方向性采样导致颜色不一致）
    // 
    // Unity默认：Sky Light使用均匀的天空颜色作为环境光基础
    // Unreal默认：使用SH（Spherical Harmonics）计算的平均环境光
    float3 irradiance = float3(0.8, 0.8, 0.8);  // 中性灰白色环境光（模拟天空平均颜色）
    
    float3 diffuse = kD * albedo * irradiance;
    
    // 镜面反射环境光：使用反射方向采样
    float3 prefilteredColor = envColor;  
    
    // Unreal Engine 标准：环境镜面反射BRDF
    float3 specular = prefilteredColor * F;
    
    // 粗糙度衰减：粗糙材质几乎没有环境镜面反射
    float smoothness = 1.0 - roughness;
    specular *= smoothness * smoothness;
    
    // ⚠️ 关键修复：大幅降低环境镜面反射，避免灰色天空覆盖漫反射颜色
    // 非金属材质的环境镜面反射应该非常弱
    specular *= lerp(0.05, 0.3, metallic);  // 非金属5%，金属30%
    
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
