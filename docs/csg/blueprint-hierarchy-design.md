# CSG Blueprint 层级体系设计

> **Blueprint Hierarchy Design for Composable CSG System**  
> *从几何原子到完整世界的可生长结构树*

---

## 📋 核心理念

我们不是在"做几个模型"，而是在设计 **一套可以生长的物体层级体系**。

### 设计目标

不是列"物体清单"，而是构建：

> 🌱 **一棵可以无限长高的结构树**

通过底层基础组件的**递归组合**，生成高层复杂物体。

---

## 🌍 五层金字塔原则

| 层级 | 名称 | 特点 | 示例 |
|------|------|------|------|
| **Level 0** | 几何原子层 | 不可再分的数学几何 | Cube, Sphere, Cylinder |
| **Level 1** | 结构构件层 | 可复用的建筑块 | Beam, Wall Panel, Wheel Ring |
| **Level 2** | 功能单元层 | 家具部件/建筑功能件 | Table Base, Door Frame |
| **Level 3** | 生活物体层 | 完整的日常用品 | Table, Chair, Bicycle |
| **Level 4+** | 空间/场景层 | 房间、建筑、世界 | Room, House, City |

### 核心原则

✅ **高层永远由低层组合**  
✅ **优先做结构件 > 功能件 > 生活物体**  
✅ **不做具体物体，做组合语言**

---

## 🧱 Level 0 — 几何原子层 (Primitive Layer)

这些是**不可再分的数学几何单元**，必须极简。

### 基础几何体

| 几何体 | 参数 | 用途 |
|--------|------|------|
| `cube` | size / half_extents | 基础方块 |
| `sphere` | radius | 球形结构 |
| `cylinder` | radius, height | 圆柱、管道 |
| `capsule` | radius, height | 圆角柱体 |
| `cone` | radius, height | 锥形 |
| `torus` | major_radius, minor_radius | 环形（后期） |

**设计原则：** 这一层只提供数学几何，不涉及任何现实物体概念。

---

## 🧩 Level 1 — 结构构件层 (Structural Components)

这些不是生活物体，而是**可复用的建筑块**。

### 线性结构

| 构件 | 组成 | 参数 | 用途 |
|------|------|------|------|
| `beam` | Cylinder | length, thickness | 横梁 |
| `column` | Cylinder | height, radius | 柱子 |
| `rod` | Cylinder (thin) | length, radius | 杆 |
| `pipe` | Cylinder - Cylinder | length, outer_r, inner_r | 管道 |
| `plank` | Cube | length, width, thickness | 木板 |

### 面结构

| 构件 | 组成 | 参数 | 用途 |
|------|------|------|------|
| `wall_panel` | Cube (flat) | width, height, thickness | 墙板 |
| `floor_tile` | Cube (flat) | size, thickness | 地板块 |
| `roof_tile` | Cube (angled) | size, angle | 屋顶块 |
| `glass_panel` | Cube (thin) | width, height | 玻璃板 |

### 圆形构件

| 构件 | 组成 | 参数 | 用途 |
|------|------|------|------|
| `wheel_ring` | Cylinder - Cylinder | radius, thickness | 轮圈 |
| `disk` | Cylinder (flat) | radius, thickness | 圆盘 |
| `ring_frame` | Torus | major_r, minor_r | 环形框架 |

**设计原则：** 这一层的目标是**可以拼出任何结构**。

---

## 🪑 Level 2 — 功能单元层 (Functional Components)

由 Level 1 组合而成的**功能部件**。

### 家具部件

| 部件 | 组成 | 依赖 | 用途 |
|------|------|------|------|
| `table_base` | 4x Column | column | 桌腿 |
| `table_top` | Plank | plank | 桌面 |
| `chair_seat` | Plank | plank | 椅座 |
| `chair_back` | Plank | plank | 椅背 |
| `bed_frame` | 4x Beam + 4x Column | beam, column | 床架 |
| `cabinet_box` | 6x Wall Panel | wall_panel | 柜体 |

