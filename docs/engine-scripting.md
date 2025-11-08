# Scripting Module

## 职责 (Responsibilities)
- 脚本语言集成 (Lua/JavaScript/Python)
- 脚本API绑定
- 热重载支持
- 脚本调试接口
- 游戏逻辑脚本化

## 接口文件 (Interface Files)
- `IScripting.h` - 脚本系统接口
- `ScriptEngine.h` - 脚本引擎
- `ScriptAPI.h` - API绑定
- `ScriptComponent.h` - 脚本组件

## 依赖 (Dependencies)
- 脚本语言库 (Lua, V8, 或 Python)
- engine/core (ECS集成)
- engine/contracts (API定义)

## AI 生成指导
这个模块需要实现：
1. 脚本语言的集成
2. 引擎API的脚本绑定
3. 脚本组件的ECS集成
4. 脚本的热重载机制