# Editor Bridge Module

## 职责 (Responsibilities)
- C++ ↔ WebUI 通信层
- WebSocket 服务器
- CEF 或 WebView2 集成
- 消息路由和转发
- 协议版本管理

## 接口文件 (Interface Files)
- `NativeHost.h` - 原生主机接口
- `WebSocketServer.h` - WebSocket服务器
- `MessageRouter.h` - 消息路由器
- `IPCBridge.h` - IPC桥接器

## 依赖 (Dependencies)
- WebSocket库 (websocketpp 或类似)
- engine/contracts (IPC协议)
- engine/adapters (平台集成)

## AI 生成指导
这个模块需要实现：
1. WebSocket服务器
2. 消息的序列化和反序列化
3. Web视图的嵌入和管理
4. 双向通信的建立