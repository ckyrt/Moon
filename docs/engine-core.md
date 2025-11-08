# Engine Core Module

## 职责 (Responsibilities)
- ECS (Entity Component System) 架构
- 组件注册和管理
- 事件系统 (Event System)
- 整体游戏循环 (Game Loop)
- 引擎逻辑状态的唯一真相 (Single Source of Truth)
- 服务器和客户端共用的核心逻辑

## 接口文件 (Interface Files)
- `IEngine.h` - 引擎核心接口
- `EngineCore.h/.cpp` - 主要实现

## 依赖 (Dependencies)
- 无外部依赖，是整个引擎的基础模块

## AI 生成指导
这个模块需要实现：
1. 基础的ECS系统
2. 游戏主循环
3. 事件分发机制
4. 组件注册系统