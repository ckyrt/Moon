# ADR 0003: 编辑器 UI 架构选择

## 状态 (Status)
✅ **已采纳** (Accepted) - 2025-11-07

## 背景 (Context)

在开发 Moon Engine 编辑器时，我们需要选择一个合适的 UI 技术栈。编辑器需要满足以下需求：

### 核心需求
1. **快速的 UI 开发** - 编辑器界面复杂（场景树、属性面板、工具栏、资产浏览器等）
2. **原生渲染性能** - 3D 视口必须达到 60+ FPS，使用 DirectX 12/Vulkan 等原生 API
3. **C++ 主导** - 游戏引擎核心是 C++，包含渲染、物理、网络等多个模块
4. **UI 是附加功能** - 编辑器 UI 只是引擎的一个子系统，不应主导整体架构
5. **现代化界面** - 需要现代化的 UI 设计，丰富的组件库和样式支持

### 候选方案对比

#### 方案 1: ImGui (即时模式 GUI)
**优点：**
- C++ 原生，集成简单
- 性能好，适合游戏引擎
- 不需要额外依赖

**缺点：**
- UI 开发效率低
- 样式定制困难
- 缺乏现代化 UI 组件
- 不适合复杂的编辑器界面

#### 方案 2: Qt/wxWidgets (原生 GUI 框架)
**优点：**
- 成熟稳定
- 跨平台支持好
- C++ 原生集成

**缺点：**
- UI 开发仍然较慢
- 需要学习 Qt/wxWidgets
- 样式和主题系统不如 Web 灵活
- 社区生态不如 Web

#### 方案 3: Electron + Node.js N-API
**优点：**
- Web 技术栈，UI 开发快速
- npm 生态丰富
- 热重载，开发体验好

**缺点：**
- JavaScript 主导，C++ 变成"插件"
- 不符合 C++ 引擎主导的架构理念
- 体积大（~120MB），依赖 Node.js 运行时
- 需要维护 Node.js 绑定层

#### 方案 4: CEF (Chromium Embedded Framework)
**优点：**
- C++ 主程序，完全控制权
- Web UI（React/Vue），开发快速
- 体积较小（~70MB）
- 不依赖 Node.js
- 适合 C++ 引擎的模块化架构

**缺点：**
- CEF 集成有学习曲线
- JavaScript 绑定需要手写

## 决策 (Decision)

我们决定采用 **CEF (Chromium Embedded Framework) + React** 作为编辑器 UI 架构。

**重要说明**：CEF 只是**可选的编辑器界面**，不是引擎核心依赖。引擎核心保持完全独立，可以无 UI 运行。

### 最终技术栈

```
┌─────────────────────────────────────────────────────────────┐
│                    构建配置选择                              │
│  ┌────────────────────┐      ┌──────────────────────────┐   │
│  │  服务器构建        │      │  编辑器构建              │   │
│  │  (无 UI)          │      │  (有 UI)                 │   │
│  └────────────────────┘      └──────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
          │                              │
          ▼                              ▼
┌──────────────────────┐      ┌──────────────────────────────┐
│  EngineCore.lib      │      │  C++ 主程序 (EditorApp.exe)  │
│  (纯 C++)            │      │  ┌────────────────────────┐  │
│  - Scene 管理        │◄─────┤  │  EngineCore.lib        │  │
│  - Component System  │      │  │  (引擎核心)            │  │
│  - Physics           │      │  └────────────────────────┘  │
│  - Network           │      │  ┌────────────────────────┐  │
│  - Scripting         │      │  │  DiligentRenderer      │  │
│                      │      │  │  (原生 DirectX 12)     │  │
│  + NullRenderer      │      │  └────────────────────────┘  │
│  (无渲染开销)        │      │  ┌────────────────────────┐  │
│                      │      │  │  CEF UI 子系统         │  │
│  可独立运行          │      │  │  ┌──────────────────┐  │  │
│  无 UI 依赖          │      │  │  │  React + TS      │  │  │
└──────────────────────┘      │  │  │  - Hierarchy     │  │  │
                              │  │  │  - Inspector     │  │  │
                              │  │  │  - Toolbar       │  │  │
                              │  │  └──────────────────┘  │  │
                              │  │  JS ↔ C++ Binding      │  │
                              │  └────────────────────────┘  │
                              │  (可选模块，独立编译)        │
                              └──────────────────────────────┘
```

