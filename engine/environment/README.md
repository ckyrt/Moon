# Engine Environment

`engine/environment` 是 Moon 的独立自然环境模块骨架。

当前这一版先解决三件事：

- 定义统一的环境状态和配置结构
- 提供可独立更新的 `EnvironmentSystem`
- 提供场景挂载入口 `EnvironmentComponent`

## 当前边界

- 不直接修改现有 `render` 模块
- 不强制场景必须启用环境系统
- 不引入跨项目共享的全局环境单例

## 下一步建议

1. 在 `render` 中增加环境输入接口，读取 `EnvironmentState`
2. 在 `editor` 中增加时间、天气、风场调试面板
3. 把 `EnvironmentProfile` 接入资源系统和序列化
4. 再逐步接入天空、雾、云和植被表现
