# Moon Engine 编辑器项目配置说明

## 项目结构

```
editor/
├── bridge/          # EditorBridge - CEF桥接静态库
│   ├── EditorBridge.h/cpp
│   ├── cef/
│   │   ├── CefClient.h/cpp
│   │   └── CefApp.h
│   └── EditorBridge.vcxproj
└── app/             # EditorApp - 主应用程序
    ├── EditorApp.cpp
    └── EditorApp.vcxproj
```

## 项目配置详情

### EditorBridge (静态库)

**项目类型**: StaticLibrary  
**输出**: `bin\x64\{Debug|Release}\EditorBridge.lib`  
**中间文件**: `temp\EditorBridge\x64\{Debug|Release}\`

#### 编译器设置

| 配置项 | Debug | Release |
|--------|-------|---------|
| C++标准 | C++17 | C++17 |
| 运行时库 | /MTd (静态Debug) | /MT (静态Release) |
| 警告级别 | Level4 | Level4 |
| SDL检查 | 启用 | 启用 |
| 多处理器编译 | 启用 | 启用 |
| 优化 | 禁用 | MaxSpeed |
| 内联 | - | AnySuitable |
| 优化偏好 | - | Speed |

#### 预处理器定义
- Debug: `_DEBUG;_LIB;NOMINMAX;WIN32_LEAN_AND_MEAN`
- Release: `NDEBUG;_LIB;NOMINMAX;WIN32_LEAN_AND_MEAN`

#### Include路径
- `$(CEF_PATH)` - CEF SDK根目录

#### 依赖的CEF组件
- CEF头文件 (`include/`)
- libcef_dll_wrapper (静态库)

---

### EditorApp (可执行程序)

**项目类型**: Application  
**输出**: `bin\x64\{Debug|Release}\EditorApp.exe`  
**中间文件**: `temp\EditorApp\x64\{Debug|Release}\`

#### 编译器设置

| 配置项 | Debug | Release |
|--------|-------|---------|
| C++标准 | C++17 | C++17 |
| 运行时库 | /MTd (静态Debug) | /MT (静态Release) |
| 警告级别 | Level4 | Level4 |
| SDL检查 | 启用 | 启用 |
| 多处理器编译 | 启用 | 启用 |
| 优化 | 禁用 | MaxSpeed |
| 内联 | - | AnySuitable |
| 优化偏好 | - | Speed |
| 全程序优化 | - | 启用 |

#### 预处理器定义
- Debug: `_DEBUG;_WINDOWS;NOMINMAX;WIN32_LEAN_AND_MEAN`
- Release: `NDEBUG;_WINDOWS;NOMINMAX;WIN32_LEAN_AND_MEAN`

#### Include路径
- `$(CEF_PATH)` - CEF SDK根目录
- `$(SolutionDir)editor\bridge` - EditorBridge头文件

#### Library路径
- Debug: `$(CEF_PATH)\Debug` + `$(CEF_PATH)\build\libcef_dll_wrapper\Debug`
- Release: `$(CEF_PATH)\Release` + `$(CEF_PATH)\build\libcef_dll_wrapper\Release`

#### 链接库
- `libcef.lib` - CEF主库
- `libcef_dll_wrapper.lib` - CEF C++包装器
- 标准Windows库

#### 项目引用
- `EditorBridge.vcxproj` - 自动链接EditorBridge.lib

#### PostBuild事件

自动复制CEF运行时文件到输出目录：
```batch
xcopy /Y /D "$(CEF_PATH)\{Debug|Release}\*.dll" "$(OutDir)"
xcopy /Y /D "$(CEF_PATH)\{Debug|Release}\*.bin" "$(OutDir)"
xcopy /Y /D "$(CEF_PATH)\Resources\icudtl.dat" "$(OutDir)"
xcopy /Y /E /I /D "$(CEF_PATH)\Resources\locales" "$(OutDir)locales\"
```

---

## 关键配置说明

### 1. 运行时库一致性 ⚠️

**重要**: 所有组件必须使用相同的运行时库类型！

- **EditorApp**: /MTd (Debug) 或 /MT (Release)
- **EditorBridge**: /MTd (Debug) 或 /MT (Release)
- **libcef_dll_wrapper**: /MTd (Debug) 或 /MT (Release)

❌ 不匹配会导致链接错误：
```
error LNK2038: mismatch detected for 'RuntimeLibrary'
```

### 2. C++17标准

CEF 141+ 需要 C++17 支持（使用了 `std::in_place_t`等特性）

### 3. 预处理器宏

- `NOMINMAX`: 避免Windows.h定义min/max宏与std::min/max冲突
- `WIN32_LEAN_AND_MEAN`: 减少Windows.h包含的内容，加快编译

### 4. CEF路径宏

```xml
<CEF_PATH>$(SolutionDir)external\cef</CEF_PATH>
```

- 使用`$(SolutionDir)`使路径相对于解决方案
- 便于团队协作，无需修改绝对路径

### 5. 输出目录结构

```
bin/
└── x64/
    ├── Debug/
    │   ├── EditorApp.exe
    │   ├── EditorBridge.lib
    │   ├── libcef.dll (自动复制)
    │   ├── *.bin (自动复制)
    │   └── locales/ (自动复制)
    └── Release/
        └── (同上)
