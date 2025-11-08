# Scene System Module

## 职责 (Responsibilities)
- 场景图 (Scene Graph) 管理
- 场景节点 (SceneNode / GameObject) 生命周期
- 变换系统 (Transform) - 位置/旋转/缩放
- 组件系统 (Component System) 基础
- 父子层级关系 (Parent-Child Hierarchy)
- 场景遍历和更新

## 核心类 (Core Classes)

### Component (组件基类)
- 所有组件的基类
- 提供 Enable/Disable 状态管理
- 提供 Update 生命周期回调

### Transform (变换组件)
- 本地坐标系变换 (Local Transform)
- 世界坐标系变换 (World Transform)
- 位置 (Position)、旋转 (Rotation)、缩放 (Scale)
- 父子层级变换传递
- 矩阵缓存和惰性更新 (Lazy Update)

### SceneNode (场景节点)
- 场景中的实体对象（类似 Unity 的 GameObject）
- 唯一 ID 和名称
- 父子层级关系管理
- 组件容器
- Active 状态管理

### Scene (场景管理器)
- 管理所有顶层节点
- 节点创建和销毁
- 场景更新循环
- 场景遍历

## 依赖 (Dependencies)
- `Camera/Camera.h` - 使用 Vector3, Matrix4x4 数学类型
- `Logging/Logger.h` - 日志输出

## 使用示例 (Usage Example)

### 创建场景和节点
```cpp
#include "Scene/Scene.h"
#include "Scene/SceneNode.h"

// 创建场景
Moon::Scene* scene = new Moon::Scene("Main Scene");

// 创建根节点
Moon::SceneNode* root = scene->CreateNode("Root");
root->GetTransform()->SetLocalPosition(Moon::Vector3(0, 0, 0));

// 创建子节点
Moon::SceneNode* child = scene->CreateNode("Child");
child->SetParent(root);
child->GetTransform()->SetLocalPosition(Moon::Vector3(2, 0, 0));
child->GetTransform()->SetLocalRotation(Moon::Vector3(0, 45, 0));
```

### 场景更新和遍历
```cpp
// 每帧更新
scene->Update(deltaTime);

// 遍历所有节点
scene->Traverse([](Moon::SceneNode* node) {
    Moon::Matrix4x4 worldMatrix = node->GetTransform()->GetWorldMatrix();
    // 使用世界矩阵进行渲染...
});
```

### 层级关系
```cpp
// 创建层级结构
Moon::SceneNode* parent = scene->CreateNode("Parent");
Moon::SceneNode* child1 = scene->CreateNode("Child1");
Moon::SceneNode* child2 = scene->CreateNode("Child2");

child1->SetParent(parent);
child2->SetParent(parent);

// 父节点移动，子节点自动跟随
parent->GetTransform()->SetLocalPosition(Moon::Vector3(5, 0, 0));
```

## 坐标系约定 (Coordinate System)
- 使用**左手坐标系** (Left-Handed)
- +Y 向上，+Z 向前，+X 向右
- Euler 角度使用度数 (Degrees)
- 矩阵使用行主序 (Row-Major)

详见: [ADR 0005](../../../docs/adr/0005-coordinate-system-and-matrix-conventions.md)

## 性能优化 (Performance)
- **矩阵缓存**: Local 和 World 矩阵只在脏标记时重新计算
- **惰性更新**: 使用 Dirty Flag 机制避免不必要的计算
- **层级传播**: 父节点变换时自动标记所有子节点为脏

## 未来扩展 (Future)
- 场景序列化/反序列化 (Save/Load)
- 预制体系统 (Prefab System)
- 图层和标签系统 (Layer & Tags)
- 完整 ECS 架构 (Entity Component System)
- 场景事件系统 (Scene Events)
