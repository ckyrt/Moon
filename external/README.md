# External Dependencies

本目录存放第三方依赖库。**这些库不包含在 Git 仓库中**，克隆项目后需要手动下载。

---

## 快速开始（新成员必读）

克隆 Moon 项目后，需要获取两个依赖库：

1. **DiligentEngine** - 图形渲染引擎
2. **CEF** - 编辑器 UI 浏览器引擎

请按照下面的步骤操作。

---

## Diligent Engine（图形渲染引擎）

**重要**: DiligentEngine 已作为 Git 子模块添加到项目中。

### 获取步骤

**初次克隆项目后**:

```powershell
# 在 Moon 项目根目录执行
git submodule update --init --recursive
```

**如果项目已存在，只需更新子模块**:

```powershell
git submodule update --recursive
```

### 手动克隆（备选方案）

如果子模块方式有问题，也可以手动克隆：

```powershell
# 在 Moon 项目根目录执行
cd external
git clone --recursive https://github.com/DiligentGraphics/DiligentEngine.git
cd DiligentEngine
```

### 验证目录结构

```
external/DiligentEngine/
├── DiligentCore/
├── DiligentFX/
├── DiligentTools/
├── DiligentSamples/
├── CMakeLists.txt
└── README.md
```

### 当前使用版本

- **Version**: master 分支（或者特定 tag，例如 v2.5.4）
- **Repository**: https://github.com/DiligentGraphics/DiligentEngine

---

## CEF (Chromium Embedded Framework)

用于编辑器 UI 的浏览器引擎。

### 下载步骤

1. **访问**: https://cef-builds.spotifycdn.com/index.html

2. **选择版本**:
   - Platform: **Windows 64-bit**
   - Branch: **Stable** (最新稳定版，推荐 119 或更高)
   - Type: **Standard Distribution** (不要选 Minimal)

3. **下载文件**: 
   - 文件名类似: `cef_binary_119.x.x_windows64.tar.bz2`
   - 大小约 150MB (压缩)，解压后约 500MB

4. **解压到**:
   ```
   Moon/external/cef/
   ```

5. **验证目录结构**:
   ```
   external/cef/
   ├── Debug/
   ├── Release/
   ├── Resources/
   ├── include/
   ├── libcef_dll/
   └── CMakeLists.txt
   ```

6. **编译 libcef_dll_wrapper**:
   ```powershell
   cd external/cef
   mkdir build
   cd build
   cmake .. -G "Visual Studio 17 2022" -A x64
   cmake --build . --config Debug --target libcef_dll_wrapper
   cmake --build . --config Release --target libcef_dll_wrapper
   ```

7. **复制运行时文件**:
   ```powershell
   # 从项目根目录执行
   Copy-Item "external\cef\Debug\*.dll" "bin\x64\Debug\" -Force
   Copy-Item "external\cef\Debug\*.bin" "bin\x64\Debug\" -Force
   Copy-Item "external\cef\Resources\*" "bin\x64\Debug\" -Recurse -Force
   ```

### 当前使用版本

- **CEF Version**: 119+ (任何稳定版本)
- **Chromium Version**: 119+

---

## Diligent Engine

图形渲染引擎（已包含在仓库中）。

无需额外下载，已作为 Git 子模块包含。

---

## 故障排除

### 编译错误：找不到 CEF 头文件
- 确认 `external/cef/include/` 目录存在
- 确认 `EditorBridge.vcxproj` 中的 include 路径正确

### 运行错误：找不到 libcef.dll
- 确认已复制运行时文件到 `bin/x64/Debug/`
- 确认 `locales/` 文件夹和 `icudtl.dat` 都已复制

### CEF 编译错误
- 确保下载的是 **Standard Distribution**，不是 Minimal
- 确保使用 Visual Studio 2022 (v143 工具集)
- 确保 CMake 版本 >= 3.15

---

## 自动化脚本（未来）

TODO: 创建 PowerShell 脚本自动化上述步骤
