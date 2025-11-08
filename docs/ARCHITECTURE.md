# Moon Engine - 技术架构文档

> **面向开发者的完整技术架构说明**  
> *模块设计、依赖关系、技术选型*

---

## 📝 文档说明

本文档包含 Moon Engine 的完整技术架构，包括：
- 系统架构设计
- 模块划分和职责
- 依赖关系矩阵
- 技术选型说明

**相关文档：**
- [README.md](../README.md) - 产品愿景和价值主张
- [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) - 开发者实操指南
- [ROADMAP.md](ROADMAP.md) - 开发路线图

---

# 1️⃣ 项目总体架构

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

> **架构决策**: 详见 [ADR 0003: 编辑器 UI 架构选择](adr-0003-editor-ui-architecture.md)

## 🏗️ 分层优势

- **UI 与引擎彻底解耦** - 引擎核心不依赖 CEF，可无 UI 运行（服务器模式）
- **引擎与渲染彻底解耦** - 支持 NullRenderer / Diligent / bgfx 等任意后端
- **可以无渲染运行** - 适用于服务器 authoritative logic
- **可支持任意渲染后端** - DiligentEngine / bgfx / Vulkan
- **可扩展到多人在线、脚本系统、物理系统等**

> **编辑器 UI 架构**: 使用 CEF + React，详见 [ADR 0003](adr-0003-editor-ui-architecture.md)

---

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

---

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

**关键文件：**
- `IEngine.h` - 引擎核心接口
- `EngineCore.h/.cpp` - 引擎实现

---

## ✅ 3.2 Renderer 渲染抽象层

**接口 IRenderer 提供：**
- 初始化
- resize
- beginFrame / endFrame
- drawMesh(mesh, material)
- drawGizmo
- 资源加载（纹理、mesh、shader）

**可用后端：**
- **NullRenderer** - 默认，无渲染输出，用于测试
- **BgfxRenderer** - bgfx 后端
- **DiligentRenderer** - DiligentEngine 后端（当前主要使用）
- **VulkanRenderer** - 未来支持

**特点：**
- ✅ 完全可替换
- ✅ 引擎不依赖底层图形 API

**关键文件：**
- `IRenderer.h` - 渲染器接口
- `DiligentRenderer.h/.cpp` - DiligentEngine 实现
- `NullRenderer.h/.cpp` - 空渲染器
- `RenderCommon.h` - 渲染公共定义

---

## ✅ 3.3 Geometry（CSG）几何核心

UGC 平台的关键模块，负责：
- **boolean**（union / subtract / intersect）
- **extrude**（拉伸体）
- **bevel**（倒角）
- **parametric modeling**（参数化建模）
- **mesh 生成与修复**
- **自动 UV**

**用于：**
- 建筑构件
- 地形开洞
- 窗户自动开洞

**技术选型：**
- 考虑使用 CGAL、OpenCascade 或自研

---

## ✅ 3.4 Terrain 地形系统

**功能：**
- 高度图（heightmap）
- 地形笔刷（提升、压低、平滑、噪声）
- 多层材质混合
- 分块地形（LOD 地形优化）
- 自动散布自然要素（树、草、石）

**技术要点：**
- 高度图存储和采样
- 实时笔刷修改
- 分块 LOD 管理
- 材质混合（splatmap）

---

## ✅ 3.5 Building 建筑系统

**参数化构件：**
- **Wall**（厚度、材质、是否自动开洞）
- **Window**（尺寸、样式、玻璃类型）
- **Door**（开合、铰链方向）
- **Stair**（直梯、旋转梯）
- **Roof**（坡度、材质）

**特点：**
- ✅ 所有构件可用 CSG 合并
- ✅ 用户可以用拖拽方式制作一栋别墅
- ✅ 完全可扩展的构件库

**实现方式：**
- 基于 Geometry 模块的 CSG 功能
- 参数化生成网格
- 自动开洞算法

---

## ✅ 3.6 Character 角色系统（参数化捏人）

不提供专业骨骼建模，而是：
- 预制人类 / 动物基础模型
- 身体参数（身高、手臂长、腿长、头大小）
- Morph Targets（脸型、表情）
- 捏人面板
- 服装系统（mesh 替换 + 骨骼蒙皮）

**技术要点：**
- 骨骼系统
- Morph Target / Blend Shape
- 服装绑定和蒙皮

---

## ✅ 3.7 Vehicle 载具系统

**支持：**
- 轮胎 / 转向 / 悬挂
- 发动机动力模型
- 载具控制（油门、刹车、方向盘）
- 多种载具类型（汽车、摩托、船、飞机、直升机）
- 外观参数化

**技术要点：**
- 基于物理引擎的载具物理
- 轮胎摩擦力模型
- 悬挂系统模拟

---

## ✅ 3.8 Physics 物理系统

**基于 Jolt/Bullet：**
- 刚体
- 触发器
- 车辆物理
- 角色胶囊碰撞
- 刚体/CSG 动态碰撞体生成
- 布料（未来）

**技术选型：**
- 优先考虑 Jolt Physics（现代、高性能）
- 备选 Bullet Physics（成熟、稳定）

---

## ✅ 3.9 Net 多人同步（未来）

**功能：**
- authoritative server
- client prediction
- state replication
- editing sync（多人协作编辑）
- 权限系统

**技术要点：**
- 客户端预测和服务器校正
- 状态同步策略
- 多人编辑冲突解决

