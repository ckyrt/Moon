# Network Module

## 职责 (Responsibilities)
- 服务器/客户端同步
- CRDT (Conflict-free Replicated Data Types) 或指令队列
- 网络快照系统
- 多人游戏状态同步
- 网络安全和反作弊

## 接口文件 (Interface Files)
- `INetwork.h` - 网络接口
- `Server.h` - 服务器组件
- `Client.h` - 客户端组件
- `NetworkSync.h` - 同步系统
- `Snapshot.h` - 快照系统

## 依赖 (Dependencies)
- engine/core (状态同步)
- engine/contracts (网络协议)
- engine/adapters (网络IO)

## AI 生成指导
这个模块需要实现：
1. 基础的客户端-服务器架构
2. 状态同步机制
3. 网络消息的序列化
4. 简单的反作弊检测