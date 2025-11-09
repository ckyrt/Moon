# ADR-0006: 使用静态 C Runtime Library (CRT) 配置

## 状态

**已接受** (2025-11-10)

---

## 背景

### 问题描述

在集成 CEF (Chromium Embedded Framework) 和 Diligent Engine 时，遇到了大量链接错误：

```
error LNK2038: mismatch detected for 'RuntimeLibrary': value 'MDd_DynamicDebug' doesn't match value 'MTd_StaticDebug'
warning LNK4217: locally defined symbol __malloc_dbg imported in function ...
warning LNK4286: symbol __imp_malloc defined in libucrtd.lib imported by GraphicsEngineD3D11.lib
error LNK2001: unresolved external symbol __imp_ceilf
error LNK2001: unresolved external symbol __imp_cosf
error LNK2001: unresolved external symbol __imp_sinf
```

### 根本原因

**CRT 类型不匹配：**

| 组件 | 默认 CRT 类型 | 说明 |
|------|--------------|------|
| **CEF (libcef.lib)** | `/MTd` | 静态多线程调试 CRT |
| **Diligent Engine** | `/MDd` | 动态多线程调试 CRT |
| **EditorApp** | `/MTd` | 需要匹配 CEF |

**CEF 为什么要求静态 CRT？**
1. CEF 是预编译的二进制库，使用静态 CRT 避免版本冲突
2. 简化部署，不需要 Visual C++ Redistributable
3. Chromium 项目的标准配置

**混用 CRT 的后果：**
- 链接器无法解析符号（每个 CRT 有自己的堆管理器）
- 运行时崩溃（跨 CRT 边界传递内存指针）
- 难以调试的内存错误

---

## 决策

**我们决定：**

### 1️⃣ 统一使用静态 CRT

**所有 Moon Engine 组件使用：**
- **Debug 配置：** `/MTd` (Multi-threaded Debug)
- **Release 配置：** `/MT` (Multi-threaded)

**影响的项目：**
- ✅ `EditorApp` - 已使用 `/MTd`
- ✅ `EditorBridge` - 已使用 `/MTd`
- ✅ `EngineCore` - 修改为 `/MTd`
- ✅ `EngineRender` - 修改为 `/MTd`
- ✅ `HelloEngine` - 修改为 `/MTd`
- ✅ `Diligent Engine` - 重新编译为 `/MTd`

---

### 2️⃣ 修改 Diligent Engine 构建配置

**CMake 配置参数：**

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

**关键点：**
- `CMAKE_MSVC_RUNTIME_LIBRARY` - CMake 3.15+ 的标准方式
- 命令行参数优先级高于 `CMakeLists.txt`
- 必须删除旧 `build` 目录，避免缓存问题
- 禁用不需要的组件，减少编译时间

---

### 3️⃣ 验证流程

**编译后验证：**

```powershell
# 检查 Diligent 库（应该返回 0）
dumpbin /directives build\DiligentCore\Graphics\GraphicsEngineD3D11\Debug\*.lib | Select-String "MSVCRTD"

# 检查 EditorApp.exe（不应该有 MSVCR*.dll）
dumpbin /DEPENDENTS bin\x64\Debug\EditorApp.exe | Select-String "MSVCR|MSVCP|VCRUNTIME"
```

**成功标志：**
- ✅ `dumpbin` 显示 `/DEFAULTLIB:LIBCMTD` (不是 `MSVCRTD`)
- ✅ 没有 LNK4217/LNK4286 警告
- ✅ EditorApp.exe 不依赖 `MSVCR140D.dll`

---

## 后果

### ✅ 正面影响

**1. 解决了 CEF 集成问题**
- 消除了所有 CRT 相关的链接错误
- EditorApp 可以正常编译和运行
- CEF 浏览器窗口可以正常显示

**2. 简化部署**
- 不需要安装 Visual C++ Redistributable
- 减少 DLL 依赖冲突
- 更容易分发给最终用户

**3. 提高稳定性**
- 每个模块使用独立的 CRT 堆
- 避免跨模块内存管理问题
- 更容易隔离内存泄漏

---

### ❌ 负面影响

**1. 可执行文件变大**
- CRT 代码嵌入到 .exe 中
- 增加约 1-2 MB 文件大小
- **影响程度：** 轻微（对于游戏引擎来说可以接受）