### 建筑功能件

| 部件 | 组成 | 依赖 | 用途 |
|------|------|------|------|
| `door_panel` | Wall Panel + Handle | wall_panel | 门板 |
| `door_frame` | 4x Beam | beam | 门框 |
| `window_frame` | 4x Beam | beam | 窗框 |
| `window_glass` | Glass Panel | glass_panel | 窗玻璃 |

### 容器部件

| 部件 | 组成 | 依赖 | 用途 |
|------|------|------|------|
| `cup_body` | Cylinder - Cylinder | - | 杯身 |
| `handle` | Torus (partial) | - | 把手 |
| `bowl` | Sphere - Sphere | - | 碗 |
| `bottle_body` | Cylinder + Cone | - | 瓶身 |

**设计原则：** 这一层是**功能单元**，开始有明确用途。

---

## 🏠 Level 3 — 生活物体层 (Living Objects)

由 Level 2 组合而成的**完整物体**。

### 家具成品

| 物体 | 组成 | 依赖 | Blueprint 示例 |
|------|------|------|----------------|
| `table` | table_base + table_top | table_base, table_top | `table_v1.json` |
| `chair` | chair_seat + chair_back + 4x rod | chair_seat, chair_back, rod | `chair_v1.json` |
| `bed` | bed_frame + plank | bed_frame, plank | `bed_v1.json` |
| `wardrobe` | cabinet_box + door_panel | cabinet_box, door_panel | `wardrobe_v1.json` |
| `bookshelf` | 4x column + 5x plank | column, plank | `bookshelf_v1.json` |
| `desk_lamp` | rod + cone + sphere | - | `desk_lamp_v1.json` |

### 建筑部件

| 物体 | 组成 | 依赖 |
|------|------|------|
| `door` | door_frame + door_panel | door_frame, door_panel |
| `window` | window_frame + window_glass | window_frame, window_glass |
| `stair_unit` | Nx plank + 2x beam | plank, beam |
| `balcony` | floor_tile + rail | floor_tile, rod |

### 日常用品

| 物体 | 组成 | 依赖 |
|------|------|------|
| `cup` | cup_body + handle | cup_body, handle |
| `teapot` | cup_body + handle + spout | cup_body, handle |
| `bicycle` | 2x wheel_ring + beam + ... | wheel_ring, beam, rod |

**设计原则：** 这一层开始**像现实世界**。

---

## 🏢 Level 4+ — 空间与场景层

### Level 4 — 空间结构

| 空间 | 组成 | 依赖 |
|------|------|------|
| `room` | 4x wall_panel + floor_tile + door + window | wall_panel, floor_tile, door, window |
| `kitchen` | room + table + chair + cabinet | room, table, chair, cabinet |
| `bathroom` | room + ... | room, ... |
| `bedroom` | room + bed + wardrobe | room, bed, wardrobe |

### Level 5 — 建筑层

| 建筑 | 组成 | 依赖 |
|------|------|------|
| `house` | Nx room + stair | room, stair |
| `apartment_unit` | Nx room | room |
| `warehouse` | large room | room |
| `shop` | room + window | room, window |

### Level 6 — 场景层

| 场景 | 组成 | 依赖 |
|------|------|------|
| `street` | Nx house + road | house |
| `village` | Nx house | house |
| `city_block` | Nx apartment + street | apartment, street |
| `park` | grass + tree + bench | ... |

### Level 7 — 世界层

| 世界 | 组成 |
|------|------|
| `town` | Nx city_block |
| `city` | Nx town |
| `open_world_map` | Nx city + terrain |

---

## 🎯 实施策略

### 🟢 第一阶段 — 最小闭环（能生成一个房间）

**目标：** 只要 `room` 能自动生成，这个系统就**活了**。

#### 需要实现的 Blueprint

**Level 0（已完成）**
- ✅ `cube`
- ✅ `cylinder`
- ✅ `sphere`

