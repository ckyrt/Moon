# MVP 功能路线图（执行计划）

> **与 [MVP 定义](MVP_DEFINITION.md) 配套的详细实施计划**  
> 专注于核心功能的逐步实现，每个 Phase 都有明确的交付物

---

## Phase 0：基础内核（你自己用）

**目标**: 把"造东西"这件事跑通

### 几何 / 内核
- [ ] 参数化 Box / Extrude
- [ ] Manifold CSG（Difference）
- [ ] 参数 → mesh → CSG → mesh 重建流程
- [ ] 稳定缓存机制（key = 参数）

### 编辑器（内部 Authoring）
- [ ] Primitive 创建 UI
- [ ] 参数 Inspector
- [ ] 结构性切割（开洞）
- [ ] 手动组合 / 对齐
- [ ] 封装为 Asset（Prefab）

**输出结果**:
- ✅ SimpleWall
- ✅ SimpleDoor（洞 + 板）
- ✅ SimpleWindow（洞 + 玻璃）

**完成标志**: 你自己能用 CSG 创建基础构件并封装成资产

---

## Phase 1：Asset Builder 闭环

**目标**: 让"资产能被生产和复用"

### 资产系统
- [ ] Asset 数据结构（参数 + 规则）
- [ ] 参数白名单（暴露/隐藏）
- [ ] 本地资产库
- [ ] 上传服务器
- [ ] 下载 & 实例化

### Create Asset 模式
- [ ] 独立 UI Workspace
- [ ] Asset 封装向导
- [ ] 缩略图生成
- [ ] 分类（门 / 窗 / 构件）

**输出结果**:
- 本地资产库包含 10+ 基础构件
- Asset Builder 能创建新资产并上传

**完成标志**: L2 用户（Asset Builder）能完成完整的"创建-封装-上传"流程

---

## Phase 2：Builder MVP（对外可用）

**目标**: 普通用户"不会建模也能用"

### Build Space 模式
- [ ] 资产库浏览
- [ ] 放置 / 移动 / 旋转 / 缩放
- [ ] Snap / 对齐
- [ ] 门窗自动吸附
- [ ] 参数调节（尺寸）

### 空间系统
- [ ] Space 创建 / 复制
- [ ] 层级管理
- [ ] 一键进入（Walk Mode）

**输出结果**:
- 普通用户 10 分钟内搭建出简单房间
- 能放置门窗并自动吸附到墙上

**完成标志**: L1 用户（Builder）不接触任何几何操作即可创建空间

**里程碑**: 🎉 达成 [MVP 定义](MVP_DEFINITION.md) 中的成功标准

---

## Phase 3：材质 & 老化系统

**目标**: 解决"真实感 / 积木感"问题

### Base Material v1
- [ ] Metal / Wood / Concrete / Glass / Plastic
- [ ] PBR 参数控制

### Aging System
- [ ] Edge Wear（边缘磨损）
- [ ] Dirt / Vertical Gradient（污垢/渐变）
- [ ] Rust / Crack（锈迹/裂纹，材质专属）
- [ ] 风格限制（Clamp）

### Style Pack v1
- [ ] Modern Clean（现代简洁）
- [ ] Industrial（工业风）
- [ ] Cozy Home（温馨家居）

**输出结果**:
- 同一个房间可以一键切换风格
- 新旧程度可调节

**完成标志**: 创作结果有真实感，不再是"乐高积木"

---

## Phase 4：发布 & 世界入口

**目标**: 从"编辑器"变成"平台"

### 发布系统
- [ ] Space 发布
- [ ] 入口卡片生成
- [ ] Discover 列表

### 进入体验
- [ ] 多人进入（后期）
- [ ] 只读 / 可编辑权限

**输出结果**:
- 用户 A 创建的空间，用户 B 可以进入体验
- 优秀作品在 Discover 页面展示

**完成标志**: "统一世界"概念的初步验证

---

## Phase 5：治理与扩展（后期）

**目标**: 平台化运营准备

- [ ] 权限系统
- [ ] 举报 / 下架
- [ ] 性能预算提示
- [ ] 高级创作者计划
- [ ] 有限外部导入（可选）

**输出结果**:
- 平台内容质量可控
- 性能有保障

**完成标志**: 平台可以对外开放注册

---

## 📊 当前进度

| Phase | 状态 | 预计工期 | 关键交付物 |
|-------|------|----------|-----------|
| Phase 0 | 🔥 进行中 | 2-3 周 | SimpleWall/Door/Window |
| Phase 1 | ⏳ 未开始 | 3-4 周 | 资产系统 + Create Asset 模式 |
| Phase 2 | ⏳ 未开始 | 4-6 周 | Build Space 模式 + MVP |
| Phase 3 | ⏳ 未开始 | 3-4 周 | 材质系统 + 风格包 |
| Phase 4 | ⏳ 未开始 | 2-3 周 | 发布系统 |
| Phase 5 | ⏳ 未开始 | 待定 | 治理系统 |

**总预计**: 4-5 个月完成 MVP（Phase 0-2）

---

## 🎯 当前重点（Phase 0）

### 本周任务
- [ ] 完成 Manifold CSG 集成
- [ ] 实现参数化 Box 生成
- [ ] 实现 Difference 操作（开洞）

### 下周任务
- [ ] 构建 SimpleWall
- [ ] 构建 SimpleDoor（墙 + 洞 + 门板）
- [ ] 测试完整流程

---

## 📚 相关文档

- [MVP 定义](MVP_DEFINITION.md) - 产品目标和边界
- [完整路线图](ROADMAP.md) - 长期技术规划
- [产品愿景](VISION.md) - 5年愿景
- [架构文档](ARCHITECTURE.md) - 技术架构

---

**最后更新：2026-01-06**
