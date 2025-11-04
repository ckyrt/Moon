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

## AI 生成指导
这个模块需要实现：
1. 抽象的渲染接口
2. 多种渲染后端的适配
3. 相机和视口管理
4. 基础的材质系统