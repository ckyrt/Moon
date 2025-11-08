# Building Module

## 职责 (Responsibilities)
- 建筑元素的参数化生成 (墙、门、窗、楼梯、地板)
- 建筑结构的CSG布尔运算
- 建筑模板和预制件系统
- 建筑物理属性 (材质、厚度等)

## 接口文件 (Interface Files)
- `IBuilding.h` - 建筑系统接口
- `Wall.h` - 墙体组件
- `Door.h` - 门组件
- `Window.h` - 窗户组件
- `Floor.h` - 地板组件
- `Stair.h` - 楼梯组件

## 依赖 (Dependencies)
- engine/geometry (CSG布尔运算)
- engine/core (ECS系统)
- engine/physics (碰撞体生成)

## AI 生成指导
这个模块需要实现：
1. 参数化的建筑元素生成
2. 建筑元素的组装和约束
3. 与几何模块的集成
4. 建筑物的序列化和保存