**2. 编译时间变长**
- 需要重新编译 Diligent Engine（约 3-5 分钟）
- 每次清理构建都需要重新链接 CRT
- **缓解措施：** 使用并行编译 (`--parallel 4`)

**3. 内存开销增加**
- 每个模块有独立的 CRT 堆
- 理论上会有一定内存重复
- **影响程度：** 忽略不计（现代系统内存充足）

**4. 第三方库兼容性**
- 所有第三方库都必须使用 `/MT` 或 `/MTd`
- 预编译库如果是 `/MD` 则无法使用
- **解决方案：** 从源码编译，或寻找替代库

---

### 🔧 维护成本

**增加的维护工作：**

1. **新增第三方库时**
   - 必须检查 CRT 类型
   - 可能需要从源码编译

2. **CMake 配置管理**
   - 必须在命令行指定 CRT 参数
   - 不能依赖 CMake 缓存

3. **文档更新**
   - 需要记录 CRT 配置要求
   - 新开发者需要了解这个限制

---

## 替代方案

### 方案 A：统一使用动态 CRT (`/MD`)

**优点：**
- 可执行文件更小
- 编译速度更快
- 大多数第三方库默认使用 `/MD`

**缺点：**
- ❌ **CEF 不支持动态 CRT** - 这是致命的
- ❌ 需要分发 Visual C++ Redistributable
- ❌ 运行时可能遇到 CRT 版本冲突

**结论：** **不可行**（CEF 的硬性要求）

---

### 方案 B：混合 CRT（EditorApp 用 `/MT`，其他用 `/MD`）

**优点：**
- 其他模块（非 CEF 相关）可以用 `/MD`
- 编译速度更快

**缺点：**
- ❌ 跨模块传递内存指针会崩溃
- ❌ 难以调试和维护
- ❌ 违反"最小惊讶原则"

**结论：** **不推荐**（维护成本太高）

---

### 方案 C：使用预编译的 Diligent 库

**优点：**
- 不需要编译 Diligent Engine
- 节省编译时间

**缺点：**
- ❌ 官方没有提供 `/MT` 版本
- ❌ 自己构建预编译库增加了维护负担
- ❌ 不同开发者可能使用不同版本

**结论：** **不推荐**（从源码编译更可控）

---

## 实施细节

### 修改的文件

**1. CMake 配置（命令行参数）**
```powershell
# external/DiligentEngine/build/
cmake -B build -S external/DiligentEngine `
  -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded$<$<CONFIG:Debug>:Debug>"
```

**2. Visual Studio 项目文件（已有配置）**

`editor/app/EditorApp.vcxproj`:
```xml
<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>  <!-- /MTd -->
```

`engine/core/EngineCore.vcxproj`:
```xml
<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>  <!-- /MTd -->
```

`engine/render/EngineRender.vcxproj`:
```xml
<RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>  <!-- /MTd -->
```

---

### 编译流程（标准操作）

**完整步骤：**

```powershell
# 1. 清理 Diligent 旧构建
Remove-Item -Path "external\DiligentEngine\build" -Recurse -Force

# 2. 配置 Diligent Engine
cmake -B build -S external/DiligentEngine `
  -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded`$<`$<CONFIG:Debug>:Debug>" `
  -DCMAKE_CXX_FLAGS_DEBUG="/MTd" `
  -DCMAKE_C_FLAGS_DEBUG="/MTd" `
  -DDILIGENT_BUILD_SAMPLES=OFF `
  -DDILIGENT_BUILD_TESTS=OFF `
  -DDILIGENT_BUILD_TOOLS=OFF

# 3. 编译 Diligent Engine
cmake --build build --config Debug --parallel 4

# 4. 验证 CRT 类型
dumpbin /directives build\DiligentCore\Graphics\GraphicsEngineD3D11\Debug\*.lib | Select-String "MSVCRTD"

# 5. 重新编译 Moon Engine
Remove-Item bin\x64\Debug\Engine*.lib -Force
msbuild Moon.sln /t:EngineCore`;EngineRender /p:Configuration=Debug /p:Platform=x64

# 6. 编译 EditorApp
msbuild Moon.sln /t:EditorApp /p:Configuration=Debug /p:Platform=x64

# 7. 验证最终可执行文件
dumpbin /DEPENDENTS bin\x64\Debug\EditorApp.exe
```

