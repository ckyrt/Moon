# CSG 坐标系统规范

> **Coordinate System Specification for CSG Blueprint System**

---

## ⚡ 关键规则 (Quick Reference)

> **完整规则和示例请查看：[assets/csg/README.md](../../assets/csg/README.md)**

### 核心原则

1. **所有基础几何体以几何中心为原点** - Box, Cylinder, Sphere, Cone 返回的mesh都居中在 (0,0,0)
2. **Transform.position 指向几何中心的世界位置** - 不是底部位置！
3. **Y=0 是地面基准** - 着地物体的 position.y = height / 2
4. **Y是高度轴** - 不是Z！(Y-Up左手坐标系)
5. **单位：厘米(cm)** - 所有JSON中的尺寸使用厘米

---

## 📐 坐标系统定义

### 引擎坐标系：**左手Y-Up坐标系**

```
        +Y (Up)
         |
         |  / +Z (Forward)
         | /
         |/_____ +X (Right)
```

**轴定义：**
- **+X** = 右 (Right)
- **+Y** = 上 (Up) - **高度方向**
- **+Z** = 前 (Forward)

**原点定义：**
- **Y = 0** 为地面/基准高度
- 所有几何体的position表示其**几何中心**的世界位置

---

## 📦 基础几何体

| 几何体 | JSON参数 | Local中心 | 返回边界示例 |
|--------|---------|----------|------------|
| **Box** | `size_x, size_y, size_z` | `(0,0,0)` | Box(1,2,3): X[-0.5,0.5], Y[-1.0,1.0], Z[-1.5,1.5] |
| **Cylinder** | `radius, height` | `(0,0,0)` | Cyl(r=0.5,h=2): X[-0.5,0.5], Y[-1.0,1.0], Z[-0.5,0.5] |
| **Sphere** | `radius` | `(0,0,0)` | Sphere(1): X/Y/Z[-1.0,1.0] |
| **Cone** | `radius, height` | `(0,0,0)` | Cone(r=0.5,h=2): X[-0.5,0.5], Y[-1.0,1.0], Z[-0.5,0.5] |

**注意：** Cylinder和Cone的height沿**Y轴**向上延伸（不是Z轴）

---

## 📍 Transform 参数

```json
"transform": {
  "position": [x, y, z],                      // 单位: cm, 几何中心的世界位置
  "rotation_euler": [pitch, yaw, roll],       // 单位: 度, 顺序: X→Y→Z
  "scale": [x_scale, y_scale, z_scale]        // 默认: [1,1,1]
}
```

---

## 🎯 实际应用

**参考示例：** 查看 [assets/csg/components/](../../assets/csg/components/) 中的实际JSON文件

- **table_v1.json** - 完整桌子示例（腿+桌面组合）
- **chair_v1.json** - 完整椅子示例（腿+座椅+靠背堆叠）
- **table_leg_v1.json** - 简单圆柱组件
- **beam_v1.json** - 参数化长方体

---

## 🔧 内部实现 (开发者参考)

### Manifold坐标系转换

Manifold库使用Z-Up坐标系，引擎内部自动转换为Y-Up：

```cpp
// CSGOperations.cpp 内部转换
// Manifold (x,y,z) → Engine (x,z,-y)
Vector3 ManifoldToEngine(x, y, z) {
    return Vector3(x, z, -y);
}
```

**关键实现细节：**
1. `CreateCSGBox/Cylinder/Cone/Sphere` 在Manifold空间创建几何体
2. 使用 `Translate` 和 `Rotate` 在Manifold空间居中几何体
3. `ManifoldToMesh` 转换到引擎Y-Up坐标
4. 返回的mesh已经居中在原点，position通过 `CSGBuilder` 的 `worldTransform` 应用

**Blueprint作者无需关心这些细节** - 只需知道返回的mesh已经居中即可。

---

## 📚 参考文档

- **[assets/csg/README.md](../../assets/csg/README.md)** - 完整规则和实用示例 ⭐ 主要参考
- [CSG 可组合物体系统](./csg-composable-object-system.md) - 蓝图系统架构
- [ADR-0005: 坐标系统与矩阵约定](../adr-0005-coordinate-system-and-matrix-conventions.md) - 技术规范

