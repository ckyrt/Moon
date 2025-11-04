# Character Module

## 职责 (Responsibilities)
- 角色动画播放系统
- 骨骼 (Skeleton) 和蒙皮 (Skinning)
- 捏人参数系统 (Character Customization)
- 角色控制器 (Character Controller)
- 动画状态机

## 接口文件 (Interface Files)
- `ICharacter.h` - 角色系统接口
- `Character.h` - 角色组件
- `Skeleton.h` - 骨骼组件
- `Animation.h` - 动画组件
- `CharacterController.h` - 角色控制器

## 依赖 (Dependencies)
- engine/physics (角色物理)
- engine/core (ECS系统)
- engine/render (角色渲染)

## AI 生成指导
这个模块需要实现：
1. 基础的骨骼动画系统
2. 角色的物理控制器
3. 简单的动画状态机
4. 角色外观定制接口