---

### 验证检查清单

**编译时验证：**
- [ ] Diligent Engine 编译成功（无 lib.exe 崩溃）
- [ ] 没有 LNK2038 错误（RuntimeLibrary mismatch）
- [ ] 没有 LNK4217/LNK4286 警告（CRT 符号冲突）
- [ ] Moon Engine 项目编译成功

**库文件验证：**
- [ ] `dumpbin /directives *.lib | Select-String "MSVCRTD"` 返回 0
- [ ] `dumpbin /directives *.lib | Select-String "LIBCMTD"` 有大量结果

**可执行文件验证：**
- [ ] EditorApp.exe 成功生成
- [ ] `dumpbin /DEPENDENTS EditorApp.exe` 没有 `MSVCR*.dll`
- [ ] 运行 EditorApp.exe 没有缺少 DLL 的错误

---

## 经验教训

### 🎓 关键发现

**1. CMake 缓存问题**
- 修改 `CMakeLists.txt` 后必须删除 `build` 目录
- 命令行参数优先级高于 `CMakeLists.txt`
- 使用 `FORCE` 标志也可能被子模块覆盖

**2. Lib.exe 崩溃**
- 混合 `/MT` 和 `/MD` 的 `.obj` 文件会导致 Lib.exe 崩溃
- 错误代码 `-1073741510` (0xC000013A) 表示访问违例
- 唯一解决方法：完全删除 build 目录

**3. 第三方库管理**
- 预编译库很可能使用 `/MD`
- 从源码编译是保证 CRT 一致性的最佳方式
- 需要在文档中明确 CRT 要求

**4. 验证的重要性**
- 不要相信编译成功就是正确的
- 必须用 `dumpbin` 验证 CRT 类型
- LNK4217/LNK4286 警告不能忽略

---

### 📋 最佳实践

**DO (推荐):**
- ✅ 所有项目统一使用相同的 CRT 类型
- ✅ 使用命令行参数配置 CMake
- ✅ 删除旧 build 目录后重新配置
- ✅ 使用 `dumpbin` 验证库文件
- ✅ 禁用不需要的第三方库组件

**DON'T (避免):**
- ❌ 混用 `/MT` 和 `/MD`
- ❌ 忽略 LNK4217/LNK4286 警告
- ❌ 依赖 CMake 缓存
- ❌ 使用预编译的第三方库（除非确认 CRT 类型）
- ❌ 在不清理的情况下修改 CRT 配置

---

### 🔍 故障排查指南

**症状 1：LNK2038 错误**
```
error LNK2038: mismatch detected for 'RuntimeLibrary'
```
**解决：** 检查所有 `.vcxproj` 的 `<RuntimeLibrary>` 设置

---

**症状 2：LNK4217/LNK4286 警告**
```
warning LNK4217: locally defined symbol imported
```
**解决：** 使用 `dumpbin` 找到使用 `/MD` 的库，重新编译

---

**症状 3：Lib.exe 崩溃**
```
Command exited with code -1073741510
```
**解决：** 删除 `external\DiligentEngine\build` 目录

---

**症状 4：运行时找不到 DLL**
```
MSVCP140D.dll not found
```
**解决：** 检查 `dumpbin /DEPENDENTS *.exe`，重新编译相关库

---

## 参考资料

**官方文档：**
- [Microsoft Docs - /MD, /MT, /LD (Use Run-Time Library)](https://docs.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library)
- [CMake - CMAKE_MSVC_RUNTIME_LIBRARY](https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html)
- [CEF - Tutorial (Windows)](https://bitbucket.org/chromiumembedded/cef/wiki/Tutorial#markdown-header-windows)

**相关 ADR：**
- [ADR-0003: Editor UI Architecture](adr-0003-editor-ui-architecture.md) - CEF 集成决策

**相关文档：**
- [DEVELOPER_GUIDE.md § CRT 配置](DEVELOPER_GUIDE.md#️-c-runtime-library-crt-配置重要)

---

**决策日期：** 2025-11-10  
**决策者：** 开发团队 + AI Assistant  
**状态变更：** 提议 → 已接受  
**最后更新：** 2025-11-10
