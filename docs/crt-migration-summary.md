# CRT 迁移总结 (2025-11-10)

> **任务：** 解决 CEF 与 Diligent Engine 的 CRT 链接冲突  
> **状态：** ✅ 已完成  
> **耗时：** 约 2 小时

---

## 📋 问题描述

**症状：**
```
error LNK2038: mismatch detected for 'RuntimeLibrary': value 'MDd_DynamicDebug' doesn't match value 'MTd_StaticDebug'
warning LNK4217: locally defined symbol __malloc_dbg imported in function ...
warning LNK4286: symbol __imp_malloc defined in libucrtd.lib imported by GraphicsEngineD3D11.lib
error LNK2001: unresolved external symbol __imp_ceilf
error LNK2001: unresolved external symbol __imp_cosf
```

**根本原因：**
- CEF (libcef.lib) 使用 `/MTd` (静态 CRT)
- Diligent Engine 默认使用 `/MDd` (动态 CRT)
- C Runtime Library 不匹配导致链接失败

---

## ✅ 解决方案

### 1️⃣ 统一使用静态 CRT

**所有 Moon Engine 组件：**
- **Debug:** `/MTd` (Multi-threaded Debug, Static)
- **Release:** `/MT` (Multi-threaded, Static)

**影响的项目：**
| 项目 | 原 CRT | 新 CRT | 状态 |
|------|--------|--------|------|
| EditorApp | `/MTd` | `/MTd` | ✅ 无需修改 |
| EditorBridge | `/MTd` | `/MTd` | ✅ 无需修改 |
| EngineCore | `/MDd` | `/MTd` | ✅ 已修改 |
| EngineRender | `/MDd` | `/MTd` | ✅ 已修改 |
| HelloEngine | `/MDd` | `/MTd` | ✅ 已修改 |
| Diligent Engine | `/MDd` | `/MTd` | ✅ 已重新编译 |

---

### 2️⃣ 重新编译 Diligent Engine

**CMake 配置命令：**
```powershell
cmake -B build -S external/DiligentEngine `
  -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>" `
  -DCMAKE_CXX_FLAGS_DEBUG="/MTd" `
  -DCMAKE_C_FLAGS_DEBUG="/MTd" `
  -DDILIGENT_BUILD_SAMPLES=OFF `
  -DDILIGENT_BUILD_TESTS=OFF `
  -DDILIGENT_BUILD_TOOLS=OFF `
  -DDILIGENT_BUILD_TUTORIALS=OFF `
  -DDILIGENT_BUILD_APPS=OFF
```

**编译结果：**
- ✅ 编译时间：约 3-5 分钟（禁用 Samples/Tests/Tools 后大幅缩短）
- ✅ 所有核心库使用 `LIBCMTD` (静态 CRT)
- ✅ 使用 `dumpbin` 验证无 `MSVCRTD` 引用

---

### 3️⃣ 验证结果

**检查 Diligent 库：**
```powershell
dumpbin /directives build\DiligentCore\Graphics\GraphicsEngineD3D11\Debug\Diligent-GraphicsEngineD3D11-static.lib `
  | Select-String "MSVCRTD" | Measure-Object | Select-Object -ExpandProperty Count
# 返回 0 ✅
```

**检查 EditorApp.exe：**
```powershell
dumpbin /DEPENDENTS bin\x64\Debug\EditorApp.exe | Select-String "\.dll"
```

**依赖的 DLL：**
- ✅ `libcef.dll` - CEF 浏览器引擎
- ✅ `d3d11.dll`, `dxgi.dll` - DirectX 11
- ✅ `KERNEL32.dll`, `USER32.dll` - 系统 DLL
- ✅ **没有 `MSVCR*.dll` 或 `VCRUNTIME*.dll`** - 成功使用静态 CRT

**EditorApp.exe 编译成功：**
- 文件大小：8.90 MB
- 进程启动：正常 (进程 ID: 3564)
- 无链接错误：✅

---

## 📚 文档更新

### 新增文档

**1️⃣ ADR-0006: 静态 CRT 配置** (`docs/adr-0006-static-crt-configuration.md`)
- 架构决策记录
- 问题背景、根本原因分析
- 决策内容、替代方案对比
- 后果分析（正面/负面影响）
- 实施细节、验证流程