### 架构特点

1. **引擎核心完全独立** ⭐⭐⭐⭐⭐
   - EngineCore 不依赖 CEF、React 或任何 UI 框架
   - 可以编译为纯 C++ 库（.lib/.a）
   - 服务器构建不包含任何 UI 代码
   - 支持 authoritative server logic

2. **UI 与引擎彻底解耦** ⭐⭐⭐⭐⭐
   - CEF 只是可选的编辑器界面
   - 引擎可以无 UI 运行（服务器模式）
   - UI 通过清晰的 API 接口与引擎通信
   - 引擎代码完全不知道 UI 的存在

3. **渲染后端可替换** ⭐⭐⭐⭐⭐
   - 引擎通过 IRenderer 接口与渲染层通信
   - 服务器使用 NullRenderer（零开销）
   - 编辑器使用 DiligentRenderer（DirectX 12）
   - 可以切换到 bgfx、Vulkan 等任意后端

4. **原生渲染性能**
   - 3D 视口使用原生 HWND + DiligentRenderer
   - 不经过 CEF，直接 DirectX 12 渲染
   - 零性能损失，60+ FPS 无压力

5. **Web UI 开发效率**
   - React 组件化开发
   - TypeScript 类型安全
   - Tailwind CSS / Ant Design 样式库
   - Vite 构建工具

6. **轻量化**
   - 引擎核心不依赖 Node.js 运行时
   - 服务器构建 < 30MB
   - 编辑器构建 ~90MB（包含 CEF）
   - 启动速度快

### 通信机制

```cpp
// C++ 端：注册 JavaScript 函数
class EngineJSHandler : public CefV8Handler {
public:
    bool Execute(const CefString& name,
                 CefRefPtr<CefV8Value> object,
                 const CefV8ValueList& arguments,
                 CefRefPtr<CefV8Value>& retval,
                 CefString& exception) override {
        
        if (name == "createNode") {
            std::string nodeName = arguments[0]->GetStringValue();
            SceneNode* node = g_engine->CreateNode(nodeName);
            retval = CefV8Value::CreateInt(node->GetID());
            return true;
        }
        
        if (name == "getSceneGraph") {
            Scene* scene = g_engine->GetMainScene();
            std::string json = SerializeSceneToJSON(scene);
            retval = CefV8Value::CreateString(json);
            return true;
        }
        
        return false;
    }
};
```

```typescript
// React 端：调用 C++ 函数
declare global {
  interface Window {
    engine: {
      createNode: (name: string) => number;
      getSceneGraph: () => SceneNode[];
      updateTransform: (id: number, transform: Transform) => void;
      deleteNode: (id: number) => void;
    };
  }
}

function Toolbar() {
  const handleCreateCube = () => {
    const nodeId = window.engine.createNode('Cube');
    console.log('Created node:', nodeId);
  };
  
  return <button onClick={handleCreateCube}>Create Cube</button>;
}
```

### 主循环结构

**服务器模式（无 UI）**：
```cpp
// ServerMain.cpp
int main() {
    // 纯引擎，无 UI
    EngineCore engine;
    engine.SetRenderer(new NullRenderer()); // 无渲染开销
    engine.Initialize();
    
    // 主循环 - 只有逻辑
    while (running) {
        float dt = CalculateDeltaTime();
        engine.Tick(dt);
        engine.SimulatePhysics(dt);
        engine.ProcessNetworkMessages();
        // 无渲染调用
    }
    
    engine.Shutdown();
    return 0;
}
```

