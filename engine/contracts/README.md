# Contracts Module

## 职责 (Responsibilities)
- IDL (Interface Definition Language) 类型定义
- UI ↔ 引擎 IPC 协议定义
- 网络协议定义
- API接口规范
- 版本兼容性定义

## 接口文件 (Interface Files)
- `IPC.proto` - IPC协议定义 (Protobuf)
- `GameAPI.h` - 游戏API接口
- `UICommands.h` - UI命令定义
- `NetworkProtocol.h` - 网络协议

## 依赖 (Dependencies)
- Protobuf 或 FlatBuffers (推荐)
- 无其他引擎依赖

## AI 生成指导
这个模块需要实现：
1. IPC消息的协议定义
2. 引擎API的接口规范
3. 网络消息格式
4. 协议的版本管理