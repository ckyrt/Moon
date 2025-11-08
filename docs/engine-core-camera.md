# Camera System Module

## 职责 (Responsibilities)
- 透视相机 (Perspective Camera)
- 正交相机 (Orthographic Camera)
- 相机控制器（FPS、轨道、自由视角）
- 视锥体计算
- 相机组件集成到 ECS
- 矩阵缓存优化（避免重复计算）

## 接口文件 (Interface Files)
- `Camera.h/.cpp` - 相机基类（包含 Vector3、Matrix4x4 数学类型）
- `PerspectiveCamera.h/.cpp` - 透视相机
- `OrthographicCamera.h/.cpp` - 正交相机
- `FPSCameraController.h/.cpp` - 第一人称相机控制器
- `OrbitCameraController.h/.cpp` - 轨道相机控制器

## 依赖 (Dependencies)
- engine/core/Input (输入系统)

## 核心功能 (Core Features)

### 1. 相机类型
- **透视相机 (Perspective)**：用于 3D 场景渲染，具有透视效果
- **正交相机 (Orthographic)**：用于 2D 或无透视效果的 3D 渲染（如CAD视图）

### 2. 相机参数
- 位置 (Position)
- 目标/朝向 (Target/Direction)
- 上方向 (Up Vector)
- 视野角 (FOV) - 仅透视相机
- 近裁剪面 (Near Plane)
- 远裁剪面 (Far Plane)
- 宽高比 (Aspect Ratio) - 仅透视相机

### 3. 相机矩阵
- **View Matrix**: 世界空间到相机空间的变换
- **Projection Matrix**: 相机空间到裁剪空间的变换
- **View-Projection Matrix**: 组合矩阵（优化：已缓存）

### 4. 相机控制器
- **FPS 控制器**: 
  - WASD 移动 + 鼠标右键视角控制
  - Shift 加速移动
  - Q/E 垂直移动
  
- **轨道控制器**: 
  - 鼠标中键/右键旋转视角
  - 鼠标滚轮缩放距离
  - Shift + 鼠标中键平移目标点

### 5. 性能优化
- **矩阵缓存**: View 和 Projection 矩阵只在参数变化时重新计算
- **惰性计算**: 使用脏标记 (dirty flag) 机制

## 坐标系约定 (Coordinate System)

根据 [ADR 0005](adr-0005-coordinate-system-and-matrix-conventions.md)：
- 使用**左手坐标系**
- +Y 向上，+Z 向前，+X 向右
- 使用 `LookAtLH` 和 `PerspectiveFovLH` 等左手系函数

## 使用示例 (Usage Example)

### 创建透视相机
```cpp
#include "Camera/PerspectiveCamera.h"

// 创建透视相机
PerspectiveCamera* camera = new PerspectiveCamera();
camera->SetPosition(Vector3(0.0f, 5.0f, -10.0f));
camera->SetTarget(Vector3(0.0f, 0.0f, 0.0f));
camera->SetFOV(45.0f);
camera->SetAspectRatio(16.0f / 9.0f);
camera->SetNearFar(0.1f, 1000.0f);

// 获取矩阵（自动缓存，性能优化）
Matrix4x4 view = camera->GetViewMatrix();
Matrix4x4 proj = camera->GetProjectionMatrix();
Matrix4x4 viewProj = camera->GetViewProjectionMatrix();
```

### 使用 FPS 控制器
```cpp
#include "Camera/FPSCameraController.h"
#include "Input/InputSystem.h"

// 创建输入系统
InputSystem* inputSystem = new InputSystem();

// 创建相机和控制器
PerspectiveCamera* camera = new PerspectiveCamera();
FPSCameraController* controller = new FPSCameraController(camera, inputSystem);

// 设置移动速度和灵敏度
controller->SetMoveSpeed(5.0f);
controller->SetSprintMultiplier(2.0f);
controller->SetMouseSensitivity(0.002f);

// 游戏循环
while (running) {
    inputSystem->Update();
    controller->Update(deltaTime);
    
    // 渲染场景...
    Matrix4x4 viewProj = camera->GetViewProjectionMatrix();
}
```

### 使用轨道控制器
```cpp
#include "Camera/OrbitCameraController.h"

// 创建相机和轨道控制器
PerspectiveCamera* camera = new PerspectiveCamera();
OrbitCameraController* controller = new OrbitCameraController(camera, inputSystem);

// 设置目标和距离
controller->SetTarget(Vector3(0.0f, 0.0f, 0.0f));
controller->SetDistance(10.0f);
controller->SetDistanceRange(2.0f, 50.0f);

// 设置灵敏度
controller->SetRotationSensitivity(0.2f);
controller->SetZoomSensitivity(1.0f);
controller->SetPanSensitivity(0.01f);

// 每帧更新
while (running) {
    inputSystem->Update();
    controller->Update(deltaTime);
}
```

### 创建正交相机
```cpp
#include "Camera/OrthographicCamera.h"

// 创建正交相机（用于2D或CAD视图）
OrthographicCamera* orthoCamera = new OrthographicCamera();
orthoCamera->SetPosition(Vector3(0.0f, 10.0f, 0.0f));
orthoCamera->SetTarget(Vector3(0.0f, 0.0f, 0.0f));
orthoCamera->SetWidth(20.0f);
orthoCamera->SetHeight(15.0f);
orthoCamera->SetNearFar(0.1f, 100.0f);
```