**编辑器模式（有 UI）**：
```cpp
// EditorMain.cpp
int main() {
    // 初始化引擎（独立于 UI）
    EngineCore engine;
    engine.SetRenderer(new DiligentRenderer());
    engine.Initialize();
    
    // 初始化编辑器 UI（可选）
    EditorApp editor;
    editor.AttachEngine(&engine); // 松耦合
    editor.Initialize();
    
    MSG msg;
    while (running) {
        // 1. 处理 Windows 消息
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        // 2. 处理 CEF 消息（非阻塞）
        CefDoMessageLoopWork();
        
        // 3. 更新引擎（所有模块）
        float dt = CalculateDeltaTime();
        engine.Tick(dt);
        
        // 4. 渲染 3D 视口（原生）
        IRenderer* renderer = engine.GetRenderer();
        renderer->BeginFrame();
        engine.Render();
        renderer->EndFrame();
    }
    
    editor.Shutdown();
    engine.Shutdown();
    return 0;
}
```

**关键点**：
- ✅ EngineCore 完全不知道 EditorApp 的存在
- ✅ 服务器构建根本不链接 EditorApp/CEF
- ✅ 引擎可以在任何环境下运行
- ✅ UI 只是一个可选的观察者/控制器

## 后果 (Consequences)

### 正面影响 ✅

1. **架构清晰**
   - C++ 引擎保持主导地位
   - UI 作为可插拔的子系统
   - 符合模块化设计原则

2. **性能最优**
   - 3D 渲染使用原生 API，零开销
   - C++ ↔ JavaScript 通信 <1ms 延迟
   - UI 渲染使用 Chromium，性能优秀

3. **开发效率高**
   - React 组件化，快速开发复杂 UI
   - TypeScript 类型安全
   - 丰富的 Web 生态和组件库
   - 可以使用现代 CSS 框架（Tailwind、Ant Design）

4. **体积合理**
   - 不依赖 Node.js
   - 比 Electron 小 ~80MB
   - 启动速度快

5. **适合长期发展**
   - C++ 引擎可以独立演进
   - UI 可以独立迭代
   - 两者通过清晰的接口通信

### 负面影响 ❌

1. **集成复杂度**
   - CEF 初次集成有学习曲线
   - 需要手写 JavaScript 绑定代码
   - 跨平台需要适配不同的 CEF 构建

2. **调试挑战**
   - 需要同时调试 C++ 和 JavaScript
   - CEF 错误信息有时不够清晰

3. **热重载限制**
   - React 开发时热重载需要额外配置
   - 不如纯 Web 开发方便

### 缓解措施

1. **逐步实施**
   - **Step 1**: 集成 CEF SDK，创建基础窗口
   - **Step 2**: 实现 JavaScript 绑定层（简单直接）
   - **Step 3**: 开发 React UI 组件
   - **Step 4**: 嵌入原生渲染窗口
   - **Step 5**: 完善功能和打磨

2. **提供清晰的绑定层**
   - 封装通用的 JavaScript 绑定工具类
   - 使用 CefV8Handler 统一管理所有 API
   - 提供类型安全的接口定义

3. **完善文档**
   - 提供 CEF 集成教程
   - JavaScript 绑定最佳实践
   - 常见问题解决方案

## 参考 (References)

### 类似架构的成功案例
- **Unreal Engine**: C++ 引擎 + Slate UI（类似架构理念）
- **CryEngine**: C++ 引擎 + Qt/Web 混合 UI
- **Babylon.js Editor**: Electron + WebGL（Web 版本）
- **Figma Desktop**: Electron + Native 渲染（类似性能需求）

### 技术文档
- [CEF Official Documentation](https://bitbucket.org/chromiumembedded/cef/wiki/Home)
- [CEF JavaScript Integration](https://bitbucket.org/chromiumembedded/cef/wiki/JavaScriptIntegration)
- [React Official Documentation](https://react.dev/)
- [Diligent Engine](https://github.com/DiligentGraphics/DiligentEngine)

## 相关 ADR
- [ADR 0005: 坐标系统与矩阵约定](0005-coordinate-system-and-matrix-conventions.md) - 渲染系统约定
- ADR 0001: 模块化架构选择（待创建）
- ADR 0004: IPC 协议选择（待创建）

## 修订历史 (Revision History)
- 2025-11-07: 初始版本，选择 CEF + React 作为编辑器 UI 架构
