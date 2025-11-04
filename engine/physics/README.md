# Physics Module

## 职责 (Responsibilities)
- 物理引擎适配层 (Jolt Physics/PhysX)
- 刚体 (Rigid Body) 管理
- 碰撞体 (Collider) 系统
- 射线查询 (Ray Casting) API
- 物理约束和关节

## 接口文件 (Interface Files)
- `IPhysics.h` - 物理系统接口
- `RigidBody.h` - 刚体组件
- `Collider.h` - 碰撞体组件
- `PhysicsWorld.h` - 物理世界管理

## 依赖 (Dependencies)
- 物理引擎库 (Jolt Physics 推荐, 或 PhysX)
- engine/core (ECS系统)
- engine/geometry (碰撞体几何)

## AI 生成指导
这个模块需要实现：
1. 物理引擎的抽象接口
2. 刚体和碰撞体的ECS组件
3. 物理世界的更新循环
4. 射线检测和碰撞查询