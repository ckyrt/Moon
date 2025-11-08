# Render Module

## 职责 (Responsibilities)
- 渲染接口抽象 (IRenderer)
- 多渲染后端支持 (Diligent Engine/DirectX 11)
- Viewport 渲染与多相机支持
- 材质和着色器管理
- 渲染管线和批处理
- GPU 资源管理与缓存

## 接口文件 (Interface Files)
- `IRenderer.h` - 渲染器接口
- `RenderCommon.h` - 通用渲染类型
- `BgfxRenderer.h` - Bgfx实现
- `NullRenderer.h` - 空渲染器 (用于服务器)

## 依赖 (Dependencies)
- 渲染后端库 (bgfx, 或原生图形API)
- engine/core (基础系统)

## 坐标系统与矩阵约定 (Coordinate System & Matrix Conventions)

> **📋 完整的坐标系统约定请参考：**  
> [ADR 0005: 坐标系统与矩阵约定](../../docs/adr/0005-coordinate-system-and-matrix-conventions.md)

**快速参考：**
- **坐标系**：左手坐标系 (+Y 上, +Z 前, +X 右)
- **矩阵布局**：CPU 使用行主序，上传 GPU 前转置
- **着色器约定**：HLSL 使用 `mul(vector, matrix)`
- **决策日期**：2025-11-06

---

## AI 开发指导 (AI Development Guidelines)

### 渲染器实现要点
1. **接口优先**：新功能先在 `IRenderer` 中定义抽象方法
2. **资源管理**：使用 RAII 模式管理 GPU 资源，确保 `Shutdown()` 中释放
3. **缓存策略**：避免每帧重复上传数据到 GPU
4. **矩阵约定**：CPU 行主序，GPU 列主序 (需转置)
5. **错误处理**：检查 Diligent Engine API 返回值，记录日志

### 添加新渲染器后端
1. 继承 `IRenderer` 接口
2. 实现所有纯虚函数
3. 遵循 Begin/End 帧模式
4. 实现资源缓存机制
5. 在 `NullRenderer` 中添加对应的空实现