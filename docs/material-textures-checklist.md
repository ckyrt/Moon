# Material Textures Download Checklist

## Overview
Moon 引擎支持 **25 种 PBR 材质**，用于别墅级建筑场景渲染。

当前已有贴图：6 种  
需要下载贴图：**18 种**

---

## ✅ 已有贴图（6种）
位置：`assets/textures/materials/`

1. ✅ **Concrete044D_2K-PNG** - 混凝土（concrete）
2. ✅ **Fabric061_2K-PNG** - 布料（fabric）
3. ✅ **Metal061B_2K-PNG** - 金属（metal）
4. ✅ **Plastic018A_2K-PNG** - 塑料（plastic）
5. ✅ **Rock030_2K-PNG** - 岩石（rock）
6. ✅ **Wood049_2K-PNG** - 木材（wood）

---

## 📥 需要下载的贴图（18种）

### 混凝土系列（2种）
- [ ] **ConcreteFloor_2K-PNG** - 水泥地坪（concrete_floor）
- [ ] **ConcretePolished_2K-PNG** - 抛光混凝土（concrete_polished）

### 岩石/砖石系列（4种）
- [ ] **Brick_2K-PNG** - 红砖墙（brick）
- [ ] **Stone_2K-PNG** - 石材外立面（stone）
- [ ] **Plaster_2K-PNG** - 室内石膏墙（plaster）
- [ ] **TileCeramic_2K-PNG** - 瓷砖（tile_ceramic）

### 木材系列（3种）
- [ ] **WoodFloor_2K-PNG** - 木地板（wood_floor）
- [ ] **WoodPolished_2K-PNG** - 抛光木（wood_polished）
- [ ] **WoodPainted_2K-PNG** - 油漆木（wood_painted）

### 金属系列（4种）
- [ ] **Steel_2K-PNG** - 不锈钢（steel）
- [ ] **Aluminum_2K-PNG** - 铝合金（aluminum）
- [ ] **Chrome_2K-PNG** - 镀铬（chrome）
- [ ] **Copper_2K-PNG** - 铜/黄铜（copper）

### 玻璃系列（0种）
- ✅ Glass - 程序化，无需贴图
- ✅ GlassFrosted - 程序化，无需贴图
- ✅ GlassTinted - 程序化，无需贴图

### 软装系列（2种）
- [ ] **Leather_2K-PNG** - 皮革（leather）
- [ ] **Carpet_2K-PNG** - 地毯（carpet）

### 塑料/橡胶系列（1种）
- [ ] **Rubber_2K-PNG** - 橡胶（rubber）

---

## 📋 每个贴图包需要包含的文件

PBR 标准工作流（Metallic/Roughness），每个材质需要以下贴图：

```
<MaterialName>_2K-PNG/
├── <MaterialName>_2K-PNG_Color.png              # Albedo/Base Color
├── <MaterialName>_2K-PNG_NormalDX.png          # Normal Map (DirectX)
├── <MaterialName>_2K-PNG_AmbientOcclusion.png  # AO (可选)
├── <MaterialName>_2K-PNG_Roughness.png         # Roughness
└── <MaterialName>_2K-PNG_Metalness.png         # Metalness (金属材质必需)
```

---

## 🔗 推荐贴图资源网站

### 免费资源
1. **Poly Haven** (CC0)  
   https://polyhaven.com/textures  
   高质量免费 PBR 贴图，建议首选。

2. **Texture.com** (部分免费)  
   https://www.textures.com/  
   需要注册，每日免费下载限额。

3. **AmbientCG** (CC0)  
   https://ambientcg.com/  
   公共域贴图，可商用。

### 付费资源（质量更高）
1. **Megascans** (Quixel)  
   https://quixel.com/megascans  
   Epic Games 旗下，UE5 用户免费。

2. **Poliigon**  
   https://www.poliigon.com/  
   高质量建筑贴图，推荐别墅场景。

---

## 📂 安装位置

下载后放置到以下目录：

```
Moon/assets/textures/materials/
├── ConcreteFloor_2K-PNG/
├── Brick_2K-PNG/
├── Stone_2K-PNG/
├── Plaster_2K-PNG/
├── TileCeramic_2K-PNG/
├── WoodFloor_2K-PNG/
├── WoodPolished_2K-PNG/
├── WoodPainted_2K-PNG/
├── Steel_2K-PNG/
├── Aluminum_2K-PNG/
├── Chrome_2K-PNG/
├── Copper_2K-PNG/
├── Leather_2K-PNG/
├── Carpet_2K-PNG/
├── ConcretePolished_2K-PNG/
└── Rubber_2K-PNG/
```

---

## ⚙️ 贴图规格要求

- **分辨率**: 2K (2048x2048) 推荐，4K 更佳
- **格式**: PNG（支持透明通道）或 JPG
- **法线贴图**: DirectX 格式（Y+）
- **工作流**: Metallic/Roughness（不要用 Specular/Glossiness）
- **色彩空间**: 
  - Albedo: sRGB
  - Normal/Roughness/Metalness/AO: Linear

---

## ✅ 下载完成后

1. 确认所有文件夹结构正确
2. 检查文件命名一致性
3. 重新编译引擎：
   ```powershell
   & "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" Moon.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
   ```
4. 运行测试场景验证材质加载

---

## 📌 注意事项

1. **优先级**: 先下载常用材质（concrete_floor, brick, plaster, steel, wood_floor）
2. **尺寸**: 2K 贴图足够游戏场景使用，4K 可用于建筑可视化
3. **License**: 确认贴图使用许可（商用/非商用）
4. **备份**: 下载后建议备份到云盘

---

## 📊 进度追踪

- **已完成**: 6 / 24 材质（25%）
- **待下载**: 18 / 24 材质（75%）
- **预计用时**: ~2小时（含挑选和下载）

---

最后更新：2026-03-01
