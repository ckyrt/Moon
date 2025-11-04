# Moon Game Engine - Modular Architecture Framework

🚀 **这是一个完整的项目架构骨架，专为AI代码生成优化**

## 📋 项目概述

这个仓库包含一个**模块化游戏引擎**的完整架构框架，采用 Contract-First 设计理念。每个模块都有清晰的职责边界和接口定义，UI (WebUI) 与引擎逻辑完全分离。

## 🎯 设计目标

- ✅ **Contract-First 架构** - 接口先行，实现后续
- ✅ **UI与引擎完全分离** - WebUI通过IPC与引擎通信
- ✅ **确定性引擎核心** - 可无头运行 (适用于服务器)
- ✅ **稳定的渲染接口** - 支持多种渲染后端
- ✅ **独立可测试模块** - 几何/建筑/地形/车辆模块相互独立
- ✅ **标准化IPC接口** - 所有通信协议在 `/engine/contracts` 定义

## 📁 目录结构 (Monorepo)

```
/engine
  /core          # ECS、组件注册、事件系统、整体Game Loop
  /geometry      # CSG、Extrude、Sweep、Mesh生成、简化
  /physics       # 物理适配层 (Jolt/PhysX)、刚体、碰撞体、射线查询
  /render        # 渲染接口 (IRenderer)、bgfx/diligent实现、多相机支持
  /terrain       # 高度图、道路、植被、河湖海 (逻辑层)
  /building      # 墙、门窗、楼梯、地板的参数化生成
  /vehicle       # 轮式车辆、悬挂、油门转向
  /character     # 动画播放、骨骼、捏人参数系统
  /persistence   # 场景保存、加载、格式版本迁移
  /net           # Server/Client同步、CRDT/指令队列、快照
  /scripting     # 脚本语言集成 (Lua/JavaScript/Python)
  /contracts     # IDL类型定义 (Protobuf/FlatBuffers)、IPC协议
  /adapters      # 文件系统、OS、时间、平台API、CEF/WebView2 host
  /samples       # 最小可运行示例 (Cube、CSG、地形、载具)
  /tests         # 单元测试和集成测试

/editor
  /webui         # React/Vue，UI布局、Inspector、Hierarchy、菜单栏
  /bridge        # C++ ↔ WebUI通信层、WebSocket/CEF/WebView2

/tools           # 构建工具、资产处理工具

/docs
  /adr           # 架构决策记录 (Architecture Decision Records)
  /spec          # 技术规范和API文档
  /playbooks     # 开发流程和最佳实践
```

## 🚀 快速开始 (AI代码生成)

### 对AI说：

```
"根据已有项目架构，生成对应的文件内容、CMake、Hello World 实现。"
```

AI 会自动生成：
- ✅ 所有接口的初始实现
- ✅ 完整的CMakeLists.txt
- ✅ 平台适配层
- ✅ WebUI/bridge WebSocket主机
- ✅ Hello Triangle/Cube示例

### 或者分步骤：

1. **生成核心模块：**
   ```
   "实现 /engine/core 的ECS系统和游戏循环"
   ```

2. **生成渲染系统：**
   ```
   "实现 /engine/render 的IRenderer接口和NullRenderer"
   ```

3. **生成IPC通信：**
   ```
   "实现 /engine/contracts 的IPC协议和 /editor/bridge 的WebSocket通信"
   ```

## 🏗️ 技术栈

### 引擎核心
- **语言**: C++20
- **构建**: CMake
- **架构**: ECS (Entity Component System)
- **物理**: Jolt Physics (推荐) 或 PhysX
- **渲染**: bgfx (跨平台) 或 原生图形API

### 编辑器界面
- **前端**: React 或 Vue.js
- **通信**: WebSocket + Protobuf/JSON
- **宿主**: CEF 或 WebView2

### 开发工具
- **版本控制**: Git
- **文档**: Markdown + ADR
- **测试**: 单元测试 + 集成测试

## 📖 文档结构

- **[架构决策记录](/docs/adr/)** - 重要技术决策的记录
- **[技术规范](/docs/spec/)** - API和协议详细说明
- **[开发手册](/docs/playbooks/)** - 流程和最佳实践

## 🎮 示例程序

每个模块都包含最小化的示例程序：

- `hello_cube` - 基础渲染和相机控制
- `csg_demo` - CSG布尔运算演示
- `terrain_demo` - 地形生成和渲染
- `vehicle_demo` - 车辆物理和控制
- `building_demo` - 建筑系统演示

## 🔧 开发环境

### Windows (推荐)
- Visual Studio 2022
- CMake 3.24+
- Node.js 18+ (编辑器WebUI)

### Linux/macOS
- GCC 11+ 或 Clang 14+
- CMake 3.24+
- Node.js 18+ (编辑器WebUI)

## 🤖 AI 生成指导

这个框架专为AI代码生成设计：

1. **每个模块都有明确的README** - 说明职责和依赖关系
2. **接口先行设计** - 定义清晰的API边界
3. **示例驱动** - 每个功能都有对应的示例程序
4. **测试覆盖** - 每个模块都有对应的测试结构

## 📄 许可证

[MIT License](LICENSE) - 自由使用和修改

---

**💡 提示**: 这是一个架构框架，等待AI或开发者填充具体实现。每个模块的README文件包含详细的实现指导。

## Notes
- `/engine/render` contains a **NullRenderer** (default) and a **BgfxRenderer** stub.
  - NullRenderer requires no dependencies and is used by the sample.
  - BgfxRenderer is a placeholder for future integration (AI/you can wire bgfx later).
- The WebUI (`/editor/webui`) is a basic React/Vite skeleton; start it separately with `npm install && npm run dev`.

## Layout
```
/engine
  /core           # EngineCore (Initialize/Tick/Shutdown)
  /render         # IRenderer + NullRenderer (default) + bgfx stub
  /samples        # hello_engine (Win32 window + render loop)
/editor
  /webui          # React skeleton (Blueprint-like layout)
  /bridge         # IPC stubs
/docs             # ADR/spec placeholders
```