```

### 6. 中间文件隔离

```
temp/
├── EditorApp/
│   └── x64/
│       ├── Debug/
│       └── Release/
└── EditorBridge/
    └── x64/
        ├── Debug/
        └── Release/
```

每个项目有独立的中间文件目录，避免冲突。

---

## 编译流程

1. **EditorBridge编译**
   - 编译C++源文件 → .obj
   - 打包 → EditorBridge.lib

2. **EditorApp编译**
   - 编译C++源文件 → .obj
   - 链接 EditorBridge.lib + libcef.lib + libcef_dll_wrapper.lib
   - 生成 EditorApp.exe
   - PostBuild: 复制CEF运行时文件

3. **运行**
   - EditorApp.exe 加载 libcef.dll
   - CEF创建子进程（渲染器、GPU等）

---

## 常见问题

### Q: 编译时出现 LNK2038 运行时库不匹配？

**A**: 检查所有项目的运行时库设置：
1. EditorApp.vcxproj → 属性 → C/C++ → 代码生成 → 运行时库
2. EditorBridge.vcxproj → 同上
3. 确保libcef_dll_wrapper也是用相同设置编译的

### Q: 运行时找不到 libcef.dll？

**A**: PostBuild事件可能失败，手动复制：
```powershell
Copy-Item "external\cef\Debug\*.dll" "bin\x64\Debug\" -Force
Copy-Item "external\cef\Debug\*.bin" "bin\x64\Debug\" -Force
```

### Q: CEF窗口显示空白？

**A**: 检查：
1. icudtl.dat 是否在bin目录
2. locales/ 文件夹是否完整
3. 查看控制台是否有CEF错误信息

### Q: 大量C4100警告（未使用参数）？

**A**: 这些警告来自CEF头文件，可以忽略或在项目中禁用特定警告：
```xml
<DisableSpecificWarnings>4100</DisableSpecificWarnings>
```

### Q: 为什么使用静态运行库(/MT)而不是动态(/MD)？

**A**: 
- CEF的libcef_dll_wrapper通常用/MT编译
- 减少运行时依赖（无需MSVC运行时DLL）
- 更好的部署体验

---

## 优化建议

### 当前已实施的优化

✅ 多处理器编译 (`/MP`)  
✅ 增量链接 (Debug)  
✅ 函数级链接 (Release `/Gy`)  
✅ 内联优化 (Release `/Ob2`)  
✅ 速度优化 (Release `/Ot`)  
✅ PostBuild自动化  
✅ Level4警告级别  

### 可选的进一步优化

- [ ] 添加预编译头 (PCH) - 可加速20-30%编译时间
- [ ] 链接时代码生成 (LTCG/WPO) - Release构建
- [ ] Profile-Guided Optimization (PGO) - 高级性能优化
- [ ] Unity Build - 大幅减少编译时间

---

## 更新日志

### 2025-11-08
- ✅ 统一C++标准为C++17
- ✅ 对齐运行时库为静态(/MT)
- ✅ 提升警告级别至Level4
- ✅ 添加Release优化选项
- ✅ 添加PostBuild事件自动复制CEF文件
- ✅ 移除不必要的库忽略设置

---

## 参考资料

- [CEF C++ API文档](https://bitbucket.org/chromiumembedded/cef/wiki/Home)
- [MSBuild项目文件参考](https://learn.microsoft.com/en-us/cpp/build/reference/)
- [C++运行时库说明](https://learn.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library)
