# Persistence Module

## 职责 (Responsibilities)
- 场景保存和加载
- 序列化系统 (Serialization)
- 文件格式版本迁移
- 资产管理 (Asset Management)
- 备份和恢复机制

## 接口文件 (Interface Files)
- `IPersistence.h` - 持久化接口
- `Serializer.h` - 序列化器
- `AssetManager.h` - 资产管理器
- `SaveGame.h` - 存档系统

## 依赖 (Dependencies)
- engine/core (所有组件的序列化)
- engine/adapters (文件系统访问)

## AI 生成指导
这个模块需要实现：
1. ECS组件的序列化机制
2. 场景的保存和加载
3. 资产的引用和管理
4. 版本兼容性处理