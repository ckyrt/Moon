# Moon Engine - 开发者指南

> **面向开发者的实用操作手册**  
> *编译方法、开发规范、AI 协作指南*

---

## 📝 文档说明

本文档为参与 Moon Engine 开发的程序员提供实用指南，包括：
- 编译和构建方法
- 开发规范和最佳实践
- AI 辅助开发规范
- 文件编码规范
- 配置精简原则

**相关文档：**
- [README.md](../README.md) - 产品愿景
- [ARCHITECTURE.md](ARCHITECTURE.md) - 技术架构
- [ROADMAP.md](ROADMAP.md) - 开发路线图

---

# 1️⃣ 编译指南

## ⚠️ MSBuild 路径问题（重要）

**必须使用 Visual Studio 的 MSBuild，而不是 Mono 的 MSBuild！**

### ❌ 错误的方式（会报错）

```powershell
msbuild Moon.sln  # ❌ 可能调用 Mono 的 MSBuild
# 错误：找不到导入的项目"E:\Microsoft.Cpp.Default.props"
```

### ✅ 正确的方式

```powershell
# 使用完整路径（推荐）
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Moon.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

---

## 编译参数说明

| 参数 | 说明 | 可选值 |
|------|------|--------|
| `/p:Configuration` | 编译配置 | `Debug` / `Release` |
| `/p:Platform` | 目标平台 | `x64` / `Win32` |
| `/m` | 多核并行编译 | - |
| `/v:minimal` | 输出详细程度 | `quiet` / `minimal` / `normal` / `detailed` / `diagnostic` |
| `/t:ProjectName` | 只编译指定项目 | 项目名称 |
| `/t:Clean` | 清理构建 | - |

---

## 常用编译命令

### 编译所有项目

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Moon.sln /p:Configuration=Debug /p:Platform=x64 /m
```

### 清理构建

```powershell
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Moon.sln /t:Clean /p:Configuration=Debug /p:Platform=x64
```

---

## 输出目录结构

```
bin/
└── x64/
    ├── Debug/              # Debug 版本输出
    │   ├── EditorApp.exe
    │   ├── EngineCore.lib
    │   ├── EngineRender.lib
    │   └── HelloEngine.exe
    └── Release/            # Release 版本输出
        ├── EditorApp.exe
        ├── EngineCore.lib
        ├── EngineRender.lib
        └── HelloEngine.exe

temp/                       # 中间文件目录
├── EditorApp/
│   └── x64/
│       ├── Debug/
│       └── Release/
├── EngineCore/
├── EngineRender/
└── HelloEngine/
```

**说明：**
- **可执行文件**：`bin\x64\{Debug|Release}\*.exe`
- **静态库**：`bin\x64\{Debug|Release}\*.lib`
- **中间文件**：`temp\{ProjectName}\x64\{Debug|Release}\`

---

# 2️⃣ 文件编码规范

## ✅ UTF-8 无 BOM 编码

**所有代码文件必须使用 UTF-8 无 BOM 编码格式：**

| 文件类型 | 编码格式 |
|---------|---------|
| `.cpp`, `.h`, `.hpp` | **UTF-8 无 BOM** |
| `.md`, `.json`, `.xml` | **UTF-8 无 BOM** |

**禁止使用：**
- ❌ UTF-8 with BOM
- ❌ GBK / GB2312
- ❌ 其他编码格式

---

## 为什么使用 UTF-8 无 BOM？

**理由：**
- ✅ **避免编译器警告或错误** - BOM 可能导致某些编译器错误
- ✅ **跨平台兼容性更好** - Linux/macOS 默认不使用 BOM
- ✅ **Git diff 更清晰** - BOM 会在 diff 中显示为乱码
- ✅ **符合现代标准** - UTF-8 无 BOM 是现代开发的标准做法

**常见问题：**
- ❌ BOM 导致 C++ 编译器无法识别文件开头
- ❌ Git 将 BOM 标记为文件更改
- ❌ 某些工具链不支持 BOM

---

# 3️⃣ AI 辅助开发规范

为了防止 AI 修改代码时造成反复返工，必须遵守以下规范：

## ✅ 3.1 接口确定后禁止 AI 修改模块边界

**原则：**
- 模块之间的接口一旦确定，AI 不得随意修改
- 各模块职责明确，不得越界

**示例：**
- ❌ renderer 不能包含 scene graph
- ❌ geometry 不能调用 render
- ❌ physics 不得操作 UI

**执行方式：**
- 在提示词中明确说明模块边界
- 定期检查模块依赖关系
- 使用静态分析工具检测违规

---

## ✅ 3.2 头文件一旦确定，不允许 AI 修改函数签名

**原则：**
- 公开 API 的函数签名不得随意修改
- 除非人工明确允许

**理由：**
- 避免破坏现有代码
- 保持 API 稳定性
- 减少回归问题

**允许的修改：**
- 函数实现内部逻辑
- 私有函数
- 新增函数（不破坏现有接口）

---

## ✅ 3.3 所有复杂算法必须放到独立文件

**原则：**
- 复杂算法独立成文件
- 避免 AI 修改无关内容

**示例：**
- `CSGBoolean.cpp` - CSG 布尔运算
- `TerrainBrush.cpp` - 地形笔刷算法
- `VehicleSuspension.cpp` - 载具悬挂系统
- `MeshOptimizer.cpp` - 网格优化算法

**好处：**
- 降低文件复杂度
- AI 修改时范围可控
- 便于单元测试
- 便于代码审查

---

## ✅ 3.4 使用 PR（单模块）开发

**原则：**
- 不允许 AI 一次改所有文件
- 每个 PR 只关注一个模块或功能

**工作流程：**
1. 创建功能分支
2. AI 只修改相关模块
3. 运行测试
4. 代码审查
5. 合并到主分支

**好处：**
- 降低风险
- 便于回滚
- 便于审查
- 清晰的历史记录

---

## ✅ 3.5 版本库必须加文档

**原则：**
- 每个重要的架构决策都要记录
- 使用 ADR（Architecture Decision Record）格式

**文档结构：**
```
/docs/
  ├── adr-0001-*.md         # 架构决策记录
  ├── adr-0002-*.md
  ├── adr-0003-editor-ui-architecture.md
  ├── adr-0005-coordinate-system-and-matrix-conventions.md
  └── ...
