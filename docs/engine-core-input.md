# Input System Module

## 职责 (Responsibilities)
- 键盘输入捕获与状态管理
- 鼠标输入（位置、按键、滚轮）
- 输入事件系统
- 输入状态查询（按下、释放、保持）
- 跨平台输入抽象

## 接口文件 (Interface Files)
- `IInputSystem.h` - 输入系统接口
- `InputSystem.h/.cpp` - 输入系统实现（Win32）
- `InputTypes.h` - Vector2 类型定义
- `KeyCodes.h` - 键盘码和鼠标按键定义

## 依赖 (Dependencies)
- Windows.h (Win32 API) - Windows 平台

## 核心功能 (Core Features)

### 1. 键盘输入
- **按键状态检测**：按下/释放/保持
- **实时查询**：`IsKeyDown()` 查询当前按键状态
- **帧事件**：`IsKeyPressed()` 和 `IsKeyReleased()` 检测按键变化
- **按键组合支持**：可同时检测多个按键

### 2. 鼠标输入
- **鼠标位置追踪**：屏幕空间坐标
- **鼠标移动增量**：每帧的鼠标移动量
- **鼠标按键状态**：支持5个鼠标按键（左、右、中、X1、X2）
- **鼠标滚轮**：支持垂直和水平滚动（Vector2）

### 3. 输入查询 API

```cpp
// 键盘查询
bool IsKeyDown(KeyCode key);          // 当前帧按键是否按下
bool IsKeyUp(KeyCode key);            // 当前帧按键是否松开
bool IsKeyPressed(KeyCode key);       // 本帧刚按下（边沿触发）
bool IsKeyReleased(KeyCode key);      // 本帧刚释放（边沿触发）

// 鼠标按键查询
bool IsMouseButtonDown(MouseButton button);
bool IsMouseButtonUp(MouseButton button);
bool IsMouseButtonPressed(MouseButton button);
bool IsMouseButtonReleased(MouseButton button);

// 鼠标位置和移动
Vector2 GetMousePosition();           // 当前鼠标位置（屏幕坐标）
Vector2 GetMouseDelta();              // 鼠标移动增量
Vector2 GetMouseScrollDelta();        // 滚轮滚动增量（x=水平, y=垂直）
```

## 使用示例 (Usage Example)

### 基本键盘输入
```cpp
#include "Input/InputSystem.h"
#include "Input/KeyCodes.h"

// 初始化输入系统
InputSystem* input = new InputSystem();

// 游戏循环
while (running) {
    // 更新输入状态（必须每帧调用）
    input->Update();
    
    // 检查按键是否按下（持续检测）
    if (input->IsKeyDown(KeyCode::W)) {
        player->MoveForward(deltaTime);
    }
    
    // 检查按键是否刚按下（单次触发）
    if (input->IsKeyPressed(KeyCode::Space)) {
        player->Jump();
    }
    
    // 检查按键组合
    if (input->IsKeyDown(KeyCode::LeftControl) && 
        input->IsKeyPressed(KeyCode::S)) {
        SaveGame();
    }
}
```

### 鼠标输入
```cpp
// 检查鼠标按键
if (input->IsMouseButtonPressed(MouseButton::Left)) {
    Vector2 pos = input->GetMousePosition();
    HandleClick(pos.x, pos.y);
}

// 获取鼠标移动
Vector2 delta = input->GetMouseDelta();
if (delta.x != 0.0f || delta.y != 0.0f) {
    camera->Rotate(delta.x, delta.y);
}

// 处理鼠标滚轮
Vector2 scroll = input->GetMouseScrollDelta();
if (scroll.y != 0.0f) {
    camera->Zoom(scroll.y);
}
```

### 与相机控制器集成
```cpp
#include "Camera/FPSCameraController.h"
#include "Input/InputSystem.h"

InputSystem* input = new InputSystem();
PerspectiveCamera* camera = new PerspectiveCamera();
FPSCameraController* controller = new FPSCameraController(camera, input);

// 游戏循环
while (running) {
    input->Update();
    controller->Update(deltaTime);
    
    // 相机控制器会自动使用输入系统
    // WASD 移动，鼠标右键旋转视角
}
```