**Level 1**
- [ ] `wall_panel`
- [ ] `floor_panel`
- [ ] `column`
- [ ] `plank`
- [ ] `beam`

**Level 2**
- [ ] `door_panel`
- [ ] `window_frame`
- [ ] `table_base`
- [ ] `table_top`
- [ ] `chair_seat`

**Level 3**
- [ ] `table`
- [ ] `chair`
- [ ] `door`
- [ ] `window`

**Level 4**
- [ ] `room`

#### 时间规划

**第一周**
- cube, cylinder （✅ 已完成）
- subtract, union （✅ 已完成）
- wall_panel, plank, column
- simple table, simple chair

**第二周**
- door, window
- bed, cabinet
- cup

**第三周**
- room
- stair, balcony
- house

---

### 🟡 第二阶段 — 丰富生活物体

加入：
- cup, teapot
- lamp, bookshelf
- bed

---

### 🔵 第三阶段 — 复杂组合

加入：
- bicycle
- car
- cabinet with drawers
- sofa
- kitchen_set

---

## 🧠 核心设计智慧

### ❌ 错误思路

```
直接做：茶杯 → 沙发 → 自行车
```

茶杯太具体，无法复用。

### ✅ 正确思路

```
cylinder → cup_body → cup
```

茶杯只是**组合结果**。

### 成长路径

```
Primitive (几何原子)
  ↓
Panel / Beam / Rod (结构构件)
  ↓
Frame / Container / Support (功能单元)
  ↓
Furniture (生活物体)
  ↓
Room (空间)
  ↓
House (建筑)
  ↓
World (世界)
```

---

## 🔥 关键洞察

### 你不是在建模，而是在定义**组合语言**

当你能组合出：
- 柱子（Column）
- 梁（Beam）
- 板（Plank）

你就可以生成：
- **任何家具**
- **任何建筑**
- **任何机械结构**

---

## 📊 依赖关系图示例

```
Level 0: cube, cylinder, sphere
           ↓
Level 1: wall_panel ← cube
         column ← cylinder
         plank ← cube
           ↓
Level 2: table_base ← column (x4)
         table_top ← plank
         door_frame ← plank (x4)
           ↓
Level 3: table ← table_base + table_top
         door ← door_frame + door_panel
           ↓
Level 4: room ← wall_panel (x4) + floor_panel + door + window
```

---

## 📝 Blueprint 命名规范

### 文件组织

```
assets/csg/
├── primitives/          # Level 0（通常由引擎提供，不需要 JSON）
├── components/          # Level 1-2
│   ├── wall_panel_v1.json
│   ├── column_v1.json
│   ├── table_base_v1.json
│   └── ...
├── objects/             # Level 3
│   ├── table_v1.json
│   ├── chair_v1.json
│   ├── bicycle_v1.json
│   └── ...
├── spaces/              # Level 4
│   ├── room_v1.json
│   ├── kitchen_v1.json
│   └── ...
└── scenes/              # Level 5+
    ├── house_v1.json
    └── ...
```

### 命名约定

- 使用小写 + 下划线：`table_base_v1`
- 版本号后缀：`_v1`, `_v2`
- 参数化变体：`table_base_wood_v1`, `table_base_metal_v1`

---

## 🚀 下一步行动

1. **完成 Level 1 基础构件**（wall_panel, column, plank）
2. **实现 index.json 加载系统**
3. **测试 Reference 递归组合**（wheel → bicycle）
4. **创建第一个 room Blueprint**
5. **实现参数化变体系统**

---

## 🔗 相关文档

- **系统设计：** [csg-composable-object-system.md](./csg-composable-object-system.md)
- **UI 设计：** [csg-ui-design.md](./csg-ui-design.md)
- **M1 实施记录：** [implementation-log-m1.md](./implementation-log-m1.md) *(待创建)*

---

**文档版本：** 1.0  
**最后更新：** 2026-02-21  
**维护者：** Moon Engine Team
