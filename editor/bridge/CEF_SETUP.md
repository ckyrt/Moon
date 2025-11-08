# CEF SDK 集成说明

## 当前状态
✅ 代码框架已创建完成
⚠️ 需要下载和配置 CEF SDK

## 快速配置步骤

### 1. 下载 CEF SDK

**下载地址**: https://cef-builds.spotifycdn.com/index.html

**选择版本**:
- Platform: Windows 64-bit
- Branch: 最新稳定版（建议 119 或更新）
- Type: **Standard Distribution**

**下载文件**: `cef_binary_*_windows64.tar.bz2` (约 150MB)

### 2. 解压到项目目录

推荐路径: `E:\game_engine\Moon\external\cef`

```powershell
# 解压后目录结构应该是:
E:\game_engine\Moon\external\cef\
  ├── Debug/
  ├── Release/
  ├── Resources/
  ├── include/
  ├── libcef_dll/
  └── CMakeLists.txt
```

### 3. 编译 libcef_dll_wrapper

CEF 需要先编译一个包装库：

```powershell
cd E:\game_engine\Moon\external\cef
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug
cmake --build . --config Release
```

这会生成: `libcef_dll_wrapper.lib`

### 4. 修改项目配置

编辑 `editor/app/EditorApp.vcxproj`，修改第 51 行：

```xml
<CEF_PATH>E:\game_engine\Moon\external\cef</CEF_PATH>
```

### 5. 手动复制 CEF 运行时文件（一次性操作）

⚠️ **重要：这是必需步骤，只需要执行一次（除非 CEF SDK 更新）**

编译后需要手动复制这些文件到输出目录 `bin/x64/Debug/`：

#### 核心文件（必需）
从 `external/cef/Debug/` 复制：
- `libcef.dll` - CEF 主库
- `chrome_elf.dll` - Chrome 加载器
- `v8_context_snapshot.bin` - V8 快照

从 `external/cef/Resources/` 复制：
- `icudtl.dat` - Unicode 数据
- `locales/` - **整个文件夹**（本地化资源）

#### GPU 支持（推荐，支持 WebGL/Canvas）
从 `external/cef/Debug/` 复制：
- `d3dcompiler_47.dll`
- `libEGL.dll`
- `libGLESv2.dll`

#### PowerShell 快速复制命令

```powershell
# 在项目根目录执行
$CEF = "external\cef"
$OUT = "bin\x64\Debug"

# 核心文件
Copy-Item "$CEF\Debug\libcef.dll" $OUT -Force
Copy-Item "$CEF\Debug\chrome_elf.dll" $OUT -Force
Copy-Item "$CEF\Debug\v8_context_snapshot.bin" $OUT -Force
Copy-Item "$CEF\Resources\icudtl.dat" $OUT -Force
Copy-Item "$CEF\Resources\locales" "$OUT\locales" -Recurse -Force

# GPU 支持
Copy-Item "$CEF\Debug\d3dcompiler_47.dll" $OUT -Force
Copy-Item "$CEF\Debug\libEGL.dll" $OUT -Force
Copy-Item "$CEF\Debug\libGLESv2.dll" $OUT -Force

Write-Host "✅ CEF runtime files copied successfully!"
```

#### 对于 Release 配置

将上述命令中的 `Debug` 替换为 `Release` 即可：
```powershell
$OUT = "bin\x64\Release"
# ... 其他命令相同，只是路径从 Debug 改为 Release
```

#### ✨ 为什么不用 PostBuild 自动复制？

根据 Moon Engine 的**最小化配置原则**（详见 [MINIMALIST_CONFIGURATION.md](../../docs/playbooks/MINIMALIST_CONFIGURATION.md)）：

1. **CEF SDK 不会频繁修改** - 不需要每次编译都复制
2. **手动复制更清晰** - 明确知道复制了什么文件
3. **编译更快** - 不执行额外的 PostBuild 命令
4. **符合业界实践** - Spotify、Discord 等项目都采用手动复制

只有在以下情况才需要重新复制：
- 首次配置 CEF
- 更新 CEF SDK 版本
- 切换 Debug/Release 配置

---

### 6. 添加到解决方案

在 Visual Studio 中:
1. 右键解决方案 → 添加 → 现有项目
2. 选择 `editor/samples/EditorApp.vcxproj`
3. 设置为启动项目

### 7. 编译运行

按 F5 运行，应该看到一个 CEF 窗口显示 "Hello from CEF!"
