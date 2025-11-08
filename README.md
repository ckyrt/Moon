# UGC 虚拟世界引擎与编辑器 (Moon Engine)

> **超级 README（最终版）**  
> *适用于项目根目录 README.md*

User-Generated World Engine & Editor

让任何人都能轻松创造一个 3D 世界。
像 Roblox + GTA + Minecraft + SketchUp + SimCity 的结合体。
你可以使用此引擎创建：建筑、地形、城市、角色、载具、脚本化交互，甚至是多人开放世界。

## 📋 快速导航

- 📖 [开发路线图](docs/ROADMAP.md) - 开发计划、当前进度、快速参考
- 📐 [架构决策记录 (ADR)](docs/adr/README.md) - 重要技术决策
- 🔧 [开发手册 (Playbooks)](docs/playbooks/README.md) - 开发流程和最佳实践
- 🔨 **[编译指南](#8️⃣-编译指南重要)** - MSBuild 正确使用方法（AI 必读）
- ⚙️ **[配置精简原则](docs/playbooks/MINIMALIST_CONFIGURATION.md)** - 最小化配置原则（AI 必读）

## 🎯 核心目标

- ✅ **低门槛建造**：小白用户无需学习建模软件即可搭建建筑 / 地形 / 场景
- ✅ **高扩展性**：支持高级用户进行 CSG 建模、地形编辑、参数化系统
- ✅ **模块化引擎**：渲染、物理、几何、网络全部可替换
- ✅ **多人协作**：未来支持多人实时编辑与在线世界
- ✅ **UGC 平台**：所有可被用户修改、组合、分享

# 1️⃣ 项目总体架构 Overview

```
┌─────────────────────────────────────────────────────┐
│          C++ 主程序 (EditorApp.exe)                  │
│  ┌───────────────────────────────────────────────┐  │
│  │  原生引擎核心 (EngineCore - C++)              │  │
│  │  - SceneGraph / Entity / Component            │  │
│  │  - CSG 几何系统                               │  │
│  │  - 建筑系统、地形系统、资产系统                 │  │
│  │  - 物理、网络、脚本等模块                      │  │
│  │                                               │  │
│  │  ┌─────────────────────────────────────────┐ │  │
│  │  │   DiligentRenderer (原生渲染)           │ │  │
│  │  │   - DirectX 12 / Vulkan                 │ │  │
│  │  │   - 渲染到原生 HWND 窗口                │ │  │
│  │  │   - 60+ FPS 性能                        │ │  │
│  │  └─────────────────────────────────────────┘ │  │
│  │                                               │  │
│  │  ┌─────────────────────────────────────────┐ │  │
│  │  │   CEF UI 子系统                         │ │  │
│  │  │   ┌───────────────────────────────────┐ │ │  │
│  │  │   │  React + TypeScript (WebUI)       │ │ │  │
│  │  │   │  - 场景树 (Hierarchy)             │ │ │  │
│  │  │   │  - 属性面板 (Inspector)           │ │ │  │
│  │  │   │  - 工具栏 (Toolbar)               │ │ │  │
│  │  │   │  - 资产浏览器                     │ │ │  │
│  │  │   └───────────────────────────────────┘ │ │  │
│  │  │   JavaScript ↔ C++ Binding (CEF V8)    │ │  │
│  │  └─────────────────────────────────────────┘ │  │
│  └───────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
```

> **架构决策**: 详见 [ADR 0003: 编辑器 UI 架构选择](docs/adr/0003-editor-ui-architecture.md)

## 🏗️ 分层优势

- **UI 与引擎彻底解耦** - 引擎核心不依赖 CEF，可无 UI 运行（服务器模式）
- **引擎与渲染彻底解耦** - 支持 NullRenderer / Diligent / bgfx 等任意后端

可以无渲染运行 → 适用于服务器 authoritative logic

可支持任意渲染后端

可扩展到多人在线、脚本系统、物理系统等

> **编辑器 UI 架构**: 使用 CEF + React，详见 [ADR 0003](docs/adr/0003-editor-ui-architecture.md)

---

# 2. 项目目录结构
# 2️⃣ 目录结构

```
/engine/                  # 引擎核心模块
  /core/                 # 🔧 ECS系统、场景图、序列化
  /geometry/             # 📐 CSG几何、布尔运算、网格生成
  /render/               # 🖼️ 渲染抽象层（支持多后端）
  /terrain/              # 🏔️ 地形系统、高度图、笔刷
  /building/             # 🏠 建筑系统、参数化构件
  /character/            # 👤 角色系统、参数化捏人
  /vehicle/              # 🚗 载具系统、物理模拟
  /physics/              # ⚡ 物理引擎集成
  /persistence/          # 💾 序列化、资产管理
  /net/                  # 🌐 网络同步、多人协作
  /scripting/            # 📜 脚本系统、行为编程
  /contracts/            # 📋 模块间接口定义
  /adapters/             # 🔌 第三方库适配器
  /samples/              # 📚 示例项目
  /tests/                # 🧪 测试框架

/editor/                  # 编辑器
  /app/                  # 🎮 编辑器主程序 (EditorApp.exe)
  /bridge/               # 🌉 C++ ↔ WebUI 通信桥
  /webui/                # 🖥️ React编辑器界面

/tools/                   # 🛠️ 构建工具、资产流水线
/docs/                    # 📖 文档、架构决策记录
```

# 3️⃣ 系统模块说明（完整版）

## ✅ 3.1 Core Engine 核心引擎

**包含：**
- 引擎生命周期（Init/Update/Shutdown）
- Scene Graph（节点）
- Entity / Component 系统
- 组件注册表
- 系统管理器（SystemManager）
- Undo/Redo 栈
- 序列化（JSON / 二进制）
- EventBus / 消息系统
- Time / File / Platform 抽象层

**特点：**
- ✅ 不依赖渲染
- ✅ 可在服务器无界面运行
- ✅ 所有模块的基础

## ✅ 3.2 Renderer 渲染抽象层

**接口 IRenderer 提供：**
- 初始化

- resize
- beginFrame / endFrame
- drawMesh(mesh, material)
- drawGizmo
- 资源加载（纹理、mesh、shader）

**可用后端：**
- NullRenderer（默认）
- BgfxRenderer
- DiligentRenderer
- VulkanRenderer（未来）

**特点：**
- ✅ 完全可替换
- ✅ 引擎不依赖底层图形 API

## ✅ 3.3 Geometry（CSG）几何核心

UGC 平台的关键模块，负责：
- boolean（union / subtract / intersect）
- extrude（拉伸体）
- bevel（倒角）
- parametric modeling（参数化建模）
- mesh 生成与修复
- 自动 UV

**用于：**
- 建筑构件
- 地形开洞
- 窗户自动开洞

## ✅ 3.4 Terrain 地形系统

- 高度图（heightmap）
- 地形笔刷（提升、压低、平滑、噪声）
- 多层材质混合

- 分块地形（LOD 地形优化）
- 自动散布自然要素（树、草、石）

## ✅ 3.5 Building 建筑系统

**参数化构件：**
- Wall（厚度、材质、是否自动开洞）
- Window（尺寸、样式、玻璃类型）
- Door（开合、铰链方向）
- Stair（直梯、旋转梯）
- Roof（坡度、材质）

**特点：**
- ✅ 所有构件可用 CSG 合并
- ✅ 用户可以用拖拽方式制作一栋别墅
- ✅ 完全可扩展的构件库

## ✅ 3.6 Character 角色系统（参数化捏人）

不提供专业骨骼建模，而是：
- 预制人类 / 动物
- 身体参数（身高、手臂长、腿长、头大小）
- Morph Targets（脸型、表情）
- 捏人面板
- 服装系统（mesh 替换 + 骨骼蒙皮）

## ✅ 3.7 Vehicle 载具系统

**支持：**
- 轮胎 / 转向 / 悬挂
- 发动机动力模型
- 载具控制（油门、刹车、方向盘）
- 多种载具类型（汽车、摩托、船、飞机、直升机）
- 外观参数化

## ✅ 3.8 Physics 物理系统

**基于 Jolt/Bullet：**
- 刚体
- 触发器
- 车辆物理
- 角色胶囊碰撞
- 刚体/CSG 动态碰撞体生成
- 布料（未来）

## ✅ 3.9 Net 多人同步（未来）

- authoritative server
- client prediction
- state replication
- editing sync（多人协作编辑）
- 权限系统

## ✅ 3.10 Scripting 脚本系统（未来）

- Lua / JS 虚拟机
- 沙盒环境
- Visual Scripting（节点图）
- 事件系统（OnClick / OnEnter / Timer）
- 控制角色/载具/动画/物理

## ✅ 3.11 Assets 持久化与资产系统

- 场景保存/加载
- 资产格式（模型、材质、贴图）
- 预制件（Prefab）
- 用户自定义资产库
- 在线分享（未来）

# 4️⃣ 模块依赖矩阵（防止 AI 越界修改）

```
core:           无依赖
geometry:       core
terrain:        core, geometry
building:       core, geometry
character:      core
vehicle:        core, physics
physics:        core
render:         core
persistence:    core
net:            core
scripting:      core, contracts
contracts:      core
adapters:       core
editor-bridge:  core, CEF SDK（仅编辑器构建）
editor-webui:   无依赖（纯 React）
```

## ⚠️ 重要约束

- ✅ EngineCore 不得依赖 CEF/UI
- ✅ 引擎不得依赖 EditorBridge
- ✅ Render 不得依赖 Terrain/Building
- ✅ Physics 不得依赖 Render

# 5️⃣ 开发阶段成功标准（AI 友好）

## ✅ 阶段 1：基础框架 + Null Renderer

**成功标准：**
- 能创建 Scene/Entity/Component
- WebUI 能发送 JSON → 引擎接收命令
- NullRenderer 输出 draw log
- HelloCube 示例运行

## ✅ 阶段 2：CSG 基础

**成功标准：**
- Box + Cylinder + Extrude
- Boolean：Union/Subtract
- 自动生成可渲染 Mesh
- WebUI 能创建/删除形状

## ✅ 阶段 3：编辑器基本操作

**成功标准：**
- 选择（点击选中）
- 移动/旋转/缩放
- Undo/Redo
- 属性面板实时修改

## ✅ 阶段 4：地形系统

**成功标准：**
- Heightmap 生成
- 笔刷修改实时看到结果
- 多材质地表

## ✅ 阶段 5：建筑系统

**成功标准：**
- 墙体绘制
- 自动开洞（门窗）
- 楼梯参数化生成
- 屋顶生成

# 6️⃣ AI 辅助开发规范（非常关键）

**为了防止反复返工，你必须遵守：**

## ✅ 6.0 文件编码规范

**所有代码文件必须使用 UTF-8 无 BOM 编码格式：**
- `.cpp`、`.h`、`.hpp` 文件 → **UTF-8 无 BOM**
- `.md`、`.json`、`.xml` 文件 → **UTF-8 无 BOM**
- 禁止使用 UTF-8 with BOM 或其他编码格式
- Visual Studio 设置：文件 → 高级保存选项 → 编码选择 "Unicode (UTF-8 无签名)"

**理由：**
- 避免 BOM 导致编译器警告或错误
- 跨平台兼容性更好
- Git diff 更清晰

## ✅ 6.1 接口确定后禁止 AI 修改模块边界

**例如：**
- renderer 不能包含 scene graph
- geometry 不能调用 render
- physics 不得操作 UI

## ✅ 6.2 头文件一旦确定，不允许 AI 修改函数签名
（除非你人工允许）

## ✅ 6.3 所有复杂算法必须放到独立文件

**比如：**
- CSGBoolean.cpp
- TerrainBrush.cpp
- VehicleSuspension.cpp

避免 AI 修改无关内容。

## ✅ 6.4 所有模块必须有测试
防止 AI 修改一处导致全盘崩。

## ✅ 6.5 使用 PR（单模块）开发
不允许 AI 改所有文件。

## ✅ 6.6 版本库必须加文档：
- `/docs/adr/*.md` - 记录每个架构决策原因。

# 7️⃣ 开发路线图（建议）

- ✅ **阶段 0**：搭建工程（CMake + 基础目录）
- ✅ **阶段 1**：Core + Null Renderer
- ✅ **阶段 2**：CSG 几何系统
- ✅ **阶段 3**：WebUI + 引擎交互
- ✅ **阶段 4**：编辑器工具（选择、Gizmo、Undo）
- ✅ **阶段 5**：地形系统
- ✅ **阶段 6**：建筑系统
- ✅ **阶段 7**：参数化角色
- ✅ **阶段 8**：载具系统
- ✅ **阶段 9**：物理系统
- ✅ **阶段 10**：多人同步
- ✅ **阶段 11**：脚本系统
- ✅ **阶段 12**：资产系统

---

# 8️⃣ 编译指南（重要）

## ⚠️ MSBuild 路径问题（AI 必读）

**必须使用 Visual Studio 的 MSBuild，而不是 Mono 的 MSBuild！**

### 错误的方式（会报错）
```powershell
msbuild Moon.sln  # ❌ 可能调用 Mono 的 MSBuild
# 错误：找不到导入的项目"E:\Microsoft.Cpp.Default.props"
```

### ✅ 正确的方式
```powershell
# 使用完整路径（推荐）
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Moon.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

### 编译参数说明
- `/p:Configuration=Debug` - Debug 或 Release
- `/p:Platform=x64` - 64位平台
- `/m` - 多核并行编译
- `/v:minimal` - 最小化输出
- `/t:ProjectName` - 只编译指定项目（可选）

### 常用编译命令
```powershell
# 编译所有项目
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Moon.sln /p:Configuration=Debug /p:Platform=x64 /m

# 只编译编辑器
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Moon.sln /p:Configuration=Debug /p:Platform=x64 /t:EditorApp /m

# 只编译引擎示例
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Moon.sln /p:Configuration=Debug /p:Platform=x64 /t:HelloEngine /m

# 清理构建
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Moon.sln /t:Clean /p:Configuration=Debug /p:Platform=x64
```

### 输出目录
- 可执行文件：`bin\x64\{Debug|Release}\*.exe`
- 静态库：`bin\x64\{Debug|Release}\*.lib`
- 中间文件：`temp\{ProjectName}\x64\{Debug|Release}\`

---

## 🎯 **总结**

这是一个完整的 UGC 虚拟世界引擎架构文档。所有模块都已规划完毕，具备清晰的边界和依赖关系。现在可以开始逐步实现每个模块，建议按照路线图顺序进行开发。

**记住：保持模块间的解耦，确保每个模块都可以独立开发和测试！** 🚀