---

## ✅ 3.10 Scripting 脚本系统（未来）

**功能：**
- Lua / JS 虚拟机
- 沙盒环境
- Visual Scripting（节点图）
- 事件系统（OnClick / OnEnter / Timer）
- 控制角色/载具/动画/物理

**技术选型：**
- Lua（轻量、成熟）
- JavaScript（V8引擎）
- 可视化脚本编辑器

---

## ✅ 3.11 Assets 持久化与资产系统

**功能：**
- 场景保存/加载
- 资产格式（模型、材质、贴图）
- 预制件（Prefab）
- 用户自定义资产库
- 在线分享（未来）

**技术要点：**
- JSON/二进制序列化
- 资产引用管理
- 资产打包和加载

---

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

- ✅ **EngineCore 不得依赖 CEF/UI**
- ✅ **引擎不得依赖 EditorBridge**
- ✅ **Render 不得依赖 Terrain/Building**
- ✅ **Physics 不得依赖 Render**

**原因：**
- 保持模块独立性和可测试性
- 引擎核心可以在无 UI 环境下运行（服务器模式）
- 渲染层可以随时替换
- 便于单元测试和模块复用

---

# 5️⃣ 开发阶段成功标准（AI 友好）

## ✅ 阶段 1：基础框架 + Null Renderer

**成功标准：**
- 能创建 Scene/Entity/Component
- WebUI 能发送 JSON → 引擎接收命令
- NullRenderer 输出 draw log
- HelloCube 示例运行

**验证方式：**
- 运行 HelloEngine 示例
- 查看日志输出
- 验证场景创建和组件添加

---

## ✅ 阶段 2：CSG 基础

**成功标准：**
- Box + Cylinder + Extrude
- Boolean：Union/Subtract
- 自动生成可渲染 Mesh
- WebUI 能创建/删除形状

**验证方式：**
- 创建基础形体并渲染
- 执行布尔运算并查看结果
- 通过 WebUI 操作验证

---

## ✅ 阶段 3：编辑器基本操作

**成功标准：**
- 选择（点击选中）
- 移动/旋转/缩放
- Undo/Redo
- 属性面板实时修改

**验证方式：**
- 在编辑器中选择物体
- 使用 Gizmo 操作物体
- 测试撤销/重做功能

---

## ✅ 阶段 4：地形系统

**成功标准：**
- Heightmap 生成
- 笔刷修改实时看到结果
- 多材质地表

**验证方式：**
- 创建地形
- 使用笔刷修改
- 应用多层材质

---

## ✅ 阶段 5：建筑系统

**成功标准：**
- 墙体绘制
- 自动开洞（门窗）
- 楼梯参数化生成
- 屋顶生成

**验证方式：**
- 绘制墙体
- 添加门窗并验证开洞
- 生成楼梯和屋顶

---

# 6️⃣ 技术选型说明

## 渲染引擎

**DiligentEngine**（当前主要选择）
- ✅ 现代图形 API 抽象层
- ✅ 支持 DirectX 11/12、Vulkan、OpenGL
- ✅ 跨平台（Windows、Linux、macOS）
- ✅ 活跃维护

**备选方案：**
- bgfx（更轻量）
- 自研渲染器

## 编辑器 UI

**CEF (Chromium Embedded Framework) + React**
- ✅ 强大的前端生态
- ✅ 快速开发和迭代
- ✅ 丰富的 UI 组件库
- ✅ 完善的调试工具

> 详见 [ADR 0003](adr-0003-editor-ui-architecture.md)

## 物理引擎

**Jolt Physics**（优先考虑）
- ✅ 现代、高性能
- ✅ 支持大规模场景
- ✅ 活跃开发

**Bullet Physics**（备选）
- ✅ 成熟稳定
- ✅ 广泛使用
- ✅ 丰富的功能

## CSG 几何

**待评估：**
- CGAL（强大但复杂）
- OpenCascade（工业级）
- 自研（可控性强）

---

# 7️⃣ 坐标系统和约定

详见：[ADR 0005: 坐标系统和矩阵约定](adr-0005-coordinate-system-and-matrix-conventions.md)

**关键约定：**
- 右手坐标系
- Y 轴向上
- 矩阵存储和变换顺序
- 旋转表示方法

---

# 8️⃣ 性能考虑

## 渲染性能

- 实例化渲染（Instancing）
- 遮挡剔除（Occlusion Culling）
- LOD（Level of Detail）
- 分块加载（Chunking）

## 物理性能

- 空间分割（BVH、Octree）
- 休眠机制（Sleeping）
- 连续碰撞检测（CCD）优化

## 网络性能

- 状态差异同步
- 预测和插值
- 带宽优化

---

# 9️⃣ 扩展性设计

## 插件系统（未来）

- 动态加载模块
- 热重载支持
- 版本兼容性管理

## 脚本绑定

- 核心 API 暴露
- 事件系统
- 沙盒安全

## 资源扩展

- 自定义资源类型
- 导入/导出管道
- 资源压缩和优化

---

# 🔟 相关文档

- **[README.md](../README.md)** - 产品愿景
- **[DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md)** - 开发指南
- **[ROADMAP.md](ROADMAP.md)** - 开发路线图
- **[ADR 文档](.)** - 架构决策记录

---

**Moon Engine - 技术架构文档** 🏗️
