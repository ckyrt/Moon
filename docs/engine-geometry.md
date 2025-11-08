# Geometry Module

## 职责 (Responsibilities)
- CSG (Constructive Solid Geometry) 布尔运算
- Extrude (拉伸) 操作
- Sweep (扫掠) 操作
- 网格生成和简化
- 对第三方几何库的封装 (fast-csg/libigl/carve)

## 接口文件 (Interface Files)
- `IGeometry.h` - 几何操作接口
- `CSGOperations.h` - CSG布尔运算
- `MeshGenerator.h` - 网格生成器

## 依赖 (Dependencies)
- 第三方几何库 (可选: fast-csg, libigl, carve)
- engine/core (基础类型)

## AI 生成指导
这个模块需要实现：
1. 基础几何图元 (立方体、球体、圆柱等)
2. CSG布尔运算 (并集、交集、差集)
3. 网格生成和优化算法
4. 几何变换操作