# Mesh Module

## 概述 (Overview)

Mesh 模块提供基础的几何网格数据结构，用于存储和管理可渲染的几何体。

## 设计原则 (Design Principles)

### 当前阶段：简化版
- ✅ 仅支持**位置 + 颜色**顶点格式
- ✅ 三角形列表（indexed triangles）
- ✅ 轻量级设计，易于理解和使用
- ✅ 为未来扩展预留接口

### 未来扩展
- 🔲 法线（Normal）- 用于光照计算
- 🔲 纹理坐标（UV）- 用于纹理映射
- 🔲 切线（Tangent）- 用于法线贴图
- 🔲 骨骼权重（Bone Weights）- 用于蒙皮动画
- 🔲 多材质支持（Sub-Mesh）

## 文件结构 (File Structure)

```
engine/core/Mesh/
├── Mesh.h              - Mesh 类和 Vertex 结构定义
├── Mesh.cpp            - 辅助函数实现（CreateCubeMesh等）
└── README.md           - 本文档
```

## 核心类 (Core Classes)

### `Vertex` 结构体
```cpp
struct Vertex {
    Vector3 position;                          // 顶点位置 (12 bytes)
    float colorR, colorG, colorB, colorA;      // 顶点颜色 (RGBA, 16 bytes)
};
```

**内存布局**（与 DiligentRenderer 着色器匹配）：
- Offset 0-11: Position (3 × float32)
- Offset 12-27: Color (4 × float32)
- Total: 28 bytes/vertex

### `Mesh` 类
存储几何网格数据：
- 顶点数组（`std::vector<Vertex>`）
- 索引数组（`std::vector<uint32_t>`）

**关键方法**：
- `SetVertices()` - 设置顶点数据
- `SetIndices()` - 设置索引数据
- `GetVertices()` / `GetIndices()` - 只读访问
- `IsValid()` - 检查网格有效性
- `Clear()` - 清空数据

## 使用示例 (Usage Examples)

### 创建立方体
```cpp
#include "Mesh/Mesh.h"

// 使用辅助函数创建
Moon::Mesh* cubeMesh = Moon::CreateCubeMesh(1.0f);

// 检查有效性
if (cubeMesh->IsValid()) {
    printf("顶点数: %zu, 三角形数: %zu\n", 
           cubeMesh->GetVertexCount(), 
           cubeMesh->GetTriangleCount());
}
```

### 手动创建 Mesh
```cpp
Moon::Mesh* mesh = new Moon::Mesh();

std::vector<Moon::Vertex> vertices = {
    { {-1, -1, 0}, {1, 0, 0} },  // 红色
    { { 1, -1, 0}, {0, 1, 0} },  // 绿色
    { { 0,  1, 0}, {0, 0, 1} }   // 蓝色
};

std::vector<uint32_t> indices = { 0, 1, 2 };

mesh->SetVertices(vertices);
mesh->SetIndices(indices);
```

### 在 MeshRenderer 中使用
```cpp
// 创建场景节点
Moon::SceneNode* node = scene->CreateNode("Cube");

// 添加 MeshRenderer 组件
Moon::MeshRenderer* renderer = node->AddComponent<Moon::MeshRenderer>();

// 设置 Mesh
Moon::Mesh* mesh = Moon::CreateCubeMesh(1.0f);
renderer->SetMesh(mesh);
```

## 内存管理 (Memory Management)

### 当前策略：手动管理
- `CreateCubeMesh()` 返回 `new Mesh*`，需要手动 `delete`
- `MeshRenderer` 不拥有 Mesh，只持有指针

### 未来优化
- 🔲 使用 `std::shared_ptr<Mesh>` 实现自动引用计数
- 🔲 MeshManager 统一管理 Mesh 生命周期
- 🔲 资源缓存和复用

## 性能考虑 (Performance Notes)

### 顶点格式
当前顶点大小：
- `position`: 12 bytes (3 * float)
- `colorR, colorG, colorB, colorA`: 16 bytes (4 * float)
- **总计**: 28 bytes/vertex

立方体数据量：
- 24 vertices × 28 bytes = **672 bytes**
- 36 indices × 4 bytes = **144 bytes**
- **总计**: 816 bytes

### 渲染效率
- ✅ 使用索引绘制（减少顶点重复）
- ✅ 顶点数据紧凑（无填充）
- 🔲 未来：实例化渲染（多个相同 Mesh）

## 与其他模块的关系 (Module Dependencies)

```
Mesh (core/Mesh)
  ↓ 被引用
MeshRenderer (core/Scene)
  ↓ 传递给
IRenderer (render)
  ↓ 实现
DiligentRenderer (render)
```

## 扩展路线图 (Roadmap)

### Phase 1: 基础（当前）✅
- 位置 + 颜色顶点
- 索引三角形
- 立方体辅助函数

### Phase 2: 常用图元
- `CreateSphereMesh()` - 球体
- `CreatePlaneMesh()` - 平面
- `CreateCylinderMesh()` - 圆柱

### Phase 3: 高级特性
- 法线 + UV 支持
- OBJ/GLTF 文件加载
- MeshManager 资源管理

### Phase 4: CSG 集成
- `CSGMesh` 类（Half-Edge 数据结构）
- Boolean 运算（Union/Subtract/Intersect）
- CSG → Mesh 转换

## 参考资料 (References)
- [OpenGL Tutorial - VBO](https://www.opengl-tutorial.org/beginners-tutorials/tutorial-2-the-first-triangle/)
- [DirectX - Vertex Buffers](https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-buffers-vertex-how-to)
- [Real-Time Rendering - Geometry Representation](http://www.realtimerendering.com/)
