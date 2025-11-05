# Render Module

## 职责 (Responsibilities)
- 渲染接口抽象 (IRenderer)
- 多渲染后端支持 (bgfx/DirectX/OpenGL/Vulkan)
- Viewport 渲染与多相机支持
- 材质和着色器管理
- 渲染管线和批处理

## 接口文件 (Interface Files)
- `IRenderer.h` - 渲染器接口
- `RenderCommon.h` - 通用渲染类型
- `BgfxRenderer.h` - Bgfx实现
- `NullRenderer.h` - 空渲染器 (用于服务器)

## 依赖 (Dependencies)
- 渲染后端库 (bgfx, 或原生图形API)
- engine/core (基础系统)

## 坐标系统与矩阵约定 (Coordinate System & Matrix Conventions)

### ✅ 统一规则（已确定 2025-11-06）

**1. 坐标系：左手坐标系 (Left-Handed)**
```
+Y (Up) |  
        |  / +Z (Forward)
        | /
        |/_____ +X (Right)
```

**2. 矩阵布局：CPU 用 Row-Major，上传 GPU 前转置**
```cpp
// CPU 构建 row-major 矩阵
Matrix4x4 wvp = world * view * proj;

// 转置后上传给 HLSL
Matrix4x4 wvpT = transpose(wvp);
```

**3. HLSL 着色器：使用 `mul(vector, matrix)`**
```hlsl
VSOut.Pos = mul(float4(VSIn.Pos, 1.0), g_WorldViewProj);
```

**4. 标准矩阵函数（左手系）**
- `Perspective()` - 左手系透视投影，Z 正向前
- `LookAt()` - 左手系视图矩阵，Forward = target - eye
- `RotationY()` - 左手系旋转，符号与右手系相反

**理由：** 与 DirectX/Diligent Engine 标准一致，避免坐标系混乱。

**⚠️ 所有新代码必须遵循此规则！**

---

## AI 生成指导
这个模块需要实现：
1. 抽象的渲染接口
2. 多种渲染后端的适配
3. 相机和视口管理
4. 基础的材质系统