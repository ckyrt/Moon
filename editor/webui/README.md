# Editor WebUI Module

## 职责 (Responsibilities)
- React/Vue 用户界面
- UI 布局和主题
- Inspector (属性面板)
- Hierarchy (层级面板)
- 菜单栏和工具栏
- 不含业务逻辑，只调用后端 IPC

## 文件结构 (File Structure)
- `src/` - 源代码目录
- `public/` - 静态资源
- `package.json` - 依赖管理
- `webpack.config.js` - 构建配置

## 依赖 (Dependencies)
- React 或 Vue.js
- WebSocket 客户端
- UI组件库 (Ant Design/Element UI)

## AI 生成指导
这个模块需要实现：
1. 现代化的Web界面
2. 与后端的IPC通信
3. 响应式布局设计
4. 组件化的UI架构