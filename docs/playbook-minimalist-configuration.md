# Moon Engine 最小化配置原则（AI 开发指南）

> **核心理念：如非必要，勿增设置 (Occam's Razor for Configuration)**

## 🎯 原则说明

在 Moon Engine 项目开发中，所有配置、构建脚本、依赖关系必须遵循以下原则：

1. **最小必需原则** - 只保留项目运行所必需的配置
2. **避免冗余原则** - 删除所有重复、多余的设置
3. **自动化优先原则** - 优先使用工具自动推断，避免显式配置
4. **可维护性原则** - 减少配置数量 = 降低维护成本

---

## ✅ 正确示范（遵循原则）

### 1. 项目依赖关系

**✅ 推荐：使用 ProjectReference（自动推断依赖）**
```xml
<!-- EditorApp.vcxproj -->
<ItemGroup>
  <ProjectReference Include="..\bridge\EditorBridge.vcxproj">
    <Project>{24F11C82-584E-4AEB-8F20-09CCD345B193}</Project>
  </ProjectReference>
</ItemGroup>
```

**❌ 避免：在 .sln 中手动添加 ProjectDependencies（冗余）**
```xml
<!-- Moon.sln - 不需要这段 -->
Project("{...}") = "EditorApp", "..."
  ProjectSection(ProjectDependencies) = postProject
    {24F11C82-584E-4AEB-8F20-09CCD345B193} = {24F11C82-584E-4AEB-8F20-09CCD345B193}
  EndProjectSection
EndProject
```

**理由：** MSBuild 会自动从 `<ProjectReference>` 推断依赖顺序，无需在 sln 中重复声明。

---

### 2. Include 路径设置

**✅ 推荐：在 PropertyGroup 中统一设置**
```xml
<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
  <IncludePath>$(CEF_PATH);$(SolutionDir)editor\bridge;$(IncludePath)</IncludePath>
</PropertyGroup>
```

**❌ 避免：同时在 PropertyGroup 和 ItemDefinitionGroup 中重复设置**
```xml
<!-- 冗余配置 -->
<PropertyGroup>
  <IncludePath>$(CEF_PATH);$(IncludePath)</IncludePath>
</PropertyGroup>
<ItemDefinitionGroup>
  <ClCompile>
    <AdditionalIncludeDirectories>$(CEF_PATH);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
  </ClCompile>
</ItemDefinitionGroup>
```

---

### 3. PostBuild 事件（CEF 文件复制）

**✅ 推荐：手动复制一次（首次配置或 CEF 更新时）**
```powershell
# 在项目根目录执行（只需运行一次）
$CEF = "external\cef"
$OUT = "bin\x64\Debug"

Copy-Item "$CEF\Debug\libcef.dll" $OUT -Force
Copy-Item "$CEF\Debug\chrome_elf.dll" $OUT -Force
Copy-Item "$CEF\Debug\v8_context_snapshot.bin" $OUT -Force
Copy-Item "$CEF\Resources\icudtl.dat" $OUT -Force
Copy-Item "$CEF\Resources\locales" "$OUT\locales" -Recurse -Force

# GPU 支持
Copy-Item "$CEF\Debug\d3dcompiler_47.dll" $OUT -Force
Copy-Item "$CEF\Debug\libEGL.dll" $OUT -Force
Copy-Item "$CEF\Debug\libGLESv2.dll" $OUT -Force
```

在 .gitignore 中排除：
```gitignore
bin/**/*.dll
bin/**/*.bin
bin/**/locales/
bin/**/*.dat
```

**❌ 避免：在 PostBuild 中每次编译都复制**
```xml
<!-- 不推荐：每次编译都执行，浪费时间 -->
<PostBuildEvent>
  <Command>
    xcopy /Y /D "$(CEF_PATH)\Debug\*.dll" "$(OutDir)"
    xcopy /Y /D "$(CEF_PATH)\Resources\*" "$(OutDir)"
  </Command>
</PostBuildEvent>
```

**理由：** 
- CEF SDK 不会频繁修改，不需要每次编译都复制
- 手动复制更清晰，知道复制了什么文件
- 编译更快，不执行额外的 PostBuild 命令
- 符合业界实践（Spotify、Discord、VS Code 等都采用手动复制）
- 只有在首次配置或 CEF 更新时才需要重新复制

---

### 4. 编译器设置去重

**✅ 推荐：每个设置只出现一次**
```xml
<ClCompile>
  <WarningLevel>Level4</WarningLevel>
  <RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>
  <LanguageStandard>stdcpp17</LanguageStandard>
</ClCompile>
```

**❌ 避免：重复定义相同设置**
```xml
<ClCompile>
  <WarningLevel>Level4</WarningLevel>
  <RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>
  <WarningLevel>Level4</WarningLevel>  <!-- 重复！ -->
  <LanguageStandard>stdcpp17</LanguageStandard>
</ClCompile>
```

---

## 🛠️ AI 开发检查清单

在修改项目配置时，AI 必须检查以下项目：

- [ ] **依赖关系**：是否可以用 `<ProjectReference>` 自动推断？
- [ ] **Include 路径**：是否在多处重复定义？
- [ ] **PostBuild 事件**：复制的文件是否都是必需的？
- [ ] **编译器设置**：是否有重复的设置项？
- [ ] **链接库**：是否链接了不需要的库？
- [ ] **宏定义**：是否有冗余的预处理器定义？
- [ ] **输出目录**：是否使用了相对路径宏（如 `$(SolutionDir)`）而非硬编码？

---

## 📐 判断是否需要某个配置的决策树

```
是否需要添加配置 X？
  ├─ 工具能否自动推断？
  │   ├─ 是 → ❌ 不添加，使用默认行为
  │   └─ 否 → 继续
  ├─ 是否影响构建/运行？
  │   ├─ 否 → ❌ 不添加
  │   └─ 是 → 继续
  ├─ 是否有更简单的替代方案？
  │   ├─ 是 → ❌ 使用更简单的方案
  │   └─ 否 → ✅ 添加，并写注释说明原因
```

---

## 🚫 常见冗余配置模式（必须避免）

### 1. 双重依赖声明
- .sln 中的 `ProjectDependencies` + .vcxproj 中的 `ProjectReference`

### 2. 双重路径设置
- `<IncludePath>` + `<AdditionalIncludeDirectories>`

### 3. PostBuild 每次复制不变的文件
- ❌ 每次编译都复制 CEF 运行时文件
- ✅ 改用手动复制一次（详见 [CEF_SETUP.md](../../editor/bridge/CEF_SETUP.md)）

### 4. 重复的编译器标志
- 同一个 `<ClCompile>` 节点中多次设置 `<WarningLevel>`

### 5. 硬编码路径
- 使用 `E:\game_engine\Moon\...` 而非 `$(SolutionDir)...`

---

## 📝 配置修改记录要求

每次修改项目配置时，必须在 commit message 中说明：

```
✅ 好的 commit message:
feat(build): 删除冗余的 ProjectDependencies，MSBuild 可自动推断

❌ 不好的 commit message:
update project files
```

---

## 🔧 定期清理检查

每月或每个大版本发布前，必须检查：

1. **是否有新的冗余配置？**
2. **PostBuild 事件是否还需要复制所有文件？**
3. **依赖关系是否可以进一步简化？**
4. **是否有不再使用的宏定义？**

---

## 💡 实际案例分析

### 案例 1: EditorApp 依赖 EditorBridge

**❌ 之前（冗余）:**
```xml
<!-- Moon.sln -->
Project("{...}") = "EditorApp", "..."
  ProjectSection(ProjectDependencies) = postProject
    {EditorBridge-GUID} = {EditorBridge-GUID}
  EndProjectSection
EndProject

<!-- EditorApp.vcxproj -->
<ItemGroup>
  <ProjectReference Include="..\bridge\EditorBridge.vcxproj" />
</ItemGroup>
```

**✅ 之后（精简）:**
```xml
<!-- Moon.sln - 删除 ProjectDependencies 节点 -->
Project("{...}") = "EditorApp", "..."
EndProject

<!-- EditorApp.vcxproj - 保留 ProjectReference 即可 -->
<ItemGroup>
  <ProjectReference Include="..\bridge\EditorBridge.vcxproj">
    <Project>{24F11C82-584E-4AEB-8F20-09CCD345B193}</Project>
  </ProjectReference>
</ItemGroup>
```

**节省：** 3 行代码，减少一个维护点

---

### 案例 2: CEF 文件复制

**❌ 之前（每次编译都复制）:**
```xml
<PostBuildEvent>
  <Command>
    xcopy /Y /D "$(CEF_PATH)\Debug\*.dll" "$(OutDir)"
    xcopy /Y /D "$(CEF_PATH)\Debug\*.bin" "$(OutDir)"
    xcopy /Y /D "$(CEF_PATH)\Resources\*.pak" "$(OutDir)"
  </Command>
</PostBuildEvent>
```

**✅ 之后（手动复制一次）:**
```powershell
# 只在首次配置或 CEF 更新时运行
Copy-Item "external\cef\Debug\libcef.dll" "bin\x64\Debug\" -Force
Copy-Item "external\cef\Debug\chrome_elf.dll" "bin\x64\Debug\" -Force
# ... 其他必需文件
```

**删除 PostBuild 事件**，在 CEF_SETUP.md 中说明手动复制步骤。

**节省：** 
- 每次编译节省 1-2 秒
- 日志更清爽（不再显示 "Copying CEF runtime files..."）
- 减少约 50MB 不必要文件的复制
- 符合业界标准做法（Spotify、Discord、VS Code 等）

---

## 🎓 AI 开发者准则总结

1. **添加配置前先问：这是否必需？**
2. **发现重复配置立即删除**
3. **优先使用工具自动推断**
4. **每次修改后验证构建仍然成功**
5. **记录删除理由，防止误认为遗漏**

---

## 📚 参考资料

- [MSBuild Project References](https://learn.microsoft.com/en-us/visualstudio/msbuild/common-msbuild-project-items#projectreference)
- [CEF Minimal Distribution Files](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage#markdown-header-windows)
- [Visual Studio Project Properties Best Practices](https://learn.microsoft.com/en-us/cpp/build/reference/property-pages-visual-cpp)

---

**最后更新：** 2025-11-08  
**维护者：** Moon Engine Team