### LookAt 辅助方法
```cpp
// 快速设置相机朝向某个点
camera->SetPosition(Vector3(10.0f, 5.0f, 10.0f));
camera->LookAt(Vector3(0.0f, 0.0f, 0.0f));  // 相机朝向原点
```

## 数学库 (Math Library)

相机系统内置以下数学类型：
- `Vector3` - 3D 向量（位置、方向）
  - 支持加减乘除、长度计算、归一化
  - 支持点积 (Dot) 和叉积 (Cross)
  
- `Matrix4x4` - 4x4 矩阵
  - 支持矩阵乘法
  - 提供 LookAtLH、PerspectiveFovLH、OrthoLH 等静态方法

## API 参考 (API Reference)

### Camera 基类

```cpp
// 设置相机参数
void SetPosition(const Vector3& p);
void SetTarget(const Vector3& t);
void SetUp(const Vector3& u);
void LookAt(const Vector3& target);

// 获取相机参数
Vector3 GetPosition() const;
Vector3 GetTarget() const;
Vector3 GetUp() const;
Vector3 GetForward() const;
Vector3 GetRight() const;

// 获取矩阵（自动缓存）
Matrix4x4 GetViewMatrix() const;
Matrix4x4 GetProjectionMatrix() const;
Matrix4x4 GetViewProjectionMatrix() const;
```

### PerspectiveCamera

```cpp
// 构造函数
PerspectiveCamera(float fov=60, float aspect=16.0f/9.0f, 
                  float near=0.1f, float far=1000.0f);

// 设置投影参数（自动标记为脏）
void SetFOV(float fov);
void SetAspectRatio(float aspect);
void SetNearPlane(float near);
void SetFarPlane(float far);
void SetNearFar(float near, float far);

// 获取投影参数
float GetFOV() const;
float GetAspectRatio() const;
float GetNearPlane() const;
float GetFarPlane() const;
```

### FPSCameraController

```cpp
// 构造函数
FPSCameraController(Camera* camera, IInputSystem* input);

// 更新（每帧调用）
void Update(float deltaTime);

// 设置参数
void SetMoveSpeed(float speed);
void SetSprintMultiplier(float multiplier);
void SetMouseSensitivity(float sensitivity);
void SetEnabled(bool enabled);

// 控制方式：
// WASD - 前后左右移动
// Q/E - 垂直移动
// Shift - 加速移动
// 鼠标右键 + 移动 - 视角旋转
```

### OrbitCameraController

```cpp
// 构造函数
OrbitCameraController(Camera* camera, IInputSystem* input);

// 更新（每帧调用）
void Update(float deltaTime);

// 设置参数
void SetTarget(const Vector3& target);
void SetDistance(float distance);
void SetDistanceRange(float min, float max);
void SetRotationSensitivity(float sensitivity);
void SetZoomSensitivity(float sensitivity);
void SetPanSensitivity(float sensitivity);
void SetEnabled(bool enabled);

// 控制方式：
// 鼠标中键/右键 - 旋转相机
// 鼠标滚轮 - 缩放距离
// Shift + 鼠标中键 - 平移目标点
```

## 最佳实践 (Best Practices)

1. **使用缓存机制**: 矩阵会自动缓存，无需手动管理
2. **批量设置参数**: 使用 `SetNearFar()` 而不是分别调用 `SetNearPlane()` 和 `SetFarPlane()`
3. **合理设置裁剪面**: 避免 near 太小或 far 太大，会降低深度精度
4. **相机控制器**: 根据场景需求选择合适的控制器（FPS 或 Orbit）
5. **输入系统**: 确保每帧调用 `inputSystem->Update()` 更新输入状态

## 性能优化 (Performance)

- ✅ 矩阵缓存：避免重复计算 View 和 Projection 矩阵
- ✅ 脏标记：只在参数变化时重新计算
- ✅ 内联函数：简单的 getter 函数都是内联的
- ✅ 最小化内存分配：所有数据都在栈上

## 平台支持 (Platform Support)
- Windows（已测试）
- 跨平台数学库实现
- 兼容 Diligent Engine 的矩阵约定

## 待实现功能 (Future Features)
- [ ] 相机动画和平滑插值
- [ ] 多相机支持（画中画、分屏）
- [ ] 相机碰撞检测
- [ ] 相机抖动效果
- [ ] 视锥体裁剪查询
- [ ] 相机序列化/反序列化

## 常见问题 (FAQ)

**Q: 为什么使用左手坐标系？**  
A: 为了与 DirectX 和 Diligent Engine 保持一致，便于渲染后端集成。

**Q: 如何切换相机类型？**  
A: 创建新的相机实例，并将控制器重新绑定到新相机。

**Q: 相机矩阵何时会重新计算？**  
A: 只有在相机参数（位置、目标、FOV等）发生变化时才会重新计算。

**Q: 如何实现相机平滑移动？**  
A: 可以使用插值函数（lerp）在当前位置和目标位置之间进行平滑过渡。

## 相关文档
- [架构决策记录 - 坐标系和矩阵约定](adr-0005-coordinate-system-and-matrix-conventions.md)
- [输入系统文档](engine-core-input.md)