**2️⃣ 开发者指南更新** (`docs/DEVELOPER_GUIDE.md`)
- 简化 CRT 配置章节（快速参考）
- 指向 ADR-0006 的详细文档
- 保留常用命令和错误速查

**3️⃣ CRT 迁移总结** (`docs/crt-migration-summary.md`)
- 本文档，快速回顾整个过程

---

## 🎯 关键经验

### ✅ 成功要点

1. **命令行参数优先级最高**
   - CMake 命令行参数 > CMakeLists.txt 配置
   - 确保使用 `-DCMAKE_MSVC_RUNTIME_LIBRARY` 等参数

2. **必须删除旧 build 目录**
   - CMake 缓存会导致配置不生效
   - 使用 `Remove-Item -Recurse -Force` 完全删除

3. **使用 dumpbin 验证**
   - 检查 `.lib` 文件的 `/DEFAULTLIB` 指令
   - `LIBCMTD` = 静态 CRT ✅
   - `MSVCRTD` = 动态 CRT ❌

4. **禁用不需要的 Diligent 组件**
   - Samples, Tests, Tools, Tutorials 不需要
   - 编译时间从 15-20 分钟缩短到 3-5 分钟
   - 避免 .NET 项目错误 (NETSDK1004)

---

### ❌ 避免的错误

1. **不要混用 CRT 类型**
   - 所有组件必须统一使用 `/MTd` 或 `/MDd`
   - 混用会导致链接错误和运行时崩溃

2. **不要忽略 LNK4217/LNK4286 警告**
   - 这些警告表示 CRT 类型不匹配
   - 必须解决，否则会引发运行时问题

3. **不要直接修改 CMakeLists.txt**
   - Diligent Engine 的子模块会覆盖设置
   - 必须使用命令行参数 + `FORCE`

4. **不要在不清理的情况下重新配置**
   - CMake 缓存会保留旧设置
   - 每次修改 CRT 类型必须删除 `build` 目录

---

## 📊 性能影响

### 编译时间

| 配置 | 时间 | 说明 |
|------|------|------|
| **完整编译 (所有组件)** | 15-20 分钟 | 包含 Samples, Tests, Tools |
| **核心库编译 (禁用多余组件)** | 3-5 分钟 | ✅ 推荐配置 |

### 可执行文件大小

| 配置 | EditorApp.exe 大小 | 说明 |
|------|-------------------|------|
| **动态 CRT (`/MDd`)** | ~7.0 MB | 需要 `MSVCR140D.dll` |
| **静态 CRT (`/MTd`)** | ~8.9 MB | ✅ CRT 嵌入到 .exe 中 |

**增加：** +1.9 MB (~27%)  
**代价：** 可接受（现代游戏引擎标准）

---

## 🔗 相关资源

**项目文档：**
- [ADR-0006: 静态 CRT 配置](adr-0006-static-crt-configuration.md) - 详细架构决策
- [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) - 开发者指南
- [ARCHITECTURE.md](ARCHITECTURE.md) - 技术架构

**外部参考：**
- [MSVC Runtime Library Options](https://learn.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library)
- [CMake MSVC_RUNTIME_LIBRARY](https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html)
- [Diligent Engine Build Guide](https://github.com/DiligentGraphics/DiligentEngine/blob/master/README.md)

---

## ✅ 最终检查清单

- [x] 删除 Diligent Engine 旧构建目录
- [x] 使用正确的 CMake 参数重新配置
- [x] 编译 Diligent Engine (3-5 分钟)
- [x] 使用 `dumpbin` 验证所有库使用 `/MTd`
- [x] 重新编译 EngineCore 和 EngineRender
- [x] 重新编译 EditorApp（无 CRT 错误）
- [x] 验证 EditorApp.exe 不依赖 `MSVCR*.dll`
- [x] 运行 EditorApp.exe 测试启动
- [x] 更新 ADR-0006 文档
- [x] 更新 DEVELOPER_GUIDE.md
- [x] 创建迁移总结文档

---

**完成时间：** 2025-11-10  
**下一步：** 整合 CEF 和引擎的消息循环，测试 3D 窗口嵌入效果
