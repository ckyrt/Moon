# Moon Game Engine - Modular UGC Engine

一个基于Windows的模块化游戏引擎，支持用户生成内容(UGC)和WebUI编辑器。

## 项目结构

```
Moon/
├── engine/              # 引擎核心
│   ├── core/           # 引擎核心模块
│   ├── render/         # 渲染模块
│   └── samples/        # 示例程序
├── editor/             # 编辑器
│   ├── bridge/         # 原生-Web桥接
│   └── webui/          # React Web界面
├── docs/               # 文档
└── tools/              # 工具
```

## 开发环境

- **开发工具**: Visual Studio 2022
- **语言**: C++20
- **平台**: Windows
- **前端**: React + TypeScript (编辑器界面)

## 如何开始

### 方法1: 直接运行示例
1. 打开Visual Studio 2022
2. 创建新项目或解决方案
3. 添加现有代码文件到项目中
4. 编译并运行 `engine/samples/hello_win32.cpp`

### 方法2: 创建完整解决方案
1. 在Visual Studio中创建新的空白解决方案
2. 添加以下项目：
   - **EngineCore** (静态库) - `engine/core/`
   - **EngineRender** (静态库) - `engine/render/`
   - **EditorBridge** (静态库) - `editor/bridge/`
   - **HelloEngine** (可执行文件) - `engine/samples/`

## 特性

- ✅ 引擎核心架构 (Initialize/Tick/Shutdown)
- ✅ 模块化渲染系统
- ✅ Win32窗口支持
- ✅ 空渲染器实现（无外部依赖）
- 🚧 WebUI编辑器（开发中）
- 🚧 IPC通信桥接（开发中）

## 编译说明

项目使用Visual Studio开发，无需外部构建工具。所有源文件已经过Windows编译验证。

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
