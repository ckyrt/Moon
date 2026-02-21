# CSG 可组合物体系统设计文档

> **Composable CSG Object System Design**  
> *纯数据驱动的几何体组合与复用系统*

---

## 📋 目录

- [1. 目标与范围](#1-目标与范围)
- [2. 核心概念](#2-核心概念)
- [3. 数据结构设计](#3-数据结构设计)
- [4. 运行时架构](#4-运行时架构)
- [5. 构建流程](#5-构建流程)
- [6. 性能优化](#6-性能优化)
- [7. 索引系统](#7-索引系统)
- [8. 示例](#8-示例)
- [9. 技能系统](#9-技能系统)
- [10. 错误处理](#10-错误处理)
- [11. 版本管理](#11-版本管理)
- [12. 开发路线图](#12-开发路线图)

---

## 1. 目标与范围

### 1.1 核心目标

构建一个**纯数据驱动**的几何体合成系统，实现：

✅ **基础组合** - 使用基础几何体 + CSG 运算（Union/Subtract/Intersect）组合生成新几何体  
✅ **递归复用** - 生成的物体可作为可复用部件（Blueprint），被更高层物体引用  
✅ **数据驱动** - 通过 JSON 文件描述组合方式，引擎根据索引加载并生成网格  
✅ **参数化** - 支持尺寸、厚度、半径等参数，同一 Blueprint 可实例化为不同规格  
✅ **高性能** - 支持缓存与增量构建，避免重复的昂贵布尔运算

### 1.2 非目标（第一阶段）

❌ CAD 级精度的布尔运算（满足游戏/UGC 级别即可）  
❌ 任意多边形网格输入的布尔运算  
❌ 分布式/云端构建

---

## 2. 核心概念

### 2.1 Node（节点）

构建树/图的最小单元，支持四种类型：

| 类型 | 说明 | 用途 |
|------|------|------|
| **primitive** | 基础几何体 | Cube, Sphere, Cylinder, Capsule, Cone, Torus |
| **csg** | 布尔运算节点 | Union, Subtract, Intersect |
| **group** | 层级组节点 | 多个子节点并列输出 |
| **reference** | 引用其他 Blueprint | 递归组合的关键 |

### 2.2 Blueprint（蓝图）

可复用的"物体配方"，包含：

- **id** - 唯一标识符
- **parameters** - 参数默认值（可选）
- **root** - Node 树定义

### 2.3 Instance（实例）

Blueprint 的具体使用，包含：

- **ref** - 引用的 Blueprint ID
- **transform** - 位置/旋转/缩放
- **overrides** - 参数覆盖值

---

## 3. 数据结构设计

### 3.1 资源目录结构

```
/assets/csg/
  ├── index.json              # 全局索引（必需）
  ├── primitives.json         # 基础几何体配置（可选）
  ├── components/             # 可复用组件
  │   ├── wheel_v1.json
  │   └── frame_tube_v1.json
  ├── objects/                # 完整物体
  │   ├── bike_v1.json
  │   └── desk_lamp_v1.json
  └── skills/                 # 参数化配方模板
      └── make_table_v1.json
```

### 3.2 Blueprint JSON 格式

```json
{
  "schema_version": 1,
  "id": "bike_v1",
  "category": "vehicle",
  "tags": ["bike", "vehicle", "demo"],
  "parameters": {
    "wheel_radius": {
      "type": "float",
      "default": 0.5,
      "min": 0.1,
      "max": 2.0
    },
    "tube_radius": {
      "type": "float",
      "default": 0.05,
      "min": 0.01,
      "max": 0.2
    }
  },
  "root": { /* Node 定义 */ }
}
```

### 3.3 Transform 统一格式

```json
{
  "transform": {
    "position": [0, 0, 0],
    "rotation_euler": [0, 0, 0],
    "scale": [1, 1, 1]
  }
}
```

### 3.4 Node 类型详解

#### 3.4.1 Primitive Node

```json
{
  "type": "primitive",
  "primitive": "cylinder",
  "params": {
    "radius": "$tube_radius",
    "height": 2.0
  },
  "transform": {
    "position": [0, 1, 0],
    "rotation_euler": [0, 0, 90]
  },
  "material": "metal_rough"
}
```

**标准参数命名：**
- `cube`: `size` (正方体) 或 `size_x`, `size_y`, `size_z` (长方体)
- `sphere`: `radius`
- `cylinder`: `radius`, `height`
- `capsule`: `radius`, `height`

#### 3.4.2 CSG Node

```json
{
  "type": "csg",
  "operation": "subtract",
  "options": {
    "solver": "fast",
    "weld_epsilon": 1e-5,
    "recompute_normals": true
  },
  "left": { /* Node */ },
  "right": { /* Node */ }
}
```

**运算类型：**
- `union` - 并集
- `subtract` - 差集
- `intersect` - 交集

#### 3.4.3 Group Node

```json
{
  "type": "group",
  "children": [
    { /* Node */ },
    { /* Node */ }
  ],
  "output": {
    "mode": "separate"
  }
}
```

**输出模式：**
- `separate` - 多网格输出（推荐）
- `merge` - 合并为单网格（后续实现）

#### 3.4.4 Reference Node

```json
{
  "type": "reference",
  "ref": "wheel_v1",
  "transform": {
    "position": [-1, 0, 0]
  },
  "overrides": {
    "wheel_radius": 0.45
  }
}
```

### 3.5 参数表达式

**第一版支持：**
- 数字常量: `0.5`
- 变量引用: `"$wheel_radius"`

**后续扩展：**
- 表达式计算: `"2 * $wheel_radius"`

---

## 4. 运行时架构

### 4.1 核心模块

```
┌─────────────────────────────────────────┐
│        Blueprint Database               │
│  • 加载/缓存所有 Blueprint JSON         │
│  • 提供 GetBlueprint(id)                │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│      Node Graph Compiler                │
│  • JSON → AST(Node) → Runtime Node      │
│  • 参数替换、transform 合并、校验       │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│          CSG Builder                    │
│  • 递归构建 mesh                        │
│  • 执行布尔运算                         │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│          Mesh Cache                     │
│  • 基于 hash 的缓存                     │
│  • 内存缓存 + 磁盘缓存                  │
└─────────────────────────────────────────┘
                  ↓
┌─────────────────────────────────────────┐
│      Renderer Integration               │
│  • 输出 mesh handle + material          │
│  • 支持 instancing                      │
└─────────────────────────────────────────┘
```

### 4.2 Runtime 数据结构

```cpp
enum class NodeType { Primitive, Csg, Group, Reference };

struct TransformTRS {
    Vec3 position;
    Quat rotation;
    Vec3 scale;
};

struct ValueExpr {
    enum Kind { Constant, ParamRef } kind;
    float constant;
    std::string paramName;
};

struct PrimitiveNode {
    PrimitiveType primitive;
    std::unordered_map<std::string, ValueExpr> params;
    TransformTRS local;
    MaterialId material;
};

struct CsgNode {
    CsgOp operation;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
    CsgOptions options;
};

struct GroupNode {
    std::vector<std::unique_ptr<Node>> children;
    GroupOutputMode mode;
};

struct RefNode {
    std::string refId;
    TransformTRS local;
    std::unordered_map<std::string, ValueExpr> overrides;
};

struct Node {
    NodeType type;
    std::variant<PrimitiveNode, CsgNode, GroupNode, RefNode> data;
};
```

---

## 5. 构建流程

### 5.1 总体流程

```
输入: object_id + instance_overrides + instance_transform
  ↓
LoadBlueprint(object_id)
  ↓
CompileNodeGraph(root, parameter_scope)
  ↓
BuildNode(root)
  ↓
输出: 一组 RenderItem (mesh/material/transform)
```

### 5.2 参数作用域规则

**规则：**
1. Blueprint 自身的 `parameters` 提供默认值
2. Instance `overrides` 覆盖默认值
3. 进入 `reference` 时创建新作用域 = parent scope + reference overrides
4. 变量 `$name` 按最近作用域查找

**示例：**
```
bike_v1 (wheel_radius=0.5)
  └─ reference wheel_v1 (overrides: wheel_radius=0.45)
      └─ primitive cylinder (radius=$wheel_radius)
          ↓
          实际使用 0.45 (reference 的 override)
```

### 5.3 BuildNode 输出设计

```cpp
struct BuiltResult {
    std::vector<MeshItem> meshes;
};

struct MeshItem {
    MeshHandle mesh;
    MaterialId material;
    Mat4 world;  // 或 TRS
};
```

**Node 到 BuiltResult 映射：**
- `primitive` → 1 个 MeshItem
- `csg` → 左右各 build → 布尔运算 → 1 个 MeshItem
- `group` → 拼接所有 children 的 meshes
- `reference` → 加载子 Blueprint → build → 应用 transform

⚠️ **第一版限制：** CSG 的左右节点必须各输出 1 个 mesh

---

## 6. 性能优化

### 6.1 CSG 生成时机

**推荐方案：Build-time Mesh + Cache**

- ✅ 载入/编辑时执行 CSG
- ✅ 运行时只渲染生成的 mesh
- ✅ 适合 UGC 平台，可缓存

**不推荐：Runtime CSG**
- ❌ 每次实例化都做布尔运算（性能差）

### 6.2 Mesh Cache 策略

**缓存 Key 组成：**
```cpp
cacheKey = Hash(
    blueprintId +
    blueprintVersion +
    sortedParams +
    buildOptions
)
```

**两级缓存：**
1. **内存缓存** - 本次运行复用
2. **磁盘缓存** - 跨运行复用（强烈推荐）

**缓存内容：**
- 生成的 mesh 数据（顶点/索引）
- 或 mesh asset 路径

### 6.3 增量更新（编辑器优化）

编辑器修改节点时：
1. 计算受影响 subtree 的 hash
2. 只重建变化的子树
3. 其他节点从 cache 命中

---

## 7. 索引系统

### 7.1 index.json 的作用

当物体数量上千时，不能靠扫描目录来查找 ID。

**功能：**
- `id → filepath` 映射
- tags/category 搜索
- 版本/依赖信息

**格式：**
```json
{
  "schema_version": 1,
  "items": [
    {
      "id": "wheel_v1",
      "path": "components/wheel_v1.json",
      "tags": ["wheel", "component"]
    },
    {
      "id": "bike_v1",
      "path": "objects/bike_v1.json",
      "tags": ["bike", "vehicle"]
    }
  ]
}
```

**使用方式：**
```cpp
// 启动时加载
std::unordered_map<std::string, std::string> blueprintIndex;
LoadIndex("assets/csg/index.json", blueprintIndex);

// 快速查找
std::string path = blueprintIndex["bike_v1"];
```

---

## 8. 示例

### 8.1 示例1：Cube - Sphere

```json
{
  "schema_version": 1,
  "id": "cube_minus_sphere_v1",
  "root": {
    "type": "csg",
    "operation": "subtract",
    "left": {
      "type": "primitive",
      "primitive": "cube",
      "params": { "size": 2.0 }
    },
    "right": {
      "type": "primitive",
      "primitive": "sphere",
      "params": { "radius": 1.2 }
    }
  }
}
```

### 8.2 示例2：轮子（wheel_v1）

```json
{
  "schema_version": 1,
  "id": "wheel_v1",
  "parameters": {
    "radius": { "type": "float", "default": 0.5 },
    "thickness": { "type": "float", "default": 0.2 }
  },
  "root": {
    "type": "csg",
    "operation": "subtract",
    "left": {
      "type": "primitive",
      "primitive": "cylinder",
      "params": {
        "radius": "$radius",
        "height": "$thickness"
      },
      "transform": {
        "rotation_euler": [90, 0, 0]
      }
    },
    "right": {
      "type": "primitive",
      "primitive": "cylinder",
      "params": {
        "radius": "0.8 * $radius",
        "height": "$thickness"
      },
      "transform": {
        "rotation_euler": [90, 0, 0]
      }
    }
  }
}
```

### 8.3 示例3：自行车（bike_v1）

```json
{
  "schema_version": 1,
  "id": "bike_v1",
  "parameters": {
    "wheel_radius": { "type": "float", "default": 0.5 }
  },
  "root": {
    "type": "group",
    "children": [
      {
        "type": "reference",
        "ref": "wheel_v1",
        "transform": { "position": [-1, 0, 0] },
        "overrides": { "radius": "$wheel_radius" }
      },
      {
        "type": "reference",
        "ref": "wheel_v1",
        "transform": { "position": [1, 0, 0] },
        "overrides": { "radius": "$wheel_radius" }
      },
      {
        "type": "primitive",
        "primitive": "cylinder",
        "params": {
          "radius": 0.05,
          "height": 2.0
        },
        "transform": {
          "position": [0, 0.5, 0],
          "rotation_euler": [0, 0, 90]
        },
        "material": "metal"
      }
    ],
    "output": { "mode": "separate" }
  }
}
```

---

## 9. 技能系统

### 9.1 Skill 的意义

Skill 是"可参数化生成 Blueprint"的高层抽象：

- ✅ AI 生成更稳定
- ✅ 用户输入更友好（如"生成一张桌子：长宽高"）

### 9.2 Skill 文件结构

```json
{
  "schema_version": 1,
  "id": "make_table_v1",
  "inputs": {
    "length": {
      "type": "float",
      "default": 1.2,
      "min": 0.5,
      "max": 5.0
    },
    "width": {
      "type": "float",
      "default": 0.6,
      "min": 0.3,
      "max": 3.0
    },
    "height": {
      "type": "float",
      "default": 0.75,
      "min": 0.3,
      "max": 2.0
    }
  },
  "template": {
    "id": "table_${length}_${width}_${height}",
    "category": "furniture",
    "root": { /* Node 定义 */ }
  }
}
```

### 9.3 运行流程

```
Skill 输入参数
  ↓
生成临时 Blueprint (或直接生成 Node tree)
  ↓
构建 mesh
  ↓
缓存结果
```

---

## 10. 错误处理

### 10.1 JSON 校验（加载阶段）

**必须报错的情况：**
- ❌ Blueprint ID 重复
- ❌ Reference 找不到引用的 Blueprint
- ❌ Primitive 参数缺失（如 cylinder 没 radius/height）
- ❌ CSG left/right 输出多个 mesh（第一版限制）
- ❌ 参数引用 `$xxx` 未定义

### 10.2 错误输出格式

```json
{
  "blueprint_id": "bike_v1",
  "node_path": "root.left.right",
  "error": "Parameter $unknown_param not defined",
  "severity": "error"
}
```

---

## 11. 版本管理

### 11.1 schema_version

所有 JSON 文件必须包含 `schema_version`：

- **v1** - 仅支持 `$param` 引用，不支持表达式
- **v2** - 支持表达式、group merge、多 mesh CSG

### 11.2 兼容策略

**方案1：读取时自动升级**
```cpp
if (schema_version == 1) {
    UpgradeToV2(blueprint);
}
```

**方案2：离线升级工具**
```bash
./upgrade_blueprints --from=1 --to=2 assets/csg/
```

---

## 12. 开发路线图

### 🎯 M1：最小闭环（TestCSG 立刻做）

- [ ] Node JSON 解析（primitive + csg）
- [ ] 参数系统（常量 + `$param`）
- [ ] BuildNode 输出单 mesh
- [ ] 支持 cube-sphere demo

### 🎯 M2：递归复用

- [ ] Reference 节点实现
- [ ] index.json 读取与 id→path 映射
- [ ] wheel_v1 + bike_v1 能生成并渲染

### 🎯 M3：性能优化

- [ ] 内存 cache（hash key）
- [ ] 磁盘 cache（写出 mesh）
- [ ] 编辑器增量 rebuild（hash subtree）

### 🎯 M4：技能系统

- [ ] Skill template 支持
- [ ] 生成临时 blueprint 并构建

---

## 📌 关键设计决策

| 决策点 | 选择 | 原因 |
|--------|------|------|
| **Node 类型** | 固定四类 | 简化设计，降低复杂度 |
| **核心机制** | 递归引用 + 参数作用域 | 系统灵魂，实现复用 |
| **CSG 限制** | 左右必须各输出 1 mesh | 第一版降低复杂度 |
| **构建时机** | Build-time CSG + Cache | 避免运行时布尔运算 |
| **索引方式** | index.json | 规模化 ID 查找 |

---

## 🔗 相关文档

- [CSG 单元测试设计](./csg-unit-test.md)
- [引擎架构文档](./ARCHITECTURE.md)
- [开发者指南](./DEVELOPER_GUIDE.md)

---

**文档版本：** 1.0  
**最后更新：** 2026-02-21  
**维护者：** Moon Engine Team