```

**ADR 模板：**
```markdown
# ADR-XXXX: 决策标题

## 状态
提议 / 已接受 / 已废弃

## 背景
为什么需要做这个决策？

## 决策
我们决定做什么？

## 后果
这个决策的影响是什么？
```

---

# 4️⃣ 配置精简原则

详见：[playbook-minimalist-configuration.md](playbook-minimalist-configuration.md)

**核心原则：**
- ✅ 避免配置蔓延
- ✅ 约定优于配置
- ✅ 代码即配置
- ✅ 最小化 .vcxproj 文件

**实践：**
- 使用项目默认设置
- 减少自定义配置
- 使用属性表共享配置
- 避免硬编码路径

---

# 5️⃣ 代码规范

## 命名约定

**文件命名：**
- 头文件：`PascalCase.h`
- 源文件：`PascalCase.cpp`
- 示例：`EngineCore.h`, `EngineCore.cpp`

**类/结构体命名：**
- `PascalCase`
- 示例：`SceneNode`, `MeshRenderer`

**函数命名：**
- `camelCase`
- 示例：`updateScene()`, `renderMesh()`

**变量命名：**
- 成员变量：`m_camelCase`
- 局部变量：`camelCase`
- 示例：`m_entityList`, `deltaTime`

**常量/宏：**
- `UPPER_SNAKE_CASE`
- 示例：`MAX_ENTITIES`, `PI`

---

## 代码风格

**缩进：**
- 使用 4 个空格（不使用 Tab）

**大括号：**
```cpp
// ✅ 推荐
if (condition) {
    doSomething();
}

// ❌ 不推荐
if (condition)
{
    doSomething();
}
```

**注释：**
```cpp
// 单行注释使用 //

/**
 * 多行注释使用 /** */
 * 用于函数/类的文档注释
 */
```

---

## 头文件保护

```cpp
// ✅ 使用 #pragma once（推荐）
#pragma once

// ❌ 或使用传统方式
#ifndef ENGINE_CORE_H
#define ENGINE_CORE_H
// ...
#endif
```

**推荐使用 `#pragma once`：**
- 简洁明了
- 避免命名冲突
- 现代编译器都支持

---

# 6️⃣ 日志系统

使用引擎内置日志系统：

```cpp
#include "Logging/Logger.h"

// 不同级别的日志（需要指定模块名）
MOON_LOG_INFO("ModuleName", "Info message: %d", value);
MOON_LOG_WARN("ModuleName", "Warning message: %s", str);
MOON_LOG_ERROR("ModuleName", "Error message: %f", floatValue);

// 简化版本（使用默认模块名"General"）
MOON_LOG_INFO_SIMPLE("Simple info message");
MOON_LOG_WARN_SIMPLE("Simple warning");
MOON_LOG_ERROR_SIMPLE("Simple error");
```

**日志输出位置：**
- 控制台输出（带颜色）
- 文件：`{可执行文件目录}\logs\YYYY-MM-DD.log`
- 每天自动创建新的日志文件

**初始化和关闭：**
```cpp
// 初始化日志系统（在程序启动时）
Moon::Core::Logger::Init();

// 关闭日志系统（在程序退出时）
Moon::Core::Logger::Shutdown();
```

---

# 7️⃣ 性能分析

## Visual Studio Profiler

1. 调试 → 性能探查器
2. 选择分析类型：
   - CPU 使用率
   - 内存使用率
   - GPU 使用率
3. 开始分析
4. 查看报告

## 性能优化建议

**渲染优化：**
- 减少 Draw Call
- 使用实例化渲染
- 批处理相同材质的物体

**内存优化：**
- 对象池（Object Pooling）
- 智能指针管理
- 避免内存泄漏

**算法优化：**
- 使用合适的数据结构
- 空间分割（Octree、BVH）
- 多线程并行

---

# 8️⃣ 资源链接

## 官方文档
- [Visual Studio 文档](https://docs.microsoft.com/visualstudio)
- [MSBuild 参考](https://docs.microsoft.com/visualstudio/msbuild)
- [C++ 标准文档](https://en.cppreference.com/)

## 第三方库文档
- [DiligentEngine](https://github.com/DiligentGraphics/DiligentEngine)
- [CEF](https://bitbucket.org/chromiumembedded/cef)
- [React](https://react.dev/)

## 相关工具
- [Git](https://git-scm.com/)
- [Visual Studio Code](https://code.visualstudio.com/)
- [CMake](https://cmake.org/)

---

## 📚 相关文档

- **[README.md](../README.md)** - 产品愿景
- **[ARCHITECTURE.md](ARCHITECTURE.md)** - 技术架构
- **[ROADMAP.md](ROADMAP.md)** - 开发路线图
- **[playbook-minimalist-configuration.md](playbook-minimalist-configuration.md)** - 配置精简原则

---

**Moon Engine - 开发者指南** 🔧