### 高级用法：输入映射
```cpp
// 自定义输入映射
class InputMapper {
public:
    enum class Action {
        MoveForward, MoveBack, MoveLeft, MoveRight,
        Jump, Crouch, Sprint, Fire
    };
    
    bool IsActionActive(IInputSystem* input, Action action) {
        switch (action) {
            case Action::MoveForward: 
                return input->IsKeyDown(KeyCode::W);
            case Action::Jump: 
                return input->IsKeyPressed(KeyCode::Space);
            case Action::Fire:
                return input->IsMouseButtonDown(MouseButton::Left);
            // ... 其他映射
        }
        return false;
    }
};

// 使用
InputMapper mapper;
if (mapper.IsActionActive(input, InputMapper::Action::Jump)) {
    player->Jump();
}
```

## 键盘码定义 (KeyCode Enum)

常用按键：
```cpp
// 字母键：A-Z (0x41-0x5A)
KeyCode::A, KeyCode::B, ..., KeyCode::Z

// 数字键：0-9 (0x30-0x39)
KeyCode::D0, KeyCode::D1, ..., KeyCode::D9

// 功能键：F1-F12 (0x70-0x7B)
KeyCode::F1, KeyCode::F2, ..., KeyCode::F12

// 方向键
KeyCode::Left, KeyCode::Up, KeyCode::Right, KeyCode::Down

// 修饰键
KeyCode::LeftShift, KeyCode::RightShift
KeyCode::LeftControl, KeyCode::RightControl
KeyCode::LeftAlt, KeyCode::RightAlt

// 特殊键
KeyCode::Space, KeyCode::Enter, KeyCode::Escape
KeyCode::Tab, KeyCode::Backspace, KeyCode::Delete
KeyCode::Home, KeyCode::End, KeyCode::PageUp, KeyCode::PageDown

// 小键盘
KeyCode::Numpad0-9, KeyCode::NumpadAdd, KeyCode::NumpadSubtract
```

## 鼠标按键定义 (MouseButton Enum)

```cpp
MouseButton::Left      // 左键
MouseButton::Right     // 右键
MouseButton::Middle    // 中键
MouseButton::X1        // 侧键1
MouseButton::X2        // 侧键2
```

## API 参考 (API Reference)

### IInputSystem 接口

```cpp
class IInputSystem {
public:
    virtual ~IInputSystem() = default;
    
    // 键盘输入
    virtual bool IsKeyDown(KeyCode key) const = 0;
    virtual bool IsKeyUp(KeyCode key) const = 0;
    virtual bool IsKeyPressed(KeyCode key) const = 0;
    virtual bool IsKeyReleased(KeyCode key) const = 0;
    
    // 鼠标按键
    virtual bool IsMouseButtonDown(MouseButton button) const = 0;
    virtual bool IsMouseButtonUp(MouseButton button) const = 0;
    virtual bool IsMouseButtonPressed(MouseButton button) const = 0;
    virtual bool IsMouseButtonReleased(MouseButton button) const = 0;
    
    // 鼠标位置和移动
    virtual Vector2 GetMousePosition() const = 0;
    virtual Vector2 GetMouseDelta() const = 0;
    virtual Vector2 GetMouseScrollDelta() const = 0;
    
    // 更新输入状态
    virtual void Update() = 0;
};
```

### InputSystem 实现

```cpp
class InputSystem : public IInputSystem {
public:
    InputSystem();
    ~InputSystem() override = default;
    
    // 实现所有 IInputSystem 接口方法
    // ...
    
    void Update() override;  // 必须每帧调用
};
```

## 设计原则 (Design Principles)

1. **平台无关接口**: `IInputSystem` 抽象平台细节
2. **双缓冲状态**: 使用当前帧和前一帧状态比较检测输入事件
3. **低延迟**: 直接使用 Win32 API 的 `GetAsyncKeyState` 获取实时状态
4. **无阻塞**: 轮询式输入，不会阻塞主线程
5. **可扩展**: 易于添加新的输入设备支持

## 实现细节 (Implementation Details)

### 状态管理
```cpp
// 双缓冲状态
std::unordered_set<int> m_currentKeys;      // 当前帧按下的键
std::unordered_set<int> m_previousKeys;     // 前一帧按下的键

// 边沿检测
bool IsKeyPressed(KeyCode key) const {
    // 当前帧按下 && 前一帧未按下 = 刚按下
    return m_currentKeys.count(key) && !m_previousKeys.count(key);
}
```

