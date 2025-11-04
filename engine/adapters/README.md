# Adapters Module

## 职责 (Responsibilities)
- 文件系统抽象
- 操作系统API封装
- 时间和计时器
- 平台特定API适配
- CEF/WebView2 Host (如果使用Native Host)

## 接口文件 (Interface Files)
- `IFileSystem.h` - 文件系统接口
- `ITimer.h` - 计时器接口
- `IPlatform.h` - 平台接口
- `WebViewHost.h` - WebView主机

## 依赖 (Dependencies)
- 操作系统API
- CEF 或 WebView2 (可选)
- 无其他引擎依赖

## AI 生成指导
这个模块需要实现：
1. 跨平台的文件系统访问
2. 高精度计时器
3. 平台特定功能的抽象
4. Web视图的集成 (如需要)