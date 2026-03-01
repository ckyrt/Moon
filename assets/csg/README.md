# CSG 蓝图系统 - 完整规则手册

> **🚨 AI生成CSG蓝图必须遵守的规则。index.json 只含路径索引，所有规则在此文件。**

---

## 0️⃣ 支持的节点类型与基础几何体

| 节点类型 | 说明 |
|----------|------|
| `primitive` | 基础几何体，见下表 |
| `csg` | 布尔运算：`union` / `subtract` / `intersect` |
| `group` | 并列输出多个子节点，`output.mode: separate` |
| `reference` | 引用其他蓝图，支持 `overrides` 覆盖参数 |

| 几何体 | 参数 | 备注 |
|--------|------|------|
| `cube` | `size_x, size_y, size_z` 或 `size` | Y轴为高度 |
| `cylinder` | `radius, height` | 默认沿Y轴竖立 |
| `sphere` | `radius` | — |
| `cone` | `radius, height` | 尖端朝+Y |

> ❌ `capsule` 和 `torus` 尚未实现，使用会报错。

**支持的材质（material 字段）：**

| 材质名称 | 别名 | 说明 | 视觉效果 |
|---------|------|------|---------|
| `wood` | — | 木材（默认） | 棕色木纹，中等粗糙 |
| `metal` | — | 金属 | 灰色金属，光滑反光 |
| `glass` | — | 玻璃 | 透明，高反射 |
| `concrete` | `rock`, `stone` | 混凝土/石材 | 灰色粗糙表面 |
| `plastic` | `emissive` | 塑料 | 光滑彩色表面 |
| `fabric` | `cloth` | 布料 | 柔软纹理，低反射 |

**示例：**
```json
{
  "type": "primitive",
  "primitive": "cube",
  "params": {"size_x": 100, "size_y": 50, "size_z": 80},
  "material": "wood"
}
```

> **注意：** 如果不指定 `material` 字段，默认使用 `wood` 材质。所有材质名称使用**小写**。

---

## 1️⃣ 所有基础几何体以几何中心为原点

| 几何体 | 参数 | Local坐标中心 |
|--------|------|--------------|
| `box` | `size_x, size_y, size_z` | 几何中心 `(0,0,0)` |
| `cylinder` | `radius, height` | 高度中点 `(0,0,0)` |
| `sphere` | `radius` | 球心 `(0,0,0)` |
| `cone` | `radius, height` | 高度中点 `(0,0,0)` |

**示例边界：**
- `Box(width=1, height=2, depth=3)` → Local bounds: X[-0.5,0.5], Y[-1.0,1.0], Z[-1.5,1.5]
- `Cylinder(radius=0.5, height=2.0)` → Local bounds: X[-0.5,0.5], Y[-1.0,1.0], Z[-0.5,0.5]

---

## 2️⃣ 坐标系：Y-Up 左手坐标系

```
        +Y (Up)
         |
         |  / +Z (Forward)
         | /
         |/_____ +X (Right)
```

- **+Y = 上** (高度方向) ⬅️ 注意：Y是高度，不是Z
- **+X = 右**
- **+Z = 前**
- **Y=0 = 地面/基准面**

---

## 3️⃣ Transform.position 指向几何中心的世界位置

```json
{
  "primitive": "cylinder",
  "params": {"height": 45},
  "transform": {
    "position": [0, 22.5, 0]  // 中心在22.5cm高度
    // ✅ 实际世界范围: Y[0, 45]cm (底部着地)
    // ❌ 不是底部位置！
  }
}
```

**关键公式：**
- 物体底部着地 → `position.y = height / 2`
- 物体顶部在某高度 → `position.y = target_height - height / 2`

---

## 4️⃣ 组件堆叠计算

**规则：** 每个组件的position.y = 前面组件的总高度 + 当前组件高度的一半

```json
{
  "parameters": {
    "leg_height": 45,
    "seat_thickness": 3,
    "backrest_height": 40
  },
  "children": [
    {
      "comment": "腿 - 底部着地",
      "params": {"height": "$leg_height"},
      "transform": {
        "position": [0, "$leg_height / 2", 0]  // 22.5
      }
    },
    {
      "comment": "座椅 - 紧贴腿顶部",
      "params": {"size_y": "$seat_thickness"},
      "transform": {
        "position": [0, "$leg_height + $seat_thickness / 2", 0]  // 46.5
      }
    },
    {
      "comment": "靠背 - 从座椅顶部延伸",
      "params": {"size_y": "$backrest_height"},
      "transform": {
        "position": [
          0,
          "$leg_height + $seat_thickness + $backrest_height / 2",  // 68
          "-$seat_depth / 2 + $backrest_thickness / 2"
        ]
      }
    }
  ]
}
```

---

## 5️⃣ 单位和格式

**单位：** 所有尺寸使用**厘米(cm)**

**Transform格式：**
```json
"transform": {
  "position": [x, y, z],                      // 厘米, 几何中心的世界位置
  "rotation_euler": [pitch, yaw, roll],       // 度, pitch=X轴, yaw=Y轴, roll=Z轴
  "scale": [x_scale, y_scale, z_scale]        // 默认[1,1,1]
}
```

**⚠️ rotation_euler 轴映射（和直觉不同，必须记住）：**

| 分量 | 变量 | 实际旋转轴 |
|------|------|----------|
| `[x, 0, 0]` | pitch | **Y 轴**旋转 |
| `[0, y, 0]` | yaw | **Z 轴**旋转 |
| `[0, 0, z]` | roll | **X 轴**旋转 |

**常用值（cylinder 默认沿 Y 轴竖直）：**
```json
// 横向圆柱 → 沿 +X 轴（横梁/台灯臂）：yaw=-90 → Z轴旋转 → Y变+X
"rotation_euler": [0, -90, 0]

// 横向圆柱 → 沿 -X 轴：yaw=+90
"rotation_euler": [0, 90, 0]

// 纵深圆柱 → 沿 +Z 轴：roll=90 → X轴旋转 → Y变+Z（左手系）
"rotation_euler": [0, 0, 90]

// 水平朝向（绕 Y 轴，不改变高度）：pitch 控制
"rotation_euler": [45, 0, 0]
```

> **注意：** rotation 在构建时已 bake 进顶点，CSG 布尔运算和 group 组合均正确生效。

---

## ⚠️ 常见错误

### ❌ 错误1：忘记position指向中心
```json
// ❌ 错误
{"position": [0, 0, 0]}  // 圆柱会穿过地面

// ✅ 正确
{"position": [0, "$height / 2", 0]}
```

### ❌ 错误2：混淆轴方向（Y是高度，不是Z）
```json
// ❌ 错误
{"size_x": 120, "size_y": 80, "size_z": 2.5}  // Y应该是小的高度

// ✅ 正确
{"size_x": 120, "size_y": 2.5, "size_z": 80}  // Y是高度
```

### ❌ 错误3：堆叠时忘记加自身高度的一半
```json
// ❌ 错误：座椅会悬浮
{"position": [0, "$leg_height", 0]}

// ✅ 正确
{"position": [0, "$leg_height + $seat_thickness / 2", 0]}
```

---

## 📁 参考示例

查看 `components/` 目录中的实际JSON文件：

- **基础组件：** `table_leg_v1.json`, `table_top_v1.json`
- **组合物体：** `table_v1.json`, `chair_v1.json`
- **参数化组件：** `beam_v1.json`, `rod_v1.json`, `disk_v1.json`

**学习方式：** 直接阅读和复制这些JSON文件的模式，它们都遵循上述规则。
