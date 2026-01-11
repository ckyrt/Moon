// ============================================================================
// PBR Triplanar Mapping Functions
// Triplanar纹理采样（用于CSG/程序化几何）
// ============================================================================

#ifndef PBR_TRIPLANAR_HLSL
#define PBR_TRIPLANAR_HLSL

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

#endif // PBR_TRIPLANAR_HLSL
