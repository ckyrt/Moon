# ADR 0005: 坐标系统与矩阵约定

## 状态 (Status)
✅ **已采纳** (Accepted) - 2025-11-06

## 背景 (Context)
在开始实现渲染系统时，我们需要确定整个引擎使用的坐标系统和矩阵布局约定。这个决策将影响：
- 所有3D数学运算
- 顶点数据的组织方式
- 着色器中的矩阵运算
- 不同渲染后端的兼容性
- 与第三方库（如物理引擎）的集成

不同的图形API和引擎使用不同的约定：
- **DirectX**：左手坐标系，行主序矩阵
- **OpenGL**：右手坐标系，列主序矩阵
- **Vulkan**：右手坐标系（但Y轴向下）
- **Unity**：左手坐标系
- **Unreal**：左手坐标系（但Z轴向上）

我们需要选择一个统一的约定，并在所有渲染后端中保持一致。

## 决策 (Decision)
我们决定采用以下约定：

### 1. 坐标系：**左手坐标系 (Left-Handed)**
```
+Y (Up) |  
        |  / +Z (Forward)
        | /
        |/_____ +X (Right)
```

**理由：**
- 符合DirectX和大多数游戏引擎的习惯
- +Z 向前在游戏开发中更直观
- 与 Diligent Engine 的默认约定一致
- 便于与 Unity、Unreal 等主流引擎对比学习

### 2. 矩阵布局：**CPU 使用行主序 (Row-Major)，上传 GPU 前转置**
```cpp
// CPU 端构建行主序矩阵
Matrix4x4 world = ...;
Matrix4x4 view = ...;
Matrix4x4 proj = ...;
Matrix4x4 wvp = world * view * proj;

// 转置后上传给着色器
Matrix4x4 wvpTransposed = transpose(wvp);
SetShaderConstant("g_WorldViewProj", wvpTransposed);
```

**理由：**
- C++ 中行主序矩阵的内存布局更自然
- 矩阵数学运算更符合线性代数习惯
- 便于调试和可读性
- 只需在上传时转置一次

### 3. 着色器约定：**HLSL 使用 `mul(vector, matrix)`**
```hlsl
cbuffer Constants
{
    float4x4 g_WorldViewProj;  // 已转置
};

VSOutput main(VSInput input)
{
    // 向量在左，矩阵在右
    output.Position = mul(float4(input.Position, 1.0), g_WorldViewProj);
    return output;
}
```

**理由：**
- 与 DirectX/HLSL 的标准约定一致
- CPU 转置后，GPU 端可以直接使用
- 避免在着色器中进行转置操作

### 4. 标准矩阵函数（左手系）
所有矩阵构建函数使用左手坐标系：

```cpp
// 透视投影 - 左手系，Z 正向前
Matrix4x4 PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ);

// 视图矩阵 - 左手系，Forward = target - eye
Matrix4x4 LookAtLH(Vector3 eye, Vector3 target, Vector3 up);

// 旋转矩阵 - 左手系
Matrix4x4 RotationYLH(float angle);  // 绕Y轴旋转（符号与右手系相反）
```

## 后果 (Consequences)

### 正面影响 ✅
- **统一性**：整个引擎使用一致的坐标系统
- **可预测性**：所有数学运算结果符合预期
- **兼容性**：与 DirectX、Diligent Engine、Unity 等保持一致
- **可维护性**：团队成员使用相同的心智模型
- **可扩展性**：容易添加新的渲染后端

### 负面影响 ⚠️
- **OpenGL 集成**：需要额外处理右手系到左手系的转换
- **学习成本**：来自 OpenGL 背景的开发者需要适应
- **第三方库**：某些使用右手系的库需要坐标转换

### 技术债务 🔧
- 需要在文档中明确说明坐标系约定
- 需要提供坐标系转换工具函数
- OpenGL 后端需要特殊处理（矩阵转换）

## 实施指南 (Implementation Guidelines)

### 1. 数学库实现
```cpp
// Math/Matrix4x4.h
class Matrix4x4 {
public:
    // 左手坐标系专用函数
    static Matrix4x4 PerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ);
    static Matrix4x4 LookAtLH(const Vector3& eye, const Vector3& target, const Vector3& up);
    static Matrix4x4 RotationYLH(float angle);
    
    // 转置函数（用于上传GPU）
    Matrix4x4 Transpose() const;
    
private:
    // 行主序存储
    float m[4][4];  // m[row][col]
};
```

### 2. 渲染器实现
```cpp
// IRenderer.h
struct RenderConstants {
    Matrix4x4 worldViewProj;  // 注意：这里存储的是转置后的矩阵
};

// Renderer.cpp
void Renderer::SetTransform(const Matrix4x4& world, const Matrix4x4& view, const Matrix4x4& proj) {
    Matrix4x4 wvp = world * view * proj;
    Matrix4x4 wvpT = wvp.Transpose();  // 转置后上传
    UpdateConstantBuffer(&wvpT);
}
```

### 3. 着色器模板
```hlsl
// Shaders/Common.hlsl
cbuffer PerObjectConstants : register(b0)
{
    float4x4 g_WorldViewProj;  // CPU 已转置
};

// 所有顶点着色器使用相同模式
VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.Position = mul(float4(input.Position, 1.0), g_WorldViewProj);
    return output;
}
```

### 4. 单元测试
```cpp
// Tests/Math/MatrixTests.cpp
TEST(Matrix4x4, LookAtLH_ForwardIsPositiveZ) {
    Vector3 eye(0, 0, 0);
    Vector3 target(0, 0, 1);  // 正Z方向
    Vector3 up(0, 1, 0);
    
    Matrix4x4 view = Matrix4x4::LookAtLH(eye, target, up);
    // 验证 forward 向量是 +Z
    EXPECT_NEAR(view.GetForward().z, 1.0f, 0.001f);
}
```

## 参考资料 (References)
- [DirectX Coordinate System](https://docs.microsoft.com/en-us/windows/win32/direct3d9/coordinate-systems)
- [Diligent Engine Tutorial 01 - Hello Triangle](https://github.com/DiligentGraphics/DiligentSamples)
- [Real-Time Rendering, 4th Edition - Chapter 4: Transforms](http://www.realtimerendering.com/)
- [3D Math Primer for Graphics and Game Development](https://gamemath.com/)

## 相关决策 (Related Decisions)
- 待定：物理引擎坐标系集成 (Jolt/Bullet 使用右手系)
- 待定：资产导入工具的坐标系转换
- 待定：编辑器UI中的坐标显示约定

## 修订历史 (Revision History)
- 2025-11-06: 初始版本，确定左手坐标系和矩阵约定