### Windows 实现
```cpp
bool InputSystem::IsKeyDown(KeyCode key) const {
#ifdef _WIN32
    return (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
#else
    return false;
#endif
}
```

## 最佳实践 (Best Practices)

1. **每帧调用 Update()**: 必须在游戏循环开始时调用，更新输入状态
2. **使用 IsKeyPressed 处理单次事件**: 如跳跃、射击、菜单切换
3. **使用 IsKeyDown 处理持续动作**: 如移动、瞄准
4. **检查组合键顺序**: 先检查修饰键（Ctrl、Shift）再检查主键
5. **保存鼠标增量**: 如果需要累积鼠标移动，自行保存增量值
6. **归零滚轮**: 每帧滚轮增量会自动归零

## 性能优化 (Performance)

- ✅ 最小内存占用：只存储按下的键（unordered_set）
- ✅ O(1) 查询：哈希表快速查找
- ✅ 无内存分配：Update() 中使用 swap 交换缓冲区
- ✅ 实时响应：直接使用 GetAsyncKeyState 获取最新状态

## 平台支持 (Platform Support)

### 当前支持
- ✅ Windows (Win32 API)

### 未来支持
- [ ] Linux (X11/Wayland)
- [ ] macOS (Cocoa)
- [ ] 游戏手柄支持
- [ ] 触摸输入支持

## 局限性 (Limitations)

1. **鼠标滚轮**: 当前实现每帧归零，需要 Windows 消息处理才能正确更新
2. **屏幕坐标**: 鼠标位置是全局屏幕坐标，不是窗口内坐标
3. **输入法**: 不支持 IME（输入法）输入
4. **文本输入**: 需要单独的文本输入系统

## 待实现功能 (Future Features)

- [ ] 游戏手柄/控制器支持
- [ ] 触摸输入支持（多点触控）
- [ ] 输入录制与回放
- [ ] 自定义按键绑定系统
- [ ] 输入配置文件（JSON）
- [ ] 输入事件回调系统
- [ ] 死区处理（摇杆）
- [ ] 输入平滑/过滤

## 常见问题 (FAQ)

**Q: 为什么 GetMouseScrollDelta() 返回 Vector2？**  
A: 为了支持水平滚轮（如触控板、特殊鼠标），y 是垂直滚动，x 是水平滚动。

**Q: 如何获取窗口内的鼠标坐标？**  
A: 需要使用 `ScreenToClient()` 转换，或在窗口消息处理中获取。

**Q: IsKeyPressed 和 IsKeyDown 有什么区别？**  
A: `IsKeyPressed` 只在按键刚按下的那一帧返回 true（单次触发），`IsKeyDown` 在按键保持按下时持续返回 true。

**Q: 为什么鼠标滚轮不工作？**  
A: 当前实现需要 Windows 消息循环中处理 WM_MOUSEWHEEL 消息并更新滚轮状态。

**Q: 如何实现按键组合？**  
A: 使用逻辑与操作：`if (input->IsKeyDown(Ctrl) && input->IsKeyPressed(S)) SaveGame();`

## 使用注意事项 (Important Notes)

⚠️ **必须每帧调用 Update()**  
如果忘记调用 `Update()`，输入状态将不会更新，导致 `IsKeyPressed()` 和 `IsKeyReleased()` 失效。

⚠️ **鼠标滚轮需要消息处理**  
当前实现的滚轮功能需要在 Windows 消息循环中处理并设置滚轮值。

⚠️ **全局坐标系**  
`GetMousePosition()` 返回全局屏幕坐标，需要转换为窗口坐标才能用于游戏逻辑。

## 示例项目 (Examples)

查看以下文件获取完整示例：
- `engine/samples/hello_win32.cpp` - 基本输入使用
- `engine/core/Camera/FPSCameraController.cpp` - FPS 控制器实现
- `engine/core/Camera/OrbitCameraController.cpp` - 轨道控制器实现

## 相关文档
- [相机系统文档](engine-core-camera.md)
- [引擎核心文档](engine-